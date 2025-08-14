#pragma once

#include <vector>

#include "fmt/base.h"

namespace ib {

class MachineCode : private std::vector<uint8_t> {
  friend struct fmt::formatter<ib::MachineCode>;
  static uint64_t uuid_generator_;
  uint64_t uuid_;

public:
  using UUID = uint64_t;

  using std::vector<uint8_t>::data;
  using std::vector<uint8_t>::size;
  using std::vector<uint8_t>::begin;
  using std::vector<uint8_t>::end;
  using std::vector<uint8_t>::resize;

  uint64_t getUUID() const { return uuid_; }

  MachineCode() : std::vector<uint8_t>{}, uuid_(++uuid_generator_) {}
};

} // namespace ib

template <> struct fmt::formatter<ib::MachineCode> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }
  fmt::basic_appender<char> format(const ib::MachineCode &vec,
                                   format_context &ctx) const;
};
