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
#include <atomic>
#include <chrono>
#include <coroutine>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>
namespace koro {
class scope;
struct continuation;
struct custom_awaiter;
class context {
public:
  context(std::shared_ptr<scope> m_scope) : m_scope(std::move(m_scope)) {}
  template <class VType, class EType>
  job<VType, EType> &reg(job<VType, EType> &&new_job) noexcept;
  void reg(continuation &&cont) noexcept;
  template <class Rep, class Period>
  custom_awaiter delay(std::chrono::duration<Rep, Period> per);

private:
  std::shared_ptr<scope> m_scope;
};

struct continuation {
  job_handle m_handle;
  std::function<bool(job_handle &)> m_ready_to_resume = [](auto &) {
    return true;
  };
  continuation(job_handle handle) noexcept : m_handle(std::move(handle)) {}
  bool ready_to_resume() noexcept {
    try {
      return m_ready_to_resume(m_handle);
    } catch (...) {
      return false;
    }
  }
};

class scope : std::enable_shared_from_this<scope> {
public:
  friend class context;
  scope(const scope &) = delete;
  scope &operator=(const scope &) = delete;
  scope &operator=(scope &&) = delete;
  static std::shared_ptr<scope> create() noexcept {
    return std::make_shared<scope>();
  }
  template <typename EType>
  void run(std::function<job<void, EType>(std::unique_ptr<context>)>
               &&coro_block) noexcept {
    auto main_job = coro_block(std::make_unique<context>(shared_from_this()));
    main_job.resume();
  }
  void cancel() { m_is_cancel.store(true); }

  void loop() noexcept {
    while (true) {
      if (m_is_cancel) {
        for (auto &cont : m_coros) {
          auto &handle = cont.m_handle;
          handle.cancel();
        }
        break;
      }
      {
        std::lock_guard lock(m_coros_lock);
        continuation cont(std::move(m_coros.front()));
        m_coros.pop_front();
        if (cont.ready_to_resume()) {
          if (cont.m_handle.is_active()) {
            cont.m_handle.resume();
          }
        } else {
          m_coros.push_back(std::move(cont));
        }
      }
    }
  }

private:
  std::deque<continuation> m_coros;
  std::mutex m_coros_lock;
  std::atomic_bool m_is_cancel;
  explicit scope() = default;
};
template <class VType, class EType>
job<VType, EType> &context::reg(job<VType, EType> &&new_job) noexcept {
  {
    std::lock_guard<std::mutex> lock(m_scope->m_coros_lock);
    m_scope->m_coros.push_back(new_job);
    return m_scope->m_coros.back();
  }
}

void context::reg(continuation &&cont) noexcept {
  std::lock_guard<std::mutex> lock(m_scope->m_coros_lock);
  m_scope->m_coros.push_back(std::move(cont));
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
custom_awaiter context::delay(std::chrono::duration<Rep, Period> per) {
  return {[per, this](std::coroutine_handle<> h) {
    continuation cont{job_handle_with_coro{h},
                      [start_time = std::chrono::system_clock::now(), per]() {
                        auto now_time = std::chrono::system_clock::now();
                        auto dura = now_time - start_time;
                        return dura > per;
                      }};
    this->reg(std::move(cont));
  }};
}
} // namespace koro

#endif // scope_H