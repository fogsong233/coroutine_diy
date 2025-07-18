// // #include "./Koro/job.hpp"
// // #include "./awaiter_demo.hpp"
// // #include "task.hpp"
// // #include <chrono>
// // #include <print>
// // #include <thread>
// // // A simple task-class for void-returning coroutines.
// // // struct task {
// // //   struct promise_type {
// // //     task get_return_object() noexcept { return {}; }
// // //     std::suspend_never initial_suspend() noexcept { return {}; }
// // //     std::suspend_never final_suspend() noexcept { return {}; }
// // //     void return_void() {}
// // //     void unhandled_exception() {}
// // //   };
// // // };

#include "Koro/dispatcher.hpp"
#include "Koro/job.hpp"
#include "Koro/scope.hpp"
#include "Koro/util.hpp"
#include <chrono>
#include <iostream>

using namespace koro;
using normal_job = job<int, int>;

normal_job task1(std::shared_ptr<scope_ctx> ctx) {
  std::cout << "T1 start\n";
  co_await ctx->delay(std::chrono::seconds(1));
  std::cout << "T1 end\n";
  co_return 0;
}
normal_job task2(std::shared_ptr<scope_ctx> ctx) {
  std::cout << "T2 start\n";
  co_await ctx->delay(std::chrono::seconds(3));
  std::cout << "T2 end\n";
  co_return 114;
}

normal_job task3(std::shared_ptr<scope_ctx> ctx) {
  std::cout << "T3 start\n";
  co_await cancellation_wrapper{.do_cancel = [] { d_log("task3 cancelled"); }};
  co_await ctx->delay(std::chrono::seconds(2));
  co_await ctx->reg(task1(ctx))->await();
  std::cout << "T3 end\n";
  co_return 233;
}
void test1() {
  auto scp = scope::make();

  scp->run<int, job>([](std::shared_ptr<scope_ctx> ctx) -> normal_job {
    auto t1 = ctx->reg(task1(ctx));
    auto t2 = ctx->reg(task2(ctx));
    auto t3 = ctx->reg(task3(ctx));
    co_await via_new_thread{};
    co_await t1->await();
    t2->invoke_on_completed([](auto value) {
      if (value.has_value()) {
        d_log("receive: ", *value);
      }
    });
    t3->invoke_on_cancelled([] { d_log("receive t3 is cancelled"); });
    t3->cancel();
    d_log("end");
    co_return 0;
  });

  scp->loop();
}