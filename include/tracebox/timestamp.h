#pragma once

#include <cstdint>

namespace tracebox {

using Timestamp = std::uint64_t;

// Returns the local calendar day represented by a Unix-epoch nanosecond
// timestamp, encoded as YYYYMMDD.
std::uint32_t day(Timestamp timestamp);

}  // namespace tracebox
