#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <memory>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>
#include <thread>

#include "executor.hpp"
#include "llvm.hpp"
#include "machine_code.hpp"
#include "statistic.hpp"

void add_bench_target(
    std::string const &asm_str,
    MultipleThreadQueue<ib::MachineCode> &machine_code_queue) {
  static std::string const asm_prefix = R"(
	.section	__TEXT,__text,regular,pure_instructions
	.global	main
main:
  )";
  static std::string const asm_postfix = R"(
  ret
  )";
  std::unique_ptr<ib::MachineCode> machine_code =
      ib::llvm::compile(asm_prefix + asm_str + asm_postfix);
  spdlog::info("machine code for \"{}\":\n{}", asm_str, *machine_code);
  machine_code_queue.push(std::move(machine_code));
}

int main() {
  ib::llvm::init();
  spdlog::cfg::load_env_levels();

  MultipleThreadQueue<ib::MachineCode> machine_code_queue;
  MultipleThreadQueue<ib::MachineCode::UUID> cancel_queue;
  MultipleThreadQueue<ib::rt::Sample> statistic_queue;

  std::thread execute_thread{[&]() {
    ib::rt::Executor executor{machine_code_queue, cancel_queue,
                              statistic_queue};
    executor.start();
  }};

  std::thread statistic_thread{[&]() {
    ib::rt::Statistic statistic{statistic_queue};
    statistic.start();
  }};

  add_bench_target(R"(
    add	    x0, x0, x1
    add	    x0, x0, x3
    add	    x0, x0, x4
  )",
                   machine_code_queue);
  add_bench_target(R"(
    add	    x9, x0, x1
    add	    x8, x2, x3
    add	    x0, x9, x8
  )",
                   machine_code_queue);
  add_bench_target(R"(
    add	    x9, x0, x1
    add	    x0, x2, x3
    add	    x0, x0, x8
  )",
                   machine_code_queue);

  execute_thread.join();
  statistic_thread.join();
  return 0;
}
