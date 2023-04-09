#include "concurrencpp/threads/thread.h"

#include <pthread.h>

#include <atomic>

#include "concurrencpp/platform_defs.h"
#include "concurrencpp/runtime/constants.h"

using concurrencpp::details::thread;

namespace concurrencpp::details {
std::uintptr_t generate_thread_id() noexcept {
  static std::atomic_uintptr_t s_id_seed = 1;
  return s_id_seed.fetch_add(1, std::memory_order_relaxed);
}

struct thread_per_thread_data {
  const std::uintptr_t id = generate_thread_id();
};

static thread_local thread_per_thread_data s_tl_thread_per_data;
}  // namespace concurrencpp::details

std::thread::id thread::get_id() const noexcept { return m_thread.get_id(); }

std::uintptr_t thread::get_current_virtual_id() noexcept {
  return s_tl_thread_per_data.id;
}

bool thread::joinable() const noexcept { return m_thread.joinable(); }

void thread::join() { m_thread.join(); }

size_t thread::hardware_concurrency() noexcept {
  const auto hc = std::thread::hardware_concurrency();
  return (hc != 0) ? hc : consts::k_default_number_of_cores;
}

void thread::set_name(std::string_view name) noexcept {
  ::pthread_setname_np(::pthread_self(), name.data());
}
