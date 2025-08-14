#include "fmt/format.h"
#include "machine_code.hpp"

fmt::basic_appender<char>
fmt::formatter<ib::MachineCode>::format(const ib::MachineCode &vec,
                                        format_context &ctx) const {
  std::string hex_str;
  for (uint8_t byte : vec) {
    hex_str += fmt::format("{:02X} ", byte);
  }
  return fmt::format_to(ctx.out(), "{}", hex_str);
}
