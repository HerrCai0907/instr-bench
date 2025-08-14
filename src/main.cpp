#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <spdlog/spdlog.h>

#include "executor.hpp"
#include "llvm.hpp"
#include "machine_code.hpp"

int main() {
  ib::llvm::init();

  std::string asmStr = R"(
	.section	__TEXT,__text,regular,pure_instructions
	.global	main
main:
  stp     x29, x30, [sp, #-32]!
  

  isb

	mov	    x10, x11

  isb
  
  sub     x0, x30, x29
  ldp     x29, x30, [sp], #32
  ret
  )";
  ib::MachineCode machineCode = ib::llvm::compile(asmStr);
  spdlog::info("Machine code for \"{}\":\n{}", asmStr, machineCode);

  ib::rt::execute(machineCode);

  return 0;
}
