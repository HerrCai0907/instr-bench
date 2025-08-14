#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

#include "executor.hpp"
#include "machine_code.hpp"
#include "statistic.hpp"

extern "C" void trampoline(uint64_t *result_x0, void *machine_code_address_x1);

static size_t get_page_size() {
  static size_t const page_size = static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
  return page_size;
}

static size_t round_to_page_size(size_t size) {
  size_t const page_size = get_page_size();
  return (size + page_size - 1) & ~(page_size - 1);
}

namespace ib::rt {

namespace {

class MMapRAII {
  void *exec_mem_;
  size_t code_size_;

public:
  void *get_exec_mem() const { return exec_mem_; }

  explicit MMapRAII(MachineCode const &machine_code) : exec_mem_(nullptr) {
    code_size_ = round_to_page_size(machine_code.size());
    spdlog::info("[mm] mmap code with RW permission");
    exec_mem_ = mmap(nullptr, code_size_, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (exec_mem_ == MAP_FAILED) {
      spdlog::error("[mm] failed to allocate executable memory");
      std::abort();
    }
    spdlog::info("[mm] mmaped with RW permission in [{} {}]", exec_mem_,
                 code_size_);
    std::memcpy(exec_mem_, machine_code.data(), code_size_);
    spdlog::info("[mm] mprotect with RE permission in [{} {}]", exec_mem_,
                 code_size_);
    mprotect(exec_mem_, code_size_, PROT_READ | PROT_EXEC);
  }
  ~MMapRAII() {
    if (exec_mem_ != MAP_FAILED) {
      spdlog::info("[mm] munmap [{} {}]", exec_mem_, code_size_);
      munmap(exec_mem_, code_size_);
    }
  }
};

} // namespace

void Executor::start() {
  std::map<MachineCode::UUID, std::unique_ptr<MMapRAII>> machine_codes;
  while (true) {
    // maintain task
    std::deque<std::unique_ptr<MachineCode>> new_machine_codes =
        machine_code_queue_.pop_all();
    for (auto &machine_code : new_machine_codes) {
      spdlog::info("[executor] add machine code with uuid {}",
                   machine_code->getUUID());
      machine_codes[machine_code->getUUID()] =
          std::make_unique<MMapRAII>(*machine_code);
    }
    std::deque<std::unique_ptr<MachineCode::UUID>> cancel_uuids =
        cancel_queue_.pop_all();
    for (auto &cancel_uuid : cancel_uuids) {
      spdlog::info("[executor] remove machine code with uuid {}", *cancel_uuid);
      machine_codes.erase(*cancel_uuid);
    }
    // execute
    for (auto &[uuid, mmap_raii] : machine_codes) {
      uint64_t result = 0;
      spdlog::debug(
          "[executor] execution with result address {} and exec_mem {}",
          (void *)&result, mmap_raii->get_exec_mem());
      // yield once to avoid time out
      std::this_thread::yield();
      trampoline(&result, mmap_raii->get_exec_mem());
      statistic_queue_.push(std::unique_ptr<Sample>{
          new Sample{.uuid_ = uuid, .cpu_cycle_ = result}});
    }
  }
}

} // namespace ib::rt

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
  // yield once to avoid time out
  std::this_thread::yield();
  trampoline(&result, exec_mem);

  Sample const statistic{.cpu_cycle_ = result};

  spdlog::info("execution result: {}", statistic);

  munmap(exec_mem, code_size);
}
