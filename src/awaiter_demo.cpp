#include "awaiter_demo.hpp"
#include <atomic>
#include <coroutine>
bool async_waiting_event::awaiter::await_ready() const noexcept {
  return m_event.is_set();
}

bool async_waiting_event::awaiter::await_suspend(
    std::coroutine_handle<> await_coroutine) noexcept {
  void *old_value = m_event.waiting_linked_list.load(std::memory_order_acquire);
  const void *const set_state = &m_event;
  m_awaiting_coroutine = await_coroutine;
  do {
    if (old_value == set_state) {
      return false;
    }
    m_nxt = static_cast<awaiter *>(old_value);
  } while (!m_event.waiting_linked_list.compare_exchange_weak(
      old_value, this, std::memory_order_release, std::memory_order_acquire));
  return true;
}

async_waiting_event::async_waiting_event(bool initSet) noexcept
    : waiting_linked_list(initSet ? this : nullptr) {}

async_waiting_event::awaiter
async_waiting_event::operator co_await() const noexcept {
  return awaiter{*this};
}

bool async_waiting_event::is_set() const noexcept {
  return waiting_linked_list.load(std::memory_order_acquire) == this;
}

void async_waiting_event::reset() noexcept {
  void *old_value = this;
  waiting_linked_list.compare_exchange_strong(old_value, nullptr,
                                              std::memory_order_acquire);
}

void async_waiting_event::set() noexcept {
  void *old_value =
      waiting_linked_list.exchange(this, std::memory_order_acq_rel);
  if (old_value != this) {
    auto *waiters = static_cast<awaiter *>(old_value);
    while (waiters != nullptr) {
      auto *nxt = waiters->m_nxt;
      waiters->m_awaiting_coroutine.resume();
      waiters = nxt;
    }
  }
}