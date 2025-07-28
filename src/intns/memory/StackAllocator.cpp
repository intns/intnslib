#include "StackAllocator.hpp"

#include <memory>

namespace intns::memory {
StackAllocator::StackAllocator(size_type capacity) {
  if (capacity == 0) {
    throw std::runtime_error(
        "StackAllocator: Cannot allocate 0 memory for stack.");
  }

  void* memory = std::malloc(capacity);
  if (!memory) [[unlikely]] {
    throw std::runtime_error("StackAllocator: Failed to allocate memory.");
  }

  start_marker_ = reinterpret_cast<marker_type>(memory);
  active_marker_ = start_marker_;
  capacity_ = capacity;
  owns_memory_ = true;
}

StackAllocator::StackAllocator(void* memory, size_type size) {
  if (!memory) [[unlikely]] {
    throw std::runtime_error("StackAllocator: Handed null memory pointer.");
  }

  if (size == 0) [[unlikely]] {
    throw std::runtime_error("StackAllocator: Cannot manage 0-byte buffer");
  }

  // If the memory isn't aligned properly, we'll check
  // And align it ourselves, since they're too lazy :(
  constexpr size_type min_usable_space =
      alignof(std::max_align_t);  // minimum bytes for a 'usable' stack

  std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(memory);
  std::uintptr_t start = align_address(addr, alignof(std::max_align_t));
  std::uintptr_t offset = start - addr;

  // If unaligned, offset is non-zero; check for minimum usable space
  if (offset != 0 && offset + min_usable_space >= size) [[unlikely]] {
    throw std::runtime_error(
        "StackAllocator: Buffer too small after alignment");
  }

  start_marker_ = start;
  active_marker_ = start_marker_;
  capacity_ = size - offset;
  owns_memory_ = false;
}

StackAllocator::~StackAllocator() {
  if (owns_memory_) {
    std::free(reinterpret_cast<std::uint8_t*>(start_marker_));
  }
}

}  // namespace intns::memory
