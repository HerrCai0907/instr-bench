#pragma once

#include <cstdint>
#include <fmt/base.h>

namespace ib::rt {

struct Statistic {
  uint64_t cpu_cycle_;
};

} // namespace ib::rt

template <> struct fmt::formatter<ib::rt::Statistic> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }
  fmt::basic_appender<char> format(const ib::rt::Statistic &statistic,
                                   format_context &ctx) const;
};
