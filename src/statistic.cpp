#include "statistic.hpp"

fmt::basic_appender<char>
fmt::formatter<ib::rt::Statistic>::format(const ib::rt::Statistic &statistic,
                                          format_context &ctx) const {
  return fmt::format_to(ctx.out(), "CPU Cycles: {}", statistic.cpu_cycle_);
}
