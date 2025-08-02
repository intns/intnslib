#ifndef INTNS_IO_BINARY_READER_HPP
#define INTNS_IO_BINARY_READER_HPP

#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "IoTypes.hpp"

namespace intns::io {

/**
 * @brief A memory-based binary reader with configurable endianness.
 *
 * MemoryReader provides efficient reading of binary data from memory buffers
 * with automatic endianness conversion. It supports reading primitive types,
 * arrays, and strings from a contiguous memory region.
 *
 * @tparam E The endianness for data interpretation (default: little endian).
 *
 * @section Usage
 * Construct with a buffer and read data sequentially:
 * @code
 * std::vector<uint8_t> buffer = {...};
 * MemoryReader<Endianness::kBig> reader(buffer);
 * uint32_t value = reader.read_u32();
 * @endcode
 *
 * @section Exception Safety
 * All read operations throw std::out_of_range if attempting to read beyond
 * the buffer boundary. Position manipulation methods provide the no-throw
 * guarantee.
 */
template <Endianness E = Endianness::kLittle>
class MemoryReader {
 public:
  /**
   * @brief Constructs a MemoryReader from a vector buffer.
   *
   * @param buffer The source buffer to read from.
   * @param position Initial read position (default: 0).
   * @throws std::out_of_range If position is beyond buffer size.
   */
  explicit MemoryReader(const std::vector<uint8_t>& buffer, size_t position = 0)
      : data_(buffer.data()), size_(buffer.size()), position_(position) {
    if (position > size_) {
      throw std::out_of_range("Initial position " + std::to_string(position) +
                              " exceeds buffer size " + std::to_string(size_));
    }
  }

  /**
   * @brief Constructs a MemoryReader from a span buffer.
   *
   * @param buffer The source buffer to read from.
   * @param position Initial read position (default: 0).
   * @throws std::out_of_range If position is beyond buffer size.
   */
  explicit MemoryReader(std::span<const uint8_t> buffer, size_t position = 0)
      : data_(buffer.data()), size_(buffer.size()), position_(position) {
    if (position > size_) {
      throw std::out_of_range("Initial position " + std::to_string(position) +
                              " exceeds buffer size " + std::to_string(size_));
    }
  }

  /**
   * @brief Returns the total size of the buffer.
   *
   * @return The size of the underlying buffer in bytes.
   */
  [[nodiscard]] size_t size() const noexcept { return size_; }

  /**
   * @brief Returns the current read position.
   *
   * @return The current position within the buffer.
   */
  [[nodiscard]] size_t position() const noexcept { return position_; }

  /**
   * @brief Returns the number of bytes remaining to be read.
   *
   * @return The number of bytes from current position to end of buffer.
   */
  [[nodiscard]] size_t remaining() const noexcept { return size_ - position_; }

  /**
   * @brief Sets the read position within the buffer.
   *
   * If the specified position exceeds the buffer size, it is clamped to the
   * buffer size.
   *
   * @param pos The new position to set.
   * @note This method never throws.
   */
  void set_position(size_t pos) noexcept {
    if (pos > size_) pos = size_;
    position_ = pos;
  }

  /**
   * @brief Advances the read position by the specified number of bytes.
   *
   * If skipping would move beyond the buffer end, the position is set to
   * the buffer end.
   *
   * @param bytes The number of bytes to skip.
   * @note This method never throws.
   */
  void skip(size_t bytes) noexcept {
    position_ = std::min(position_ + bytes, size_);
  }

