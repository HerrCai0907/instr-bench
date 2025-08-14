#pragma once

#include <string>

#include "machine_code.hpp"

namespace ib::llvm {

void init();

MachineCode compile(const std::string &asmStr);

} // namespace ib::llvm
