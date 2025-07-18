# Coroutine Diy in Cpp
<center>
made with love by kacent💕
</center>
This is a step-by-step construction of coroutine 
framework using compiler generation in cpp20.

## feature
- An elegant demo of a modern coroutine framework, which 
abstract coroutine as a thread dispatched by our program,
idea from coroutine in kotlin.
- Reserved api for further async IO, thread dispatchers.

Notice that the performance of this framework is not
very good now, due to frequent use of mutex, maybe there will be a improvement by using lock-free deque and condition varible instead.

## Example
```cpp
#include "Koro/dispatcher.hpp"
#include "Koro/job.hpp"
#include "Koro/job_base.hpp"
#include "Koro/manager_job.hpp"
#include "Koro/scope.hpp"
#include "Koro/util.hpp"
#include <chrono>
#include <coroutine>
#include <string>
#include <thread>
using namespace koro;
using namespace std::chrono_literals;

template <class Rep, class Period>
job<std::string, int> get_web(scope_context ctx,
                              std::chrono::duration<Rep, Period> per) {
  co_await via_new_thread();
  d_log(std::this_thread::get_id(), "new get");
  volatile bool stop = false;
  // use this for cancel
  co_await cancellation_wrapper{[&] { stop = true; }};
  auto start = std::chrono::system_clock::now();
  auto m_per = start - start;
  while (true) {
    m_per = std::chrono::system_clock::now() - start;
    if (m_per > per) {
      d_log(std::this_thread::get_id(), "200 ok");
      co_return std::string("200 ok");
      break;
    }
    if (stop) {
      d_log(std::this_thread::get_id(), "cancelled");
      co_return std::string("not found");
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
}
// manager job can auto cancel its sub-coro registered
by promise.
manager_job<int, int> cancel_test_fn(scope_context ctx) {
  d_log("requesting");
  auto &promise =
      (co_await current_promise<manager_job<int, int>::promise_type>())
          .promise();
  d_log("requesting");
  promise.reg(get_web(ctx, 4s));
  promise.reg(get_web(ctx, 1s));
  promise.reg(get_web(ctx, 5s));
  co_return 0;
}

int main(int argc, const char **argv) {
  auto scp = scope::make();
  scp->run<int, job>([](scope_context ctx) -> job<int, int> {
    d_log("cancel bef");
    auto j = ctx->reg(cancel_test_fn(ctx));
    co_await ctx->delay(3s);
    d_log("cancel it");
    j->cancel();
    co_await ctx->delay(7s);
    co_return 0;
  });
  scp->loop();
  return 0;
}
```

```cpp
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
    // a based thread conversation.
    co_await via_new_thread{};
    co_await t1->await();
    // support callback
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
```