  /**
   * @brief Reads an unsigned 8-bit integer.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] uint8_t read_u8() {
    if (position_ >= size_) {
      throw std::out_of_range("Cannot read uint8_t: position " +
                              std::to_string(position_) + " >= size " +
                              std::to_string(size_));
    }
    return data_[position_++];
  }

  /**
   * @brief Reads an unsigned 16-bit integer with endianness conversion.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] uint16_t read_u16() {
    if (position_ + 2 > size_) {
      throw std::out_of_range("Cannot read uint16_t: need 2 bytes, only " +
                              std::to_string(remaining()) + " available");
    }
    uint16_t value;
    std::memcpy(&value, data_ + position_, 2);
    position_ += 2;
    if constexpr (E != native_endian()) value = bswap_16(value);
    return value;
  }

  /**
   * @brief Reads an unsigned 32-bit integer with endianness conversion.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] uint32_t read_u32() {
    if (position_ + 4 > size_) {
      throw std::out_of_range("Cannot read uint32_t: need 4 bytes, only " +
                              std::to_string(remaining()) + " available");
    }
    uint32_t value;
    std::memcpy(&value, data_ + position_, 4);
    position_ += 4;
    if constexpr (E != native_endian()) value = bswap_32(value);
    return value;
  }

  /**
   * @brief Reads an unsigned 64-bit integer with endianness conversion.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] uint64_t read_u64() {
    if (position_ + 8 > size_) {
      throw std::out_of_range("Cannot read uint64_t: need 8 bytes, only " +
                              std::to_string(remaining()) + " available");
    }
    uint64_t value;
    std::memcpy(&value, data_ + position_, 8);
    position_ += 8;
    if constexpr (E != native_endian()) value = bswap_64(value);
    return value;
  }

  /**
   * @brief Reads a signed 8-bit integer.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] int8_t read_s8() { return static_cast<int8_t>(read_u8()); }

  /**
   * @brief Reads a signed 16-bit integer with endianness conversion.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] int16_t read_s16() { return static_cast<int16_t>(read_u16()); }

  /**
   * @brief Reads a signed 32-bit integer with endianness conversion.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] int32_t read_s32() { return static_cast<int32_t>(read_u32()); }

  /**
   * @brief Reads a signed 64-bit integer with endianness conversion.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] int64_t read_s64() { return static_cast<int64_t>(read_u64()); }

  /**
   * @brief Reads a 32-bit floating-point value with endianness conversion.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] float read_f32() { return std::bit_cast<float>(read_u32()); }

  /**
   * @brief Reads a 64-bit floating-point value with endianness conversion.
   *
   * @return The value read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] double read_f64() { return std::bit_cast<double>(read_u64()); }

  /**
   * @brief Reads raw bytes into a destination buffer.
   *
   * @param dest Pointer to the destination buffer.
   * @param bytes Number of bytes to read.
   * @throws std::invalid_argument If dest is nullptr.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  void read_bytes(void* dest, size_t bytes) {
    if (!dest) {
      throw std::invalid_argument("Destination pointer cannot be null");
    }
    if (position_ + bytes > size_) {
      throw std::out_of_range("Cannot read " + std::to_string(bytes) +
                              " bytes: only " + std::to_string(remaining()) +
                              " available");
    }
    std::memcpy(dest, data_ + position_, bytes);
    position_ += bytes;
  }

  /**
   * @brief Reads an array of unsigned 16-bit integers with endianness
   * conversion.
   *
   * @param array Pointer to the destination array.
   * @param count Number of elements to read.
   * @throws std::invalid_argument If array is nullptr.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  void read_u16_array(uint16_t* array, size_t count) {
    if (!array) {
      throw std::invalid_argument("Array pointer cannot be null");
    }
    read_bytes(array, count * 2);
    if constexpr (E != native_endian()) {
      for (size_t i = 0; i < count; ++i) {
        array[i] = bswap_16(array[i]);
      }
    }
  }

  /**
   * @brief Reads an array of unsigned 32-bit integers with endianness
   * conversion.
   *
   * @param array Pointer to the destination array.
   * @param count Number of elements to read.
   * @throws std::invalid_argument If array is nullptr.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  void read_u32_array(uint32_t* array, size_t count) {
    if (!array) {
      throw std::invalid_argument("Array pointer cannot be null");
    }
    read_bytes(array, count * 4);
    if constexpr (E != native_endian()) {
      for (size_t i = 0; i < count; ++i) {
        array[i] = bswap_32(array[i]);
      }
    }
  }

  /**
   * @brief Reads a fixed-length string from the buffer.
   *
   * @param length Number of bytes to read.
   * @return The string read from the buffer.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] std::string read_string(size_t length) {
    if (position_ + length > size_) {
      throw std::out_of_range("Cannot read string of length " +
                              std::to_string(length) + ": only " +
                              std::to_string(remaining()) + " bytes available");
    }
    std::string result(reinterpret_cast<const char*>(data_ + position_),
                       length);
    position_ += length;
    return result;
  }

  /**
   * @brief Reads a null-terminated C-style string from the buffer.
   *
   * Reads until a null terminator is found or the end of buffer is reached.
   * If no null terminator is found, reads all remaining bytes.
   *
   * @return The string read from the buffer (without null terminator).
   * @note This method never throws.
   */
  [[nodiscard]] std::string read_cstring() {
    const uint8_t* start = data_ + position_;
    const uint8_t* end =
        static_cast<const uint8_t*>(std::memchr(start, 0, remaining()));

    if (!end) {
      // No null terminator - read to end
      end = data_ + size_;
    }

    size_t length = end - start;
    std::string result(reinterpret_cast<const char*>(start), length);
    position_ += length + (end < data_ + size_ ? 1 : 0);
    return result;
  }

