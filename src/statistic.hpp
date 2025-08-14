#pragma once

#include <cstdint>
#include <fmt/base.h>

#include "machine_code.hpp"
#include "multiple_thread_queue.hpp"

namespace ib::rt {

struct Sample {
  MachineCode::UUID uuid_;
  uint64_t cpu_cycle_;
};

class Statistic {
  MultipleThreadQueue<Sample> &statistic_queue_;

public:
  explicit Statistic(MultipleThreadQueue<Sample> &statistic_queue)
      : statistic_queue_(statistic_queue) {}

  void start();
};

} // namespace ib::rt

template <> struct fmt::formatter<ib::rt::Sample> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }
  fmt::basic_appender<char> format(const ib::rt::Sample &statistic,
                                   format_context &ctx) const;
};
