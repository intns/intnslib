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

int main(int argc, char** argv) {
  _CRT_UNUSED(argc);
  _CRT_UNUSED(argv);

  using namespace intns::memory;
  {
    ObjectPool<Object> pool;
    pool.add(Object(1, "Object 1"));

    {
      PoolLease lease(pool);
      lease->s = "Modified Object";
      std::cout << "Leased object value: " << lease->value << std::endl;
    }
  }
  {
    ObjectPool<Object, ObjectPolicy> pool_with_policy;
    pool_with_policy.add(Object(2, "Object 2"));

    {
      PoolLease lease(pool_with_policy);
      lease->s = "Modified Object with Policy";
      std::cout << "Leased object value with policy: " << lease->value << std::endl;
    }
  }

  return 0;
}
