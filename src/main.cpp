#include "./Koro/job.hpp"
#include "./awaiter_demo.hpp"
#include "task.hpp"
#include <chrono>
#include <print>
#include <thread>
// A simple task-class for void-returning coroutines.
// struct task {
//   struct promise_type {
//     task get_return_object() noexcept { return {}; }
//     std::suspend_never initial_suspend() noexcept { return {}; }
//     std::suspend_never final_suspend() noexcept { return {}; }
//     void return_void() {}
//     void unhandled_exception() {}
//   };
// };

// task waiter(async_waiting_event &event) {
//   std::println("[thread]start");
//   co_await event;
//   std::println("[thread]resumed");
// }

// int main1(int argc, char **argv) {
//   async_waiting_event event(false);
//   waiter(event);
//   std::println("[main] start waiting");
//   std::this_thread::sleep_for(std::chrono::seconds(2));
//   std::println("[main] resumed");
//   event.set();
//   std::println("[main]done");
//   return 0;
// }
struct delay_awaiter {
  std::chrono::milliseconds ms;

  // 如果延时为 0，则不挂起，直接就绪
  bool await_ready() const noexcept { return ms.count() == 0; }

  // 挂起时，启动一个后台线程等待，然后 resume 协程
  void await_suspend(std::coroutine_handle<> handle) const {
    std::thread([handle, this]() {
      std::this_thread::sleep_for(ms);
      handle.resume();
    }).detach();
  }

  // resume 时不返回任何值
  void await_resume() const noexcept {}
};

inline auto delay(std::chrono::milliseconds ms) { return delay_awaiter{ms}; }

koro::job<int, int> demo(int input) {
  co_await koro::cancellation_wrapper{
      .do_cancel = [] { std::println("do cancel"); }};
  std::println("running");
  co_await delay(std::chrono::milliseconds(1000));
  co_return input * 2;
}

int main() {
  auto a = demo(1);
  a.invoke_on_completed([](auto) { std::println("finish"); });
  a.invoke_on_cancelled([] { std::println("cancelled"); });
  a.start();
  // a.cancel();
  std::this_thread::sleep_for(std::chrono::seconds(10));
  std::println("end with {}");
}