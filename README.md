# intnslib

Functionality shared across projects

## Features

### Memory

- Thread-safe [ObjectPool](https://intns.github.io/intnslib/classintns_1_1memory_1_1ObjectPool.html) including customizable acquire/release policies, throwing and non-throwing operation variants, and a 'leasing' RAII wrapper for automatic resource management.
- A fast, linear, [StackAllocator](https://intns.github.io/intnslib/classintns_1_1memory_1_1StackAllocator.html) for temporary allocations, with a scoped RAII wrapper for working within 'frames'.
