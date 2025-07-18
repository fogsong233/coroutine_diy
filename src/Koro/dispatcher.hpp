#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <coroutine>
#include <thread>

namespace koro {

struct via_new_thread {
  bool await_ready() noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) noexcept {
    std::thread([h]() { h.resume(); }).detach();
  }
  void await_resume() noexcept {}
};

} // namespace koro

#endif // DISPATCHER_H