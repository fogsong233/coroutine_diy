#ifndef util_H
#define util_H
#include <coroutine>
#include <expected>
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

} // namespace koro

#endif // util_H