#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "intns/io.hpp"
#include "intns/memory.hpp"

#ifndef _CRT_UNUSED
#define _CRT_UNUSED(x) (void)x
#endif

struct Object {
  int value;
  std::string s;

  Object() : value(0), s("") {}
  explicit Object(int v, const std::string& str) : value(v), s(str) {}
};

struct ObjectPolicy {
  static void on_acquire(Object& obj) noexcept {
    std::cout << "Acquired object with value: " << obj.value << std::endl;
  }

  static void on_release(Object& obj) noexcept {
    std::cout << "Released object with value: " << obj.value << std::endl;
  }
};

void test_object_pool() {
  using namespace intns::memory;

  // Test with default policies
  {
    ObjectPool<Object> pool;
    pool.add(Object(1, "Object 1"));

    {
      PoolLease lease(pool);
      lease->s = "Modified Object";
      std::cout << "Leased object value: " << lease->value << std::endl;
    }
  }

  // Test with custom policies
  {
    ObjectPool<Object, ObjectPolicy> pool_with_policy;
    pool_with_policy.add(Object(2, "Object 2"));

    {
      PoolLease lease(pool_with_policy);
      lease->s = "Modified Object with Policy";
      std::cout << "Leased object value with policy: " << lease->value << std::endl;
    }
  }
}

void test_stack_allocator() {
  using namespace intns::memory;
  StackAllocator stack(4);
  {
    // Unused value to prevent unused variable warning
    void(stack.alloc_t<int>());
  }
}

void test_memory_reader() {
  using namespace intns::io;

  std::vector<uint8_t> buffer = {0x01, 0x02, 0x03, 0x04};
  LEMemoryReader reader(buffer);

  uint32_t value = reader.read_u32();
  std::cout << "Read value: " << std::hex << value << std::endl;

  if (value == 0x04030201) {
    std::cout << "MemoryReader works correctly with little-endian data."
              << std::endl;
  } else {
    std::cout << "MemoryReader did not read the expected value." << std::endl;
  }
}

void test_file_reader() {
  using namespace intns::io;

  // Create test.bin
  if (!std::filesystem::exists("test.bin")) {
    std::ofstream ofs("test.bin", std::ios::binary);
    if (!ofs) {
      std::cerr << "Failed to create test.bin" << std::endl;
      return;
    }

    uint32_t test_value = 0x12345678;
    ofs.write(reinterpret_cast<const char*>(&test_value), sizeof(test_value));
  }

  try {
    LEFileReader file_reader("test.bin");
    uint32_t value = file_reader.read_u32();
    std::cout << "Read value from file: " << std::hex << value << std::endl;

    if constexpr (std::endian::native == std::endian::little) {
      if (value == 0x12345678) {
        std::cout << "FileReader works correctly with little-endian data."
                  << std::endl;
      } else {
        std::cout << "FileReader did not read the expected value." << std::endl;
      }
    } else {
      if (value == 0x78563412) {
        std::cout << "FileReader works correctly with big-endian data."
                  << std::endl;
      } else {
        std::cout << "FileReader did not read the expected value." << std::endl;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error reading file: " << e.what() << std::endl;
  }
}

int main(int argc, char** argv) {
  _CRT_UNUSED(argc);
  _CRT_UNUSED(argv);

  test_object_pool();
  test_stack_allocator();
  test_memory_reader();
  test_file_reader();

  return 0;
}
