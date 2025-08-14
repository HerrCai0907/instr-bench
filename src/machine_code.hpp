#pragma once

#include <vector>

#include "fmt/base.h"

namespace ib {

class MachineCode : private std::vector<uint8_t> {
  friend struct fmt::formatter<ib::MachineCode>;

public:
  uint64_t uuid_;

  using std::vector<uint8_t>::data;
  using std::vector<uint8_t>::size;
  using std::vector<uint8_t>::begin;
  using std::vector<uint8_t>::end;
  using std::vector<uint8_t>::resize;

  MachineCode() : std::vector<uint8_t>{}, uuid_(-1) {}
};

} // namespace ib

template <> struct fmt::formatter<ib::MachineCode> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }
  fmt::basic_appender<char> format(const ib::MachineCode &vec,
                                   format_context &ctx) const;
};
