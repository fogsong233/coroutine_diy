// #include "./Koro/job.hpp"
// #include "./awaiter_demo.hpp"
// #include "task.hpp"
// #include <chrono>
// #include <print>
// #include <thread>
// // A simple task-class for void-returning coroutines.
// // struct task {
// //   struct promise_type {
// //     task get_return_object() noexcept { return {}; }
// //     std::suspend_never initial_suspend() noexcept { return {}; }
// //     std::suspend_never final_suspend() noexcept { return {}; }
// //     void return_void() {}
// //     void unhandled_exception() {}
// //   };
// // };

// // task waiter(async_waiting_event &event) {
// //   std::println("[thread]start");
// //   co_await event;
// //   std::println("[thread]resumed");
// // }

// // int main1(int argc, char **argv) {
// //   async_waiting_event event(false);
// //   waiter(event);
// //   std::println("[main] start waiting");
// //   std::this_thread::sleep_for(std::chrono::seconds(2));
// //   std::println("[main] resumed");
// //   event.set();
// //   std::println("[main]done");
// //   return 0;
// // }

// koro::job<int, int> demo(int input) {
//   co_await koro::cancellation_wrapper{
//       .do_cancel = [] { std::println("do cancel"); }};
//   std::println("running");
//   co_return input * 2;
// }

// int main() {
//   auto a = demo(1);
//   a.invoke_on_completed([](auto) { std::println("finish"); });
//   a.invoke_on_cancelled([] { std::println("cancelled"); });
//   // a.cancel();
//   std::this_thread::sleep_for(std::chrono::seconds(10));
// }
#include "Koro/job.hpp"
#include "Koro/scope.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace koro;
// 任务1：延迟2秒
job<void, int> task1(std::unique_ptr<context> ctx) {
  std::cout << "Task 1 started\n";

  // 延迟 2 秒
  co_await ctx->delay(std::chrono::seconds(2));

  std::cout << "Task 1 finished after 2 seconds\n";
}

// 任务2：延迟1秒
job<void> task2(std::unique_ptr<context> ctx) {
  std::cout << "Task 2 started\n";

  // 延迟 1 秒
  co_await ctx->delay(std::chrono::seconds(1));

  std::cout << "Task 2 finished after 1 second\n";
}

// 任务3：延迟3秒
job<void> task3(std::unique_ptr<context> ctx) {
  std::cout << "Task 3 started\n";

  // 延迟 3 秒
  co_await ctx->delay(std::chrono::seconds(3));

  std::cout << "Task 3 finished after 3 seconds\n";
}

int main() {
  // 创建一个新的 scope
  auto scp = scope::create();

  // 使用 scope 来运行任务
  scp->run([](std::unique_ptr<context> ctx) -> job<void> {
    // 注册多个任务
    ctx->reg(task1(std::move(ctx))); // 注册 task1
    ctx->reg(task2(std::move(ctx))); // 注册 task2
    ctx->reg(task3(std::move(ctx))); // 注册 task3

    // 返回一个不做任何事情的 job 作为 main loop
    co_return;
  });

  // 进入事件循环
  scp->loop();

  return 0;
}
