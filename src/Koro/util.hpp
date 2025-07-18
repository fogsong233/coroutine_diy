#ifndef util_H
#define util_H
#include "job_base.hpp"
#include <coroutine>
#include <expected>
#include <iostream>
#include <print>
#include <type_traits>
#define KORO_DEBUG 1;
namespace koro {
template <typename T>
concept is_coroutine =
    requires { typename std::coroutine_traits<T>::promise_type; };
template <typename Fn>
concept returns_expected = requires(Fn fn) {
  {
    fn()
  } -> std::convertible_to<std::expected<typename decltype(fn())::value_type,
                                         typename decltype(fn())::error_type>>;
};
template <template <typename V, typename E> class T, typename V, typename E>
concept job_like = std::is_base_of_v<job_handle, T<V, E>>;
template <typename... Args> void d_log(Args... args) {
#ifdef KORO_DEBUG
  std::print("[koro] ");
  (std::cout << ... << std::forward<Args>(args)) << '\n';
#endif
}
template <typename Promise> struct current_promise {
  std::coroutine_handle<Promise> handle;

  bool await_ready() const noexcept { return false; }

  std::coroutine_handle<>
  await_suspend(std::coroutine_handle<Promise> h) noexcept {
    handle = h;
    return h;
  }

  std::coroutine_handle<Promise> await_resume() noexcept { return handle; }
};
} // namespace koro

#endif // util_H