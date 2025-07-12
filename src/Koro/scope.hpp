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
#include <coroutine>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>
namespace koro {
class scope;
class context {
public:
  context(std::shared_ptr<scope> father_scope)
      : m_scope(std::move(father_scope)) {}
  template <class VType, class EType>
  job<VType, EType> reg(job<VType, EType> &&new_job) noexcept;

private:
  std::shared_ptr<scope> m_scope;
};

struct continuation {
  std::coroutine_handle<> handle;
  std::function<bool(std::coroutine_handle<> &)> m_ready_to_resume = [](auto) {
    return true;
  };
  continuation(std::coroutine_handle<> handle) noexcept : handle(handle) {}
  bool ready_to_resume() noexcept {
    try {
      return m_ready_to_resume(handle);
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
  void run(std::function<void(std::unique_ptr<context>)> coro_block) noexcept {
    coro_block(std::make_unique<context>(shared_from_this()));
    schedule();
  }
  void schedule() noexcept {
    while () {
    }
  }

private:
  std::vector<continuation> m_coros;
  std::mutex m_coros_lock;
  explicit scope() = default;
};
template <class VType, class EType>
job<VType, EType> context::reg(job<VType, EType> &&new_job) noexcept {
  {
    std::lock_guard<std::mutex> lock(m_scope->m_coros_lock);
    m_scope->m_coros.push_back(new_job.m_coro);
  }
  return new_job;
}

} // namespace koro

#endif // scope_H