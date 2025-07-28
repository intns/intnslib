#ifndef INTNS_MEMORY_STACKALLOCATOR_HPP
#define INTNS_MEMORY_STACKALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include "Alignment.hpp"

namespace intns::memory {

/**
 * @class StackAllocator
 * @brief A fast, linear stack-based memory allocator for temporary allocations.
 *
 * Allocations are linear, with memory reclaimed on reset or restoring to a
 * checkpoint. Ideal for short-lived, bulk-freed allocations.
 *
 * Key Features:
 * - Allocates aligned memory blocks for single objects of any type.
 * - Supports checkpoints to save and restore allocation state.
 * - Disallows copying and moving for safety.
 * - Monitors memory usage and capacity.
 * - Manages internal memory only
 *
 * Usage Notes:
 * - Allocations valid only until allocator reset or checkpoint restore.
 * - Doesn't invoke constructors/destructors; use placement new and manual
 * cleanup.
 * - Not thread-safe; designed for single-threaded use.
 */
class StackAllocator {
 public:
  using marker_type = std::uintptr_t;
  using checkpoint_t = marker_type;
  using size_type = std::size_t;

  // Validate our assumptions
  static_assert(sizeof(marker_type) >= sizeof(void*),
                "marker_type must be able to hold pointer values");
  static_assert(std::is_same_v<marker_type, std::uintptr_t>,
                "marker_type should be uintptr_t for pointer arithmetic");
  static_assert(sizeof(size_type) >= sizeof(std::size_t),
                "size_type must accommodate all possible allocation sizes");

  /**
   * @brief Constructs a StackAllocator with the specified capacity.
   *
   * @param capacity The size in bytes of the memory block to allocate.
   * @throws std::runtime_error If capacity is zero or memory allocation fails.
   */
  StackAllocator(size_type size = 1000);

  /**
   * @brief Constructs a StackAllocator with a given memory block and size.
   * @warning Does NOT take ownership of the memory, must be free'd externally.
   *
   * @param memory Pointer to the memory buffer to be managed.
   * @param size Size of the memory buffer in bytes.
   * @throws std::runtime_error If memory is null, size is zero, or buffer is
   * too small after alignment.
   */
  StackAllocator(void* memory, size_type size);

  /**
   * @brief Destructor for the StackAllocator class.
   *
   * Releases the allocation by freeing the memory at start_marker_.
   */
  ~StackAllocator();

  // Don't allow moving or copying
  StackAllocator(const StackAllocator&) = delete;
  StackAllocator& operator=(const StackAllocator&) = delete;
  StackAllocator(StackAllocator&&) = delete;
  StackAllocator& operator=(StackAllocator&&) = delete;

  /**
   * @brief Allocates memory for a single object of type T.
   *
   * This function performs several compile-time checks to ensure that T is a
   * valid type for allocation:
   * - T must not be a reference type.
   * - T must not be void.
   * - T must be destructible.
   * - T must have a non-zero size.
   *
   * Aligns the allocation to T's requirements; if capacity is insufficient or
   * out of bounds, returns nullptr. Otherwise, returns the allocated pointer
   * and advances the marker.
   *
   * @tparam T The type of object to allocate.
   * @return The allocated memory for T, or nullptr on failure.
   */
  template <typename T>
  [[nodiscard]] T* alloc_t() noexcept {
    static_assert(!std::is_reference_v<T>,
                  "Cannot allocate storage for reference types");
    static_assert(!std::is_void_v<T>, "Cannot allocate storage for void");
    static_assert(std::is_destructible_v<T>, "Type must be destructible");
    static_assert(sizeof(T) > 0, "Type must have non-zero size");

    constexpr size_type type_size = sizeof(T);
    constexpr size_type align_req = alignof(T);  // Always power of 2 per spec

    if (type_size > capacity_) [[unlikely]] {
      return nullptr;
    }

    const auto aligned_pos = align_address(active_marker_, align_req);

    // This calculates 'type_size' BACK from the end, where are we last valid?
    const auto last_valid_start_pos = (start_marker_ + capacity_) - type_size;
    if (aligned_pos < start_marker_ || aligned_pos > last_valid_start_pos) {
      return nullptr;
    }

    // Get aligned address and advance the marker
    T* new_addr = reinterpret_cast<T*>(aligned_pos);
    active_marker_ = aligned_pos + type_size;
    return new_addr;
  }