  /**
   * @brief Peeks at an unsigned 8-bit integer without advancing position.
   *
   * @return The value at the current position.
   * @throws std::out_of_range If the position is at or beyond the buffer end.
   */
  [[nodiscard]] uint8_t peek_u8() const {
    if (position_ >= size_) {
      throw std::out_of_range("Cannot peek uint8_t: position " +
                              std::to_string(position_) + " >= size " +
                              std::to_string(size_));
    }
    return data_[position_];
  }

  /**
   * @brief Peeks at an unsigned 16-bit integer without advancing position.
   *
   * @return The value at the current position with endianness conversion.
   * @throws std::out_of_range If insufficient bytes remain in the buffer.
   */
  [[nodiscard]] uint16_t peek_u16() const {
    if (position_ + 2 > size_) {
      throw std::out_of_range("Cannot peek uint16_t: need 2 bytes, only " +
                              std::to_string(size_ - position_) + " available");
    }
    uint16_t value;
    std::memcpy(&value, data_ + position_, 2);
    if constexpr (E != native_endian()) value = bswap_16(value);
    return value;
  }

 private:
  /**
   * @brief Determines the native endianness of the system.
   *
   * @return The system's native endianness.
   */
  static constexpr Endianness native_endian() noexcept {
    return std::endian::native == std::endian::little ? Endianness::kLittle
                                                      : Endianness::kBig;
  }

  const uint8_t* data_;  ///< Pointer to the underlying data buffer.
  size_t size_;          ///< Total size of the buffer in bytes.
  size_t position_;      ///< Current read position within the buffer.
};

/**
 * @brief A file-based binary reader with buffering and configurable endianness.
 *
 * FileReader provides efficient reading of binary data from files with
 * automatic buffering and endianness conversion. It supports the same
 * operations as MemoryReader but reads data from a file stream.
 *
 * @tparam E The endianness for data interpretation (default: little endian).
 *
 * @section Usage
 * Open a file and read data sequentially:
 * @code
 * FileReader<Endianness::kBig> reader("data.bin");
 * uint32_t magic = reader.read_u32();
 * std::string header = reader.read_string(16);
 * @endcode
 *
 * @section Exception Safety
 * Constructor throws std::runtime_error if file cannot be opened.
 * Read operations throw std::out_of_range if attempting to read beyond EOF.
 * File operations may throw std::ios_base::failure based on stream state.
 */
template <Endianness E = Endianness::kLittle>
class FileReader {
 public:
  /**
   * @brief Constructs a FileReader and opens the specified file.
   *
   * @param filename Path to the file to read.
   * @param buffer_size Size of the internal buffer (default: 8192 bytes).
   * @throws std::runtime_error If the file cannot be opened.
   * @throws std::invalid_argument If buffer_size is 0.
   */
  explicit FileReader(const std::string& filename, size_t buffer_size = 8192)
      : buffer_(buffer_size) {
    if (buffer_size == 0) {
      throw std::invalid_argument("Buffer size cannot be zero");
    }

    file_.open(filename, std::ios::binary);
    if (!file_) {
      throw std::runtime_error("Failed to open file: " + filename);
    }

    file_.seekg(0, std::ios::end);
    file_size_ = file_.tellg();
    file_.seekg(0);

    fill_buffer();
  }

  /**
   * @brief Returns the total size of the file.
   *
   * @return The size of the file in bytes.
   */
  [[nodiscard]] size_t size() const noexcept { return file_size_; }

