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
task my_task2() noexcept { co_return; }

task my_task() {
  int a = 10000;
  while (a--) {
    co_await my_task2();
  }
}
int main() { my_task(); }