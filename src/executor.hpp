#pragma once

#include <deque>

#include "machine_code.hpp"
#include "statistic.hpp"

namespace ib::rt {

void execute(MachineCode const &machineCode);

} // namespace ib::rt
