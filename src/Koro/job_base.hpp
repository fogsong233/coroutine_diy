#pragma once
#include <atomic>
#include <coroutine>
#include <exception>
#include <expected>
#include <functional>
#include <mutex>
#include <vector>
// 可以包含你现有的 cancellation_wrapper, job_state 定义

namespace koro {

struct cancellation_wrapper {
  std::function<void()> do_cancel;
};
enum class job_state : std::uint8_t { RUNNING, CANCELLED, FINISHED };

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
    return !m_coro.done();
  }
  ~job_handle_with_coro() override {
    // 不负责destory
  }

private:
  std::coroutine_handle<> m_coro;
};

template <class VType, class EType> class job;

// 这是新的 Promise 类型基类
// 注意：它需要是模板，因为 m_value 的类型 VType 和 EType 是模板参数
template <class VType, class EType> struct base_job_promise {
  using return_type = std::expected<VType, EType>;

  std::suspend_always initial_suspend() noexcept { return {}; }

  void return_value(return_type value) noexcept { m_value = std::move(value); }
  void return_value(VType value) noexcept {
    m_value = std::expected<VType, EType>(std::move(value));
  }

  void unhandled_exception() noexcept {
    try {
      std::rethrow_exception(std::current_exception());
    } catch (...) {
      m_value = std::unexpected(EType{}); // 假设 EType 有默认构造
      m_job_state = job_state::FINISHED;
    }
  }
  cancellation_wrapper m_cw;
  std::suspend_never await_transform(cancellation_wrapper &&cw) noexcept {
    m_cw = cw;
    return {};
  }
  template <typename Awaitable>
  decltype(auto) await_transform(Awaitable &&a) noexcept {
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
    std::lock_guard<std::recursive_mutex> lock(cancellation_mutex);
    if (m_job_state == job_state::RUNNING) {
      m_job_state = job_state::CANCELLED;
      if (m_cw.do_cancel) {
        m_cw.do_cancel();
      }
      for (const auto &cb : cancel_cb) {
        cb();
      }
    }
  }

  std::suspend_always final_suspend() noexcept { return {}; }

protected:
  std::coroutine_handle<> continuation;
  std::vector<std::function<void(return_type &)>> complete_cb;
  std::vector<std::function<void()>> cancel_cb;
  std::recursive_mutex complete_mutex;
  std::recursive_mutex cancellation_mutex;

  return_type m_value;
  std::atomic<job_state> m_job_state{job_state::RUNNING};
};

} // namespace koro