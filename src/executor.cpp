#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <spdlog/spdlog.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include "executor.hpp"
#include "machine_code.hpp"
#include "statistic.hpp"
#include "uuid.hpp"

extern "C" void trampoline(int64_t *result, void *machine_code_address,
                           uint64_t repeat_count);

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

static int64_t execute_impl(MMapRAII const &mmap_raii, uint64_t repeat_count) {
  int64_t result = 0;
  spdlog::debug("[executor] execution with result address {} and exec_mem {}",
                (void *)&result, mmap_raii.get_exec_mem());

  trampoline(&result, mmap_raii.get_exec_mem(), repeat_count);
  trampoline(&result, mmap_raii.get_exec_mem(), repeat_count);

  // yield once to avoid time out
  std::this_thread::yield();
  trampoline(&result, mmap_raii.get_exec_mem(), repeat_count);
  return result;
}

class RepeatCount {
  uint64_t count_ = 1U;
  MMapRAII *baseline_mmap_raii_ = nullptr;
  std::vector<MMapRAII *> pending_measure_repeat_cout{};

  void increase_count() { count_ *= 2U; }

public:
  uint64_t get_count() const {
    assert(!pending_measure_repeat_cout.empty());
    return count_;
  }

  void set_baseline_mmap_raii(MMapRAII *baseline_mmap_raii) {
    baseline_mmap_raii_ = baseline_mmap_raii;
    for (MMapRAII *mmap_raii : pending_measure_repeat_cout) {
      while (true) {
        int64_t const baseline_result =
            execute_impl(*baseline_mmap_raii, count_);
        int64_t const result =
            execute_impl(*mmap_raii, count_) - baseline_result;
        if (result >= 100)
          break;
        increase_count();
      }
    }
  }

  void add_case(MMapRAII *mmap_raii) {
    if (baseline_mmap_raii_ == nullptr) {
      pending_measure_repeat_cout.push_back(mmap_raii);
      return;
    }
    while (true) {
      int64_t const baseline_result =
          execute_impl(*baseline_mmap_raii_, count_);
      int64_t const result = execute_impl(*mmap_raii, count_) - baseline_result;
      if (result >= 100)
        break;
      increase_count();
    }
  }
};

void Executor::start() {
  RepeatCount repeat_counter{};
  std::map<UUID, std::unique_ptr<MMapRAII>> machine_codes;
  while (true) {
    // maintain task
    std::deque<std::unique_ptr<MachineCode>> new_machine_codes =
        machine_code_queue_.pop_all();
    for (auto &machine_code : new_machine_codes) {
      spdlog::info("[executor] add machine code with uuid {}",
                   machine_code->uuid_);
      std::unique_ptr<MMapRAII> mmap_raii =
          std::make_unique<MMapRAII>(*machine_code);

      if (machine_code->uuid_ == UUIDUtils::control_group_uuid) {
        repeat_counter.set_baseline_mmap_raii(mmap_raii.get());
      } else {
        repeat_counter.add_case(mmap_raii.get());
      }

      machine_codes[machine_code->uuid_] = std::move(mmap_raii);
    }
    std::deque<std::unique_ptr<UUID>> cancel_uuids = cancel_queue_.pop_all();
    for (auto &cancel_uuid : cancel_uuids) {
      spdlog::info("[executor] remove machine code with uuid {}", *cancel_uuid);
      machine_codes.erase(*cancel_uuid);
    }
    // plan
    if (!machine_codes.contains(UUIDUtils::control_group_uuid)) {
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
      continue;
    }

    std::vector<std::pair<UUID, MMapRAII const *>> entries;
    for (auto const &[uuid, mmap_raii] : machine_codes) {
      if (uuid == UUIDUtils::control_group_uuid)
        continue;
      entries.emplace_back(uuid, mmap_raii.get());
    }
    std::random_device rd;
    std::mt19937 rng{rd()};
    std::shuffle(entries.begin(), entries.end(), rng);

    MMapRAII const *baseline_mmap_raii =
        machine_codes.at(UUIDUtils::control_group_uuid).get();

    std::deque<std::unique_ptr<Sample>> samples;
    uint64_t const repeat_count = repeat_counter.get_count();

    // execute
    int64_t const baseline = execute_impl(*baseline_mmap_raii, repeat_count);
    for (size_t i = 0; i < 4; i++) {
      for (auto &[uuid, mmap_raii_ptr] : entries) {
        int64_t const result =
            execute_impl(*mmap_raii_ptr, repeat_count) - baseline;
        double_t const cpu_cycle =
            static_cast<double_t>(result) / static_cast<double_t>(repeat_count);
        samples.push_back(std::unique_ptr<Sample>{
            new Sample{.uuid_ = uuid, .cpu_cycle_ = cpu_cycle}});
      }
    }
    // send
    statistic_queue_.push_all(std::move(samples));
  }
}

} // namespace ib::rt
