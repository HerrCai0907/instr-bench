#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <spdlog/spdlog.h>
#include <vector>

#include "statistic.hpp"
#include "uuid.hpp"

fmt::basic_appender<char>
fmt::formatter<ib::rt::Sample>::format(const ib::rt::Sample &statistic,
                                       format_context &ctx) const {
  return fmt::format_to(ctx.out(), "CPU cycles: {}", statistic.cpu_cycle_);
}

namespace {

struct Centroid {
  double mean;
  double weight; // Number of data points in this centroid

  Centroid(double m, double w) : mean(m), weight(w) {}
};

struct TDigest {
  std::vector<Centroid> centroids;
  double compression;
  double maxWeight;

  explicit TDigest(double compression = 100.0) : compression(compression) {
    maxWeight = 1.0 / compression;
  }
  void add(double value) {
    if (centroids.empty()) {
      centroids.emplace_back(value, 1.0);
      return;
    }
    // Find the closest centroid to the new value
    auto it = std::min_element(centroids.begin(), centroids.end(),
                               [value](const Centroid &a, const Centroid &b) {
                                 return std::abs(a.mean - value) <
                                        std::abs(b.mean - value);
                               });

    if (std::abs(it->mean - value) < 1e-9) {
      // Merge with the closest centroid
      it->weight += 1.0;
    } else {
      // Create a new centroid
      centroids.emplace_back(value, 1.0);
      compressIfNecessary();
    }
  }

  // Compress centroids if necessary
  void compressIfNecessary() {
    if (centroids.size() <= static_cast<size_t>(compression))
      return;

    // Sort centroids by mean
    std::sort(
        centroids.begin(), centroids.end(),
        [](const Centroid &a, const Centroid &b) { return a.mean < b.mean; });
    std::vector<Centroid> newCentroids;
    for (size_t i = 0; i < centroids.size(); ++i) {
      if (newCentroids.empty()) {
        newCentroids.push_back(centroids[i]);
      } else {
        Centroid &last = newCentroids.back();
        double combinedWeight = last.weight + centroids[i].weight;
        if (combinedWeight <= maxWeight) {
          // Merge if within maxWeight
          last.mean = (last.mean * last.weight +
                       centroids[i].mean * centroids[i].weight) /
                      combinedWeight;
          last.weight = combinedWeight;
        } else {
          newCentroids.push_back(centroids[i]);
        }
      }
    }

    centroids = std::move(newCentroids);
  }

  // Estimate the quantile (0.0 <= q <= 1.0)
  double quantile(double q) const {
    if (centroids.empty())
      return std::numeric_limits<double>::quiet_NaN();
    if (q <= 0.0)
      return centroids[0].mean;
    if (q >= 1.0)
      return centroids.back().mean;

    // Compute cumulative weights
    std::vector<std::pair<double, double>> cumulative;
    double totalWeight = 0.0;
    for (const auto &c : centroids) {
      totalWeight += c.weight;
      cumulative.emplace_back(totalWeight, c.mean);
    }

    // Find the segment containing the quantile
    double targetWeight = q * totalWeight;
    auto it = std::upper_bound(
        cumulative.begin(), cumulative.end(), std::make_pair(targetWeight, 0.0),
        [](const std::pair<double, double> &a,
           const std::pair<double, double> &b) { return a.first < b.first; });

    if (it == cumulative.begin())
      return centroids[0].mean;
    if (it == cumulative.end())
      return centroids.back().mean;

    auto left = it - 1;
    auto right = it;
    double leftWeight = left->first;
    double rightWeight = right->first;
    double leftMean = left->second;
    double rightMean = right->second;

    // Linear interpolation
    double fraction = (targetWeight - leftWeight) / (rightWeight - leftWeight);
    return leftMean + fraction * (rightMean - leftMean);
  }

  // Get all centroids (for debugging/testing)
  const std::vector<Centroid> &getCentroids() const { return centroids; }
};

struct ConfidenceInterval {
  double_t lower_bound = 0.0;
  double_t upper_bound = 0.0;
};

class Stat {
  double_t mean_ = 0U;
  double_t m2_ = 0U;
  uint32_t n_ = 0U;

public:
  void update(double_t v) {
    n_ += 1;
    const double_t delta = v - mean_;
    mean_ += delta / n_;
    const double_t delta2 = v - mean_;
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

void drawHistogram(TDigest const &td) {
  if (td.centroids.empty())
    return;
  std::vector<double> data{};
  for (size_t i = 0; i < 100; i++) {
    data.push_back(td.quantile(static_cast<double>(i) / 100.0));
  }
  constexpr size_t maxBars = 1000;
  double_t max_val = *std::max_element(data.begin(), data.end());
  double scale = static_cast<double>(maxBars) / max_val;
  for (int row = maxBars; row > 0; --row) {
    std::cout << "| ";
    for (size_t height : data) {
      if (static_cast<double>(height) * scale >= row) {
        std::cout << "* ";
      } else {
        std::cout << "  ";
      }
    }
    std::cout << "|" << std::endl;
  }
  std::cout << "+ ";
  for (size_t i = 0; i < data.size(); ++i) {
    std::cout << "- ";
  }
  std::cout << "+" << std::endl;
}

void Statistic::start() {
  std::chrono::seconds last_print_time;
  std::map<UUID, Stat> stats;
  std::map<UUID, TDigest> tdigests;
  std::vector<size_t> data{20};
  while (true) {
    {
      // update
      std::unique_ptr<Sample> sample = statistic_queue_.pop();
      if (!stats.contains(sample->uuid_)) {
        stats.emplace(sample->uuid_, Stat{});
        tdigests.emplace(sample->uuid_, TDigest{});
      }
      Stat &stat = stats.at(sample->uuid_);
      stat.update(sample->cpu_cycle_);
      tdigests.at(sample->uuid_).add(sample->cpu_cycle_);
    }
    {
      // print
      const std::chrono::seconds current_time =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now().time_since_epoch());

      if (current_time - last_print_time >= std::chrono::seconds{1}) {
        spdlog::info("\x1b[2J\x1b[H");
        spdlog::info("=======STAT========");
        for (auto const &[uuid, stat] : stats) {
          spdlog::info(
              "statistics<{}>:\n - average cpu cycle: \033[31m{}\033[0m\n - "
              "confidence interval: \033[33m{}\033[0m",
              uuid, stat.avr(), stat.confidence_interval());
        }
        spdlog::info("\n");
        last_print_time = current_time;
        drawHistogram(tdigests.begin()->second);
      }
    }
  }
}

} // namespace ib::rt
