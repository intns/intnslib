#ifndef INTNS_MEMORY_ALIGNMENT_HPP
#define INTNS_MEMORY_ALIGNMENT_HPP

#include <cstddef>
#include <cstdint>

namespace intns::memory {

/**
 * @brief Aligns a given address to the specified alignment boundary.
 *
 * @param addr The address to be aligned.
 * @param alignment The alignment boundary (must be a power of two).
 * @return The aligned address.
 */
inline constexpr std::uintptr_t align_address(std::uintptr_t addr,
                                              std::size_t alignment) noexcept {
  return (addr + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Checks if the given address is aligned to the specified alignment.
 *
 * @param addr The address to check for alignment.
 * @param alignment The alignment boundary (must be a power of two).
 * @return true if address is aligned, false otherwise.
 */
inline constexpr bool is_aligned(std::uintptr_t addr,
                                 std::size_t alignment) noexcept {
  return (addr & (alignment - 1)) == 0;
}

/**
 * @brief Checks if a value is a power of two.
 *
 * @param value The value to check.
 * @return true if value is a power of two, false otherwise.
 */
inline constexpr bool is_power_of_two(std::size_t value) noexcept {
  return value != 0 && (value & (value - 1)) == 0;
}
}  // namespace intns::memory

#endif
