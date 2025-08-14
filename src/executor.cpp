#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <sys/mman.h>
#include <unistd.h>

#include "executor.hpp"
#include "machine_code.hpp"
#include "statistic.hpp"

extern "C" void trampoline(uint64_t *result_x0, void *machine_code_address_x1);

namespace ib::rt {} // namespace ib::rt

static size_t get_page_size() {
  static size_t const page_size = static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
  return page_size;
}

static size_t round_to_page_size(size_t size) {
  size_t const page_size = get_page_size();
  return (size + page_size - 1) & ~(page_size - 1);
}

void ib::rt::execute(MachineCode const &machineCode) {
  // Create executable memory mapping
  size_t const code_size = round_to_page_size(machineCode.size());
  spdlog::info("mmap {} with RW permission", code_size);
  void *exec_mem = mmap(nullptr, code_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (exec_mem == MAP_FAILED) {
    spdlog::error("Failed to allocate executable memory");
    std::abort();
  }
  std::memcpy(exec_mem, machineCode.data(), code_size);

  spdlog::info("mprotect {} with RE permission", code_size);
  mprotect(exec_mem, code_size, PROT_READ | PROT_EXEC);

  uint64_t result = 0;
  spdlog::info("execution with result address {} and exec_mem {}",
               (void *)&result, exec_mem);
  trampoline(&result, exec_mem);

  Statistic const statistic{.cpu_cycle_ = result};

  spdlog::info("execution result: {}", statistic);

  munmap(exec_mem, code_size);
}
