#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <spdlog/spdlog.h>

#include "formatter.hpp"
#include "llvm.hpp"

int main() {
  ib::llvm::init();

  std::string asmStr = R"(
	.section	__TEXT,__text,regular,pure_instructions
	.globl	main
main:
	mov	x10, x11
  )";
  std::vector<uint8_t> machineCode = ib::llvm::compile(asmStr);
  spdlog::info("Machine code for \"{}\":\n{}", asmStr, machineCode);

  return 0;
}
