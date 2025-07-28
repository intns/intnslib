#include <iostream>
#include <string>

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
  int* allocated = stack.alloc_t<int>();
}

int main(int argc, char** argv) {
  _CRT_UNUSED(argc);
  _CRT_UNUSED(argv);

  test_object_pool();
  test_stack_allocator();

  return 0;
}
