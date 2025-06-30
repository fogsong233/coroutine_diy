#ifndef awaiter_demo_H
#define awaiter_demo_H
#include <atomic>
#include <coroutine>

class async_waiting_event {
public:
  async_waiting_event(bool initSet = false) noexcept;
  ~async_waiting_event() = default;

  async_waiting_event(const async_waiting_event &) = delete;
  async_waiting_event &operator=(const async_waiting_event &) = delete;

  async_waiting_event(async_waiting_event &&) = delete;
  async_waiting_event &operator=(async_waiting_event &&) = delete;

  bool is_set() const noexcept;
  struct awaiter;
  awaiter operator co_await() const noexcept;

  void set() noexcept;
  void reset() noexcept;

private:
  friend struct awaiter;
  mutable std::atomic<void *> waiting_linked_list;
};

struct async_waiting_event::awaiter {
public:
  awaiter(const async_waiting_event &event) : m_event(event) {}
  bool await_ready() const noexcept;
  bool await_suspend(std::coroutine_handle<> await_coroutine) noexcept;
  void await_resume() noexcept {}

private:
  friend struct async_waiting_event;
  const async_waiting_event &m_event;
  std::coroutine_handle<> m_awaiting_coroutine;
  awaiter *m_nxt;
};

#endif // awaiter_demo_H