#ifndef scope_H
#define scope_H

// scope(Dispatcher::IO, [](Context ctx) {
//   auto a = ctx.reg(fn1());
//   auto b = ctx.reg(()[]->job { channel.send(1); });
//   auto c = ctx.rge(()[]->job {
//     while (co_await channel.end()) {
//       auto res co_await channel.receive();
//     }
//   }) co_await a;
//   b.invoke_on_canceelation([] { print(cancel); });
// });

#include "job.hpp"
#include "util.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
namespace koro {
class scope;
struct continuation;
struct custom_awaiter;
class scope_ctx {
public:
  scope_ctx(std::shared_ptr<scope> m_scope) : m_scope(std::move(m_scope)) {}
  template <class VType, class EType,
            template <typename V, typename E> typename JobLike>
    requires job_like<JobLike, VType, EType>
  std::shared_ptr<JobLike<VType, EType>> reg(JobLike<VType, EType> &&new_job);

  void reg(continuation &&cont);

  template <class Rep, class Period>
  custom_awaiter delay(std::chrono::duration<Rep, Period> per);

private:
  std::weak_ptr<scope> m_scope;
};

struct continuation {
  std::shared_ptr<job_handle> m_handle;
  using ready_fn = std::function<bool(job_handle *)>;
  ready_fn m_ready_to_resume;
  continuation(
      std::shared_ptr<job_handle> handle,
      ready_fn fn = [](auto *) { return true; })
      : m_handle(std::move(handle)), m_ready_to_resume(std::move(fn)) {}

  bool ready_to_resume() noexcept {
    try {
      return m_ready_to_resume(m_handle.get());
    } catch (...) {
      return false;
    }
  }
};
using scope_context = std::shared_ptr<scope_ctx>;
class scope : public std::enable_shared_from_this<scope> {
public:
  friend class scope_ctx;
  scope(const scope &) = delete;
  scope &operator=(const scope &) = delete;
  scope &operator=(scope &&) = delete;
  static std::shared_ptr<scope> make() noexcept {
    return std::make_shared<scope>();
  }
  template <typename EType, template <typename V, typename T> class JobLike>
    requires job_like<JobLike, int, EType>
  void run(std::function<JobLike<int, EType>(std::shared_ptr<scope_ctx>)>
               &&coro_block) noexcept {
    auto ctx = std::make_shared<scope_ctx>(shared_from_this());
    ctx->reg(coro_block(ctx));
  }
  void cancel() { m_is_cancel.store(true); }

  void loop() noexcept {
    while (!m_coros.empty()) {
      if (m_is_cancel) {
        break;
      }
      continuation cont{nullptr, {}};
      {
        std::lock_guard<std::recursive_mutex> lock(m_coros_lock);
        cont = std::move(m_coros.front());
        m_coros.pop_front();
      }
      if (cont.ready_to_resume()) {
        if (cont.m_handle->is_active()) {
          cont.m_handle->resume();
        }
      } else {
        std::lock_guard<std::recursive_mutex> lock(m_coros_lock);
        m_coros.push_back(std::move(cont));
      }
    }
  }

private:
  std::deque<continuation> m_coros;
  std::deque<std::shared_ptr<job_handle>> m_saved_coros;
  std::recursive_mutex m_coros_lock;
  std::recursive_mutex m_saved_coros_lock;
  std::atomic_bool m_is_cancel;

public:
  explicit scope() = default;
};
template <class VType, class EType,
          template <typename V, typename E> typename JobLike>
  requires job_like<JobLike, VType, EType>
inline std::shared_ptr<JobLike<VType, EType>>
scope_ctx::reg(JobLike<VType, EType> &&new_job) {
  if (auto m_scope_valid = m_scope.lock()) {
    std::lock_guard<std::recursive_mutex> lock(m_scope_valid->m_coros_lock);
    auto new_j = std::shared_ptr<JobLike<VType, EType>>(
        new JobLike<VType, EType>(std::move(new_job)));
    m_scope_valid->m_coros.push_back(
        continuation(std::shared_ptr<job_handle>(new_j)));
    m_scope_valid->m_saved_coros.push_back(std::shared_ptr<job_handle>(new_j));
    return new_j;
  } else {
    throw "context is alreay invalid";
  }
}

inline void scope_ctx::reg(continuation &&cont) {
  if (auto m_scope_valid = m_scope.lock()) {
    m_scope_valid->m_coros_lock.lock();
    m_scope_valid->m_coros.push_back(std::move(cont));
    m_scope_valid->m_saved_coros.push_back(cont.m_handle);
  } else {
    throw "context is alreay invalid";
  }
}

struct custom_awaiter {
  [[nodiscard]] static constexpr bool await_ready() noexcept { return false; }
  constexpr void await_suspend(std::coroutine_handle<> h) const noexcept {
    suspend_fn(h);
  }
  constexpr void await_resume() const noexcept {}
  std::function<void(std::coroutine_handle<>)> suspend_fn;
};

template <class Rep, class Period>
custom_awaiter scope_ctx::delay(std::chrono::duration<Rep, Period> per) {
  return custom_awaiter{.suspend_fn = [per, this](std::coroutine_handle<> h) {
    continuation cont(
        std::make_unique<job_handle_with_coro>(h),
        [start_time = std::chrono::system_clock::now(), per](job_handle *) {
          auto now_time = std::chrono::system_clock::now();
          auto dura = now_time - start_time;
          return dura > per;
        });
    this->reg(std::move(cont));
  }};
}
} // namespace koro

#endif // scope_H