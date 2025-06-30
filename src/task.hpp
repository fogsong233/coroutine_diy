#ifndef task_H
#define task_H
#include <coroutine>
#include <exception>
#include <utility>
class task {
public:
  class promise_type;
  class awaiter;
  task(task &&t) noexcept : m_coro(std::exchange(t.m_coro, {})) {}

  ~task() {
    if (m_coro) {
      m_coro.destroy();
    }
  }

  awaiter operator co_await() && noexcept;

private:
  explicit task(std::coroutine_handle<promise_type> h) noexcept : m_coro(h) {}
  std::coroutine_handle<promise_type> m_coro;
};

class task::promise_type {
public:
  task get_return_object() noexcept {
    return task{std::coroutine_handle<promise_type>::from_promise(*this)};
  }
  std::suspend_always initial_suspend() { return {}; }
  void return_void() noexcept {};
  void unhandled_exception() noexcept { std::terminate(); };
  struct final_awaiter {
    bool await_ready() noexcept { return false; }
    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<promise_type> h) noexcept {
      return h.promise().continuation;
    }
    void await_resume() noexcept {}
  };
  final_awaiter final_suspend() noexcept { return {}; }
  std::coroutine_handle<> continuation;
};
class task::awaiter {
public:
  explicit awaiter(std::coroutine_handle<promise_type> coro) noexcept
      : m_coro(coro) {}

  bool await_ready() noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) noexcept {
    m_coro.promise().continuation = h;
    m_coro.resume();
  }

  void await_resume() noexcept {}

private:
  std::coroutine_handle<promise_type> m_coro;
};

#endif // task_H
