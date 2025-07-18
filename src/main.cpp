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