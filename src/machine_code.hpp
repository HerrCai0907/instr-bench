#pragma once

#include <vector>

#include "fmt/base.h"

namespace ib {

struct MachineCode : public std::vector<uint8_t> {
  using std::vector<uint8_t>::vector;
};

} // namespace ib

template <> struct fmt::formatter<ib::MachineCode> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }
  fmt::basic_appender<char> format(const ib::MachineCode &vec,
                                   format_context &ctx) const;
};
