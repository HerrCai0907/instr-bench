#pragma once

#include <string>

#include "machine_code.hpp"

namespace ib::llvm {

void init();

std::unique_ptr<ib::MachineCode> compile(const std::string &asmStr);

} // namespace ib::llvm
