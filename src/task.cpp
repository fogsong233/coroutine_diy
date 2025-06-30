#include "./task.hpp"

task::awaiter task::operator co_await() && noexcept { return awaiter{m_coro}; }