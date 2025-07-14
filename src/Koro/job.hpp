#ifndef job_H
#define job_H
#include <atomic>
#include <coroutine>
#include <exception>
#include <expected>
#include <functional>
#include <mutex>
#include <type_traits>
#include <vector>

namespace koro {
struct cancellation_wrapper {
  std::function<void()> do_cancel;
};
enum class job_state : std::uint8_t { RUNNING, CANCELLED, FINISHED };
class context;

class job_handle {
public:
  job_handle() = default;
  job_handle(job_handle &&other) = default;
  job_handle(const job_handle &) = delete;
  job_handle &operator=(const job_handle &) = delete;

  [[nodiscard]] virtual bool is_active() const noexcept { return false; };
  [[nodiscard]] virtual bool is_cancelled() const noexcept { return false; };
  [[nodiscard]] virtual bool is_finished() const noexcept { return true; };
  virtual void cancel() noexcept {};
  virtual void resume() noexcept {};
  virtual ~job_handle() noexcept {};
  [[nodiscard]] virtual std::coroutine_handle<>
  get_coro_handle() const noexcept {
    return {};
  };
};

class job_handle_with_coro : public job_handle {
public:
  job_handle_with_coro(std::coroutine_handle<> h) noexcept : m_coro(h) {}
  job_handle_with_coro(const job_handle_with_coro &) = delete;
  job_handle_with_coro &operator=(const job_handle_with_coro &) = delete;
  void resume() noexcept override { m_coro.resume(); }
  [[nodiscard]] bool is_active() const noexcept override {
    return m_coro.done();
  }
  ~job_handle_with_coro() override {
    if (m_coro) {
      m_coro.destroy();
    }
  }

private:
  std::coroutine_handle<> m_coro;
};

template <class VType, class EType> class job : public job_handle {
public:
  friend class context;
  class promise_type;
  class awaiter;

  // 移动构造和赋值
  job(job &&other) noexcept : m_coro(std::exchange(other.m_coro, {})) {};
  job &operator=(job &&other) noexcept = delete;

  // 禁止拷贝
  job(const job &other) = delete;
  job &operator=(const job &other) = delete;

  awaiter operator co_await() && noexcept { return awaiter{m_coro}; };

  void invoke_on_completed(
      std::function<void(std::expected<VType, EType> &)> fn) noexcept {
    std::lock_guard<std::mutex> lock(m_coro.promise().complete_mutex);
    if (m_coro.promise().m_job_state == job_state::FINISHED) {
      // 如果已经完成，直接调用回调
      fn(m_coro.promise().m_value);
    } else {
      // 否则添加到回调列表
      m_coro.promise().complete_cb.push_back(std::move(fn));
    }
  }

  void invoke_on_cancelled(std::function<void()> fn) noexcept {
    std::lock_guard<std::mutex> lock(m_coro.promise().cancellation_mutex);
    if (m_coro.promise().m_job_state == job_state::CANCELLED) {
      // 如果已经取消，直接调用回调
      fn();
    } else {
      // 否则添加到回调列表
      m_coro.promise().cancel_cb.push_back(std::move(fn));
    }
  }

  [[nodiscard]] bool is_active() const noexcept override {
    return m_coro.promise().is_active();
  }

  [[nodiscard]] bool is_cancelled() const noexcept override {
    return m_coro.promise().is_cancelled();
  }

  [[nodiscard]] bool is_finished() const noexcept override {
    return m_coro.promise().is_finished();
  }

  void cancel() noexcept override { m_coro.promise().cancel(); }

  void resume() noexcept override { m_coro.resume(); }

  std::coroutine_handle<> get_coro_handle() const noexcept override {
    return m_coro;
  }

  // 析构函数
  ~job() override {
    if (m_coro) {
      m_coro.destroy();
    }
  }

private:
  explicit job(std::coroutine_handle<promise_type> h) noexcept : m_coro(h) {};
  std::coroutine_handle<promise_type> m_coro;

  friend class promise_type;
};

template <class VType, class EType> class job<VType, EType>::promise_type {
  using return_type = std::expected<VType, EType>;

public:
  job<VType, EType> get_return_object() noexcept {
    return job{std::coroutine_handle<promise_type>::from_promise(*this)};
  }

  std::suspend_always initial_suspend() noexcept { return {}; }

  void return_value(return_type value) noexcept { m_value = std::move(value); }
  void return_value(VType value) noexcept {
    m_value = std::expected(std::move(value));
  }
  void return_void() { static_assert(std::is_same<VType, void>, ); }

  void unhandled_exception() noexcept {
    // 将异常包装到 expected 中而不是直接终止
    try {
      std::rethrow_exception(std::current_exception());
    } catch (...) {
      m_value = std::unexpected(EType{}); // 需要 EType 有默认构造函数
      m_job_state = job_state::FINISHED;
    }
  }

  std::suspend_never await_transform(cancellation_wrapper &&cw) noexcept {
    m_cw = cw;
    return {};
  }
  template <typename Awaitable> auto await_transform(Awaitable &&a) noexcept {
    return std::forward<Awaitable>(a);
  }

  [[nodiscard]] bool is_active() const noexcept {
    return m_job_state == job_state::RUNNING;
  }
  [[nodiscard]] bool is_cancelled() const noexcept {
    return m_job_state == job_state::CANCELLED;
  }
  [[nodiscard]] bool is_finished() const noexcept {
    return m_job_state == job_state::FINISHED;
  }

  void cancel() noexcept {
    std::lock_guard<std::mutex> lock(cancellation_mutex);
    if (m_job_state == job_state::RUNNING) {
      m_job_state = job_state::CANCELLED;
      if (m_cw.do_cancel) {
        m_cw.do_cancel();
      }
      // 执行取消回调
      for (const auto &cb : cancel_cb) {
        cb();
      }
    }
  }

  struct final_awaiter {
    bool await_ready() noexcept { return false; }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<promise_type> h) noexcept {
      if (!h.promise().is_active()) {
        return std::noop_coroutine();
      }
      auto &promise = h.promise();
      promise.m_job_state = job_state::FINISHED;
      // 执行完成回调
      for (const auto &cb : promise.complete_cb) {
        cb(promise.m_value);
      }
      if (promise.continuation) {
        return promise.continuation;
      }
      return std::noop_coroutine();
    }

    void await_resume() noexcept {}
  };

  final_awaiter final_suspend() noexcept { return {}; }

  std::coroutine_handle<> continuation;
  std::vector<std::function<void(return_type &)>> complete_cb;
  std::vector<std::function<void()>> cancel_cb;
  std::mutex complete_mutex;
  std::mutex cancellation_mutex;
  return_type m_value;
  cancellation_wrapper m_cw;
  std::atomic<job_state> m_job_state{job_state::RUNNING};
};

template <class VType, class EType> class job<VType, EType>::awaiter {
public:
  explicit awaiter(std::coroutine_handle<promise_type> coro) noexcept
      : m_coro(coro) {}

  bool await_ready() noexcept {
    // 如果 job 已经完成，则不需要挂起
    return !m_coro.promise().is_active();
  }

  void await_suspend(std::coroutine_handle<> h) noexcept {
    m_coro.promise().continuation = h;
    // 由框架自己调度
  }

  std::expected<VType, EType> await_resume() noexcept {
    return std::move(m_coro.promise().m_value);
  }

private:
  std::coroutine_handle<promise_type> m_coro;
};

} // namespace koro
#endif // job_H