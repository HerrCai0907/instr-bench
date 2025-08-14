#pragma once

#include <cstdint>

namespace ib {

using UUID = uint64_t;

namespace UUIDUtils {

inline UUID alloc() {
  static UUID id = 0U;
  return id++;
}
inline constexpr UUID control_group_uuid = static_cast<UUID>(-1);

} // namespace UUIDUtils

} // namespace ib