  /**
   * @brief Returns the current read position in the file.
   *
   * @return The current position within the file.
   */
  [[nodiscard]] size_t position() const noexcept {
    return file_pos_ - buffer_remaining();
  }

  /**
   * @brief Returns the number of bytes remaining to be read.
   *
   * @return The number of bytes from current position to end of file.
   */
  [[nodiscard]] size_t remaining() const noexcept {
    return file_size_ - position();
  }

  /**
   * @brief Sets the read position within the file.
   *
   * @param pos The new position to set.
   * @throws std::runtime_error If seeking fails.
   * @note Position is clamped to file size if it exceeds the file boundary.
   */
  void set_position(size_t pos) {
    if (pos > file_size_) pos = file_size_;
    file_.seekg(pos);
    if (!file_) {
      throw std::runtime_error("Failed to seek to position " +
                               std::to_string(pos));
    }
    file_pos_ = pos;
    fill_buffer();
  }

  /**
   * @brief Advances the read position by the specified number of bytes.
   *
   * @param bytes The number of bytes to skip.
   * @throws std::runtime_error If seeking fails.
   */
  void skip(size_t bytes) { set_position(position() + bytes); }

  /**
   * @brief Reads an unsigned 8-bit integer.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If at end of file.
   */
  [[nodiscard]] uint8_t read_u8() {
    ensure_available(1);
    return buffer_[buffer_pos_++];
  }

  /**
   * @brief Reads an unsigned 16-bit integer with endianness conversion.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  [[nodiscard]] uint16_t read_u16() {
    ensure_available(2);
    uint16_t value;
    std::memcpy(&value, &buffer_[buffer_pos_], 2);
    buffer_pos_ += 2;
    if constexpr (E != native_endian()) value = bswap_16(value);
    return value;
  }

  /**
   * @brief Reads an unsigned 32-bit integer with endianness conversion.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  [[nodiscard]] uint32_t read_u32() {
    ensure_available(4);
    uint32_t value;
    std::memcpy(&value, &buffer_[buffer_pos_], 4);
    buffer_pos_ += 4;
    if constexpr (E != native_endian()) value = bswap_32(value);
    return value;
  }

  /**
   * @brief Reads an unsigned 64-bit integer with endianness conversion.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  [[nodiscard]] uint64_t read_u64() {
    ensure_available(8);
    uint64_t value;
    std::memcpy(&value, &buffer_[buffer_pos_], 8);
    buffer_pos_ += 8;
    if constexpr (E != native_endian()) value = bswap_64(value);
    return value;
  }

  /**
   * @brief Reads a signed 8-bit integer.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If at end of file.
   */
  [[nodiscard]] int8_t read_s8() { return static_cast<int8_t>(read_u8()); }

  /**
   * @brief Reads a signed 16-bit integer with endianness conversion.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  [[nodiscard]] int16_t read_s16() { return static_cast<int16_t>(read_u16()); }

  /**
   * @brief Reads a signed 32-bit integer with endianness conversion.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  [[nodiscard]] int32_t read_s32() { return static_cast<int32_t>(read_u32()); }

  /**
   * @brief Reads a signed 64-bit integer with endianness conversion.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  [[nodiscard]] int64_t read_s64() { return static_cast<int64_t>(read_u64()); }

  /**
   * @brief Reads a 32-bit floating-point value with endianness conversion.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  [[nodiscard]] float read_f32() { return std::bit_cast<float>(read_u32()); }

  /**
   * @brief Reads a 64-bit floating-point value with endianness conversion.
   *
   * @return The value read from the file.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  [[nodiscard]] double read_f64() { return std::bit_cast<double>(read_u64()); }

  /**
   * @brief Reads raw bytes into a destination buffer.
   *
   * @param dest Pointer to the destination buffer.
   * @param bytes Number of bytes to read.
   * @throws std::invalid_argument If dest is nullptr.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  void read_bytes(void* dest, size_t bytes) {
    if (!dest) {
      throw std::invalid_argument("Destination pointer cannot be null");
    }

    uint8_t* out = static_cast<uint8_t*>(dest);
    while (bytes > 0) {
      ensure_available(1);
      size_t chunk = std::min(bytes, buffer_remaining());
      std::memcpy(out, &buffer_[buffer_pos_], chunk);
      buffer_pos_ += chunk;
      out += chunk;
      bytes -= chunk;
    }
  }

  /**
   * @brief Reads a fixed-length string from the file.
   *
   * @param length Number of bytes to read.
   * @return The string read from the file.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   */
  [[nodiscard]] std::string read_string(size_t length) {
    std::string result(length, '\0');
    read_bytes(result.data(), length);
    return result;
  }

