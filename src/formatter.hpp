#include <cstdint>
#include <fmt/format.h>
#include <vector>

template <> struct fmt::formatter<std::vector<uint8_t>> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }
  auto format(const std::vector<uint8_t> &vec, format_context &ctx) const {
    std::string hex_str;
    for (uint8_t byte : vec) {
      hex_str += fmt::format("{:02X} ", byte);
    }
    return fmt::format_to(ctx.out(), "{}", hex_str);
  }
};