  /**
   * @brief Allocates a block of memory from the stack allocator with the
   * specified size and alignment.
   *
   * Allocates a contiguous `size`-byte block aligned to `alignment` from the
   * stack buffer. Returns nullptr if size is zero, exceeds capacity, or if
   * alignment is invalid (not a power of two or too large).
   *
   * The allocation advances the internal marker; subsequent allocations occur
   * after the returned block. The function is [[nodiscard]] to encourage
   * checking the return value.
   *
   * @param size Number of bytes to allocate.
   * @param alignment Alignment in bytes (default: alignof(std::max_align_t)).
   * @return Pointer to allocated memory or nullptr if allocation fails.
   */
  [[nodiscard]] void* alloc(size_type size, size_type alignment = alignof(
                                                std::max_align_t)) noexcept {
    if (size == 0 || size > capacity_) {
      return nullptr;
    }

    // If anything is awry with the alignment, bail
    if (!is_power_of_two(alignment) || alignment > capacity_) [[unlikely]] {
      return nullptr;
    }

    const auto aligned_pos = align_address(active_marker_, alignment);

    // This calculates 'size' BACK from the end, where are we last valid?
    const auto last_valid_start_pos = (start_marker_ + capacity_) - size;
    if (aligned_pos < start_marker_ || aligned_pos > last_valid_start_pos) {
      return nullptr;
    }

    // Get aligned address and advance the marker
    void* new_addr = reinterpret_cast<void*>(aligned_pos);
    active_marker_ = aligned_pos + size;
    return new_addr;
  }

  /**
   * @brief Saves the current state of the stack allocator.
   * @return checkpoint_t The current checkpoint marker.
   */
  [[nodiscard]] checkpoint_t save_checkpoint() const noexcept {
    return active_marker_;
  }

  /**
   * @brief Restores the stack allocator to a previously saved checkpoint.
   *
   * @param checkpoint The previous allocation state to restore.
   * @throws std::runtime_error if checkpoint is out of valid memory range.
   */
  void restore_checkpoint(checkpoint_t checkpoint) {
    // Check the bounds of the checkpoint: [S] checkpoint lives here [E]
    if (checkpoint < start_marker_ || checkpoint > start_marker_ + capacity_)
        [[unlikely]] {
      throw std::runtime_error(
          "StackAllocator::restore_checkpoint: Invalid checkpoint");
    }

    active_marker_ = checkpoint;
  }

  /**
   * @brief Returns the number of bytes currently used by the stack allocator.
   * @return The number of bytes used.
   * @note This function is noexcept and guarantees not to throw exceptions.
   */
  [[nodiscard]] size_type bytes_used() const noexcept {
    return active_marker_ - start_marker_;
  }

  /**
   * @brief Returns the number of bytes remaining in the stack allocator.
   * @return The number of bytes remaining that can be allocated.
   * @note This function is noexcept and guarantees not to throw exceptions.
   */
  [[nodiscard]] size_type bytes_remaining() const noexcept {
    return capacity_ - bytes_used();
  }

  /**
   * @brief Returns the total capacity of the stack allocator.
   * @return The maximum number of bytes that can be stored.
   * @note This function is noexcept and guarantees not to throw exceptions.
   */
  [[nodiscard]] size_type capacity() const noexcept { return capacity_; }

  /**
   * @brief Resets the stack allocator to its initial state.
   * @note This function is noexcept and guarantees not to throw exceptions.
   */
  void reset() noexcept { active_marker_ = start_marker_; }

 private:
  // Marker to track the beginning of the stack
  marker_type start_marker_ = 0;

  // Marker to track the current position of the stack
  marker_type active_marker_ = 0;

  // The full size of the stack
  size_type capacity_ = 0;

  // Tracks ownership of the start marker
  bool owns_memory_ = true;
};

/**
 * @class StackCheckpoint
 * @brief RAII wrapper for StackAllocator to manage scoped memory checkpoints.
 *
 * StackCheckpoint captures the StackAllocator state on creation and restores it
 * on destruction, ensuring all subsequent allocations are released when it goes
 * out of scope.
 *
 * Copying and moving are disabled to strictly enforce scope management.
 */
class StackCheckpoint {
  // The parent allocator
  StackAllocator& allocator_;

  // The position at object creation, to be restored on destruction
  StackAllocator::checkpoint_t saved_ = 0;

 public:
  /**
   * @brief Constructor that saves current state of the given StackAllocator.
   * @param a Reference to the StackAllocator whose state will be checkpointed.
   */
  StackCheckpoint(StackAllocator& a)
      : allocator_(a), saved_(a.save_checkpoint()) {}

  /**
   * @brief Destructor for the StackCheckpoint class.
   *
   * Restores the allocator to the saved checkpoint upon destruction,
   * releasing post-checkpoint allocations and maintaining stack-like semantics.
   */
  ~StackCheckpoint() { allocator_.restore_checkpoint(saved_); }

  // Don't allow moving or copying - this is a scoped checkpoint!
  StackCheckpoint(const StackCheckpoint&) = delete;
  StackCheckpoint& operator=(const StackCheckpoint&) = delete;
  StackCheckpoint(StackCheckpoint&&) = delete;
  StackCheckpoint& operator=(StackCheckpoint&&) = delete;
};

}  // namespace intns::memory

#endif