  /**
   * @brief Reads a null-terminated C-style string from the file.
   *
   * @return The string read from the file (without null terminator).
   * @throws std::out_of_range If at end of file before null terminator.
   * @note This implementation reads byte-by-byte for simplicity with buffering.
   */
  [[nodiscard]] std::string read_cstring() {
    std::string result;
    uint8_t ch;
    while ((ch = read_u8()) != 0) {
      result.push_back(static_cast<char>(ch));
    }
    return result;
  }

 private:
  /**
   * @brief Returns the number of bytes remaining in the buffer.
   *
   * @return Number of unread bytes in the current buffer.
   */
  [[nodiscard]] size_t buffer_remaining() const noexcept {
    return buffer_end_ - buffer_pos_;
  }

  /**
   * @brief Fills the internal buffer from the file.
   *
   * @throws std::runtime_error If read operation fails.
   */
  void fill_buffer() {
    file_.read(reinterpret_cast<char*>(buffer_.data()), buffer_.size());
    buffer_end_ = file_.gcount();
    buffer_pos_ = 0;
    file_pos_ = file_.tellg();

    if (file_.bad()) {
      throw std::runtime_error("Failed to read from file");
    }
  }

  /**
   * @brief Ensures the specified number of bytes are available in the buffer.
   *
   * @param bytes Number of bytes required.
   * @throws std::out_of_range If insufficient bytes remain in the file.
   * @throws std::runtime_error If read operation fails.
   */
  void ensure_available(size_t bytes) {
    if (buffer_remaining() < bytes) {
      // Move remaining bytes to start of buffer
      if (buffer_remaining() > 0) {
        std::memmove(buffer_.data(), &buffer_[buffer_pos_], buffer_remaining());
      }
      buffer_end_ = buffer_remaining();
      buffer_pos_ = 0;

      // Fill rest of buffer
      file_.read(reinterpret_cast<char*>(&buffer_[buffer_end_]),
                 buffer_.size() - buffer_end_);
      buffer_end_ += file_.gcount();
      file_pos_ = file_.tellg();

      if (file_.bad()) {
        throw std::runtime_error("Failed to read from file");
      }
    }

    if (buffer_remaining() < bytes) {
      throw std::out_of_range("Cannot read " + std::to_string(bytes) +
                              " bytes: reached end of file");
    }
  }

  /**
   * @brief Determines the native endianness of the system.
   *
   * @return The system's native endianness.
   */
  static constexpr Endianness native_endian() noexcept {
    return std::endian::native == std::endian::little ? Endianness::kLittle
                                                      : Endianness::kBig;
  }

  std::ifstream file_;           ///< The underlying file stream.
  std::vector<uint8_t> buffer_;  ///< Internal buffer for efficient reading.
  size_t file_size_ = 0;         ///< Total size of the file in bytes.
  size_t file_pos_ = 0;          ///< Current position in the file.
  size_t buffer_pos_ = 0;        ///< Current position within the buffer.
  size_t buffer_end_ = 0;        ///< Number of valid bytes in the buffer.
};

/**
 * @brief Type alias for little-endian memory reader.
 */
using LEMemoryReader = MemoryReader<Endianness::kLittle>;

/**
 * @brief Type alias for big-endian memory reader.
 */
using BEMemoryReader = MemoryReader<Endianness::kBig>;

/**
 * @brief Type alias for little-endian file reader.
 */
using LEFileReader = FileReader<Endianness::kLittle>;

/**
 * @brief Type alias for big-endian file reader.
 */
using BEFileReader = FileReader<Endianness::kBig>;

}  // namespace intns::io

#endif  // INTNS_IO_BINARY_READER_HPP