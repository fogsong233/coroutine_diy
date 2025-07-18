#ifndef manager_job_H
#define manager_job_H

#include "job_base.hpp"
#include "scope.hpp"
#include <coroutine>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
namespace koro {
template <class VType, class EType> class manager_job : public job_handle {
public:
  friend class scope_ctx;
  class promise_type;
  class awaiter;
  friend class promise_type;

  // 移动构造和赋值
  manager_job(manager_job &&other) noexcept
      : m_coro(std::exchange(other.m_coro, nullptr)),
        m_ctx(std::move(other.m_ctx)) {};
  manager_job &operator=(manager_job &&other) noexcept = delete;

  // 禁止拷贝
  manager_job(const manager_job &other) = delete;
  manager_job &operator=(const manager_job &other) = delete;

  awaiter operator co_await() noexcept { return awaiter{m_coro}; };
  awaiter await() noexcept { return awaiter{m_coro}; };

  void invoke_on_completed(
      std::function<void(std::expected<VType, EType> &)> fn) noexcept {
    std::lock_guard<std::recursive_mutex> lock(m_coro.promise().complete_mutex);
    if (m_coro.promise().m_job_state == job_state::FINISHED) {
      // 如果已经完成，直接调用回调
      fn(m_coro.promise().m_value);
    } else {
      // 否则添加到回调列表
      m_coro.promise().complete_cb.push_back(std::move(fn));
    }
  }

  void invoke_on_cancelled(std::function<void()> fn) noexcept {
    std::lock_guard<std::recursive_mutex> lock(
        m_coro.promise().cancellation_mutex);
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

  void cancel() noexcept override {
    {
      std::lock_guard<std::recursive_mutex> lock(m_coro.promise().m_coros_lock);
      for (auto &coro : m_coro.promise().m_coros) {
        // coro cancel
        coro->cancel();
      }
    }
    m_coro.promise().cancel();
  }

  void resume() noexcept override { m_coro.resume(); }

  std::coroutine_handle<> get_coro_handle() const noexcept override {
    return m_coro;
  }

  // 析构函数
  ~manager_job() override {
    if (m_coro) {
      m_coro.destroy();
    }
  }

protected:
  explicit manager_job(std::coroutine_handle<promise_type> h,
                       scope_context ctx) noexcept
      : m_coro(h), m_ctx(ctx) {};
  std::coroutine_handle<promise_type> m_coro;
  scope_context m_ctx;
};

template <class VType, class EType>
class manager_job<VType, EType>::promise_type
    : public base_job_promise<VType, EType> {
private:
  scope_context m_ctx;

public:
  friend class manager_job;
  friend class awaiter;
  template <typename... Args>
  promise_type(scope_context ctx, Args... args)
      : koro::base_job_promise<VType, EType>(), m_ctx(ctx) {}

  manager_job<VType, EType> get_return_object() noexcept {
    return manager_job(std::coroutine_handle<promise_type>::from_promise(*this),
                       m_ctx);
  }

  using base_job_promise<VType, EType>::await_transform;
  std::suspend_never await_transform() { return {}; }
  // 给协程体

  template <typename VType_, typename EType_,
            template <typename V, typename E> typename JobLike>
  std::shared_ptr<JobLike<VType_, EType_>>
  reg(JobLike<VType_, EType_> &&new_job) {
    std::shared_ptr<JobLike<VType_, EType_>> new_j =
        m_ctx->reg(std::move(new_job));
    {
      std::lock_guard<std::recursive_mutex> lock(m_coros_lock);
      m_coros.push_back(new_j);
    }
    return new_j;
  }

  struct final_awaiter {
    bool await_ready() noexcept { return false; }
    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<promise_type> h) noexcept {
      auto &promise = h.promise();
      if (promise.m_job_state == job_state::RUNNING) {
        promise.m_job_state = job_state::FINISHED;
        for (const auto &cb : promise.complete_cb) {
          cb(promise.m_value);
        }
      }
      if (promise.continuation) {
        return promise.continuation;
      }
      return std::noop_coroutine();
    }
    void await_resume() noexcept {}
  };
  final_awaiter final_suspend() noexcept { return {}; }

private:
  std::recursive_mutex m_coros_lock;
  std::deque<std::shared_ptr<job_handle>> m_coros;
};

template <class VType, class EType> class manager_job<VType, EType>::awaiter {
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

protected:
  std::coroutine_handle<promise_type> m_coro;
};
} // namespace koro

#endif // manager_job_H