#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>

#include "statistic.hpp"
#include "uuid.hpp"

fmt::basic_appender<char>
fmt::formatter<ib::rt::Sample>::format(const ib::rt::Sample &statistic,
                                       format_context &ctx) const {
  return fmt::format_to(ctx.out(), "CPU cycles: {}", statistic.cpu_cycle_);
}

namespace {

struct Centroid {
  uint64_t sum_ = 0U;
  uint32_t count_ = 0U;

  void insert(uint64_t value) {
    sum_ += value;
    count_ += 1;
  }

  uint64_t mean() const { return count_ > 0 ? sum_ / count_ : 0; }
};

struct TDigest {
  void insert(uint64_t value) {}

  uint64_t quantile(double q) const {
    // Compute the q-th quantile
    return 0;
  }
};

struct ConfidenceInterval {
  double lower_bound = 0.0;
  double upper_bound = 0.0;
};

class Stat {
  double mean_ = 0U;
  double m2_ = 0U;
  uint32_t n_ = 0U;

public:
  void update(int64_t v) {
    n_ += 1;
    const double delta = static_cast<double>(v) - mean_;
    mean_ += delta / n_;
    const double delta2 = static_cast<double>(v) - mean_;
    m2_ += delta * delta2;
  }

  double avr() const { return mean_; }

  ConfidenceInterval confidence_interval() const {
    if (n_ <= 30) {
      return {std::numeric_limits<double>::quiet_NaN(),
              std::numeric_limits<double>::quiet_NaN()};
    }
    const double stddev = std::sqrt(m2_ / (n_ - 1));
    const double margin_of_error = 1.96 * stddev / std::sqrt(n_);
    return {mean_ - margin_of_error, mean_ + margin_of_error};
  }
};

} // namespace

template <> struct fmt::formatter<ConfidenceInterval> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }
  fmt::basic_appender<char>
  format(const ConfidenceInterval &confidence_interval,
         format_context &ctx) const {
    return fmt::format_to(ctx.out(), "[{}, {}]",
                          confidence_interval.lower_bound,
                          confidence_interval.upper_bound);
  }
};

namespace ib::rt {

void Statistic::start() {
  std::chrono::seconds last_print_time;
  std::map<UUID, Stat> stats;
  while (true) {
    {
      // update
      std::unique_ptr<Sample> sample = statistic_queue_.pop();
      if (!stats.contains(sample->uuid_)) {
        stats.emplace(sample->uuid_, Stat{});
      }
      Stat &stat = stats.at(sample->uuid_);
      stat.update(sample->cpu_cycle_);
    }
    {
      // print
      const std::chrono::seconds current_time =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now().time_since_epoch());

      if (current_time - last_print_time >= std::chrono::seconds{1}) {
        spdlog::info("=======STAT========");
        for (auto const &[uuid, stat] : stats) {
          spdlog::info(
              "statistics<{}>:\n - average cpu cycle: \033[31m{}\033[0m\n - "
              "confidence interval: \033[33m{}\033[0m",
              uuid, stat.avr(), stat.confidence_interval());
        }
        spdlog::info("\n");
        last_print_time = current_time;
      }
    }
  }
}

} // namespace ib::rt
