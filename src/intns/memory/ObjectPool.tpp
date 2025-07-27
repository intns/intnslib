#include "ObjectPool.hpp"

namespace intns::memory {

template <typename T, typename A, typename R>
  requires AcquireHook<A, T> && ReleaseHook<R, T>
ObjectPool<T, A, R>::ObjectPool(size_type init_size,
                                std::optional<size_type> limit)
    : objects_(), size_limit_(limit) {
  if (limit.has_value() && init_size > limit.value()) [[unlikely]] {
    throw std::runtime_error(
        "ObjectPool::ObjectPool: initial size is > size limit.");
  }

  if (limit.has_value() && limit.value() == 0) {
    size_limit_ = std::nullopt;  // 0 can also mean "unlimited"
  }

  // Create initial objects, default-constructing them
  // And notifying the release policy
  for (size_type i = 0; i < init_size; i++) {
    objects_.emplace_back(value_type());
    R::on_release(objects_.back());  // No exception can be thrown here
  }
}

template <typename T, typename A, typename R>
  requires AcquireHook<A, T> && ReleaseHook<R, T>
typename ObjectPool<T, A, R>::value_type ObjectPool<T, A, R>::take() {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (objects_.empty()) [[unlikely]] {
    throw std::runtime_error("ObjectPool::take: queue is empty.");
  }

  // Move the last object from the queue
  // and notify the acquire policy
  value_type value = std::move(objects_.back());
  objects_.pop_back();
  A::on_acquire(value);  // No exception can be thrown here
  return value;
}

template <typename T, typename A, typename R>
  requires AcquireHook<A, T> && ReleaseHook<R, T>
std::optional<typename ObjectPool<T, A, R>::value_type>
ObjectPool<T, A, R>::try_take() {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (objects_.empty()) {
    return std::nullopt;
  }

  // Move the last object from the queue
  // and notify the acquire policy
  value_type value = std::move(objects_.back());
  objects_.pop_back();
  A::on_acquire(value);
  return value;
}

template <typename T, typename A, typename R>
  requires AcquireHook<A, T> && ReleaseHook<R, T>
void ObjectPool<T, A, R>::add(value_type&& back) {
  std::scoped_lock<std::mutex> lock(mutex_);

  if (size_limit_.has_value() && objects_.size() >= size_limit_.value()) {
    throw std::runtime_error(
        "ObjectPool::add: unable to add as size limit reached.");
  }

  // Notify the release policy before
  // adding the object to the pool
  R::on_release(back);
  objects_.push_back(std::move(back));
}

template <typename T, typename A, typename R>
  requires AcquireHook<A, T> && ReleaseHook<R, T>
bool ObjectPool<T, A, R>::try_add(value_type&& back) {
  std::scoped_lock<std::mutex> lock(mutex_);

  if (size_limit_.has_value() && objects_.size() >= size_limit_.value()) {
    return false;
  }

  // Notify the release policy before
  // adding the object to the pool
  try {
    R::on_release(back);
    objects_.push_back(std::move(back));
    return true;
  } catch (...) {
    return false;
  }
}

template <typename T, typename AcquirePolicy, typename ReleasePolicy>
  requires AcquireHook<AcquirePolicy, T> && ReleaseHook<ReleasePolicy, T>
template <bool ThrowOnError>
bool ObjectPool<T, AcquirePolicy, ReleasePolicy>::reserve_impl(
    size_type target_size) {
  if (target_size == 0) [[unlikely]] {
    if constexpr (ThrowOnError) {
      throw std::runtime_error("ObjectPool::reserve: target size is zero.");
    } else {
      return true;  // Nothing to reserve
    }
  }

  std::scoped_lock<std::mutex> lock(mutex_);
  const size_type current_size = objects_.size();

  if (current_size >= target_size) {
    return true;  // Already at or above target size
  }

  // Check size limit
  if (size_limit_.has_value() && target_size > size_limit_.value()) {
    if constexpr (ThrowOnError) {
      throw std::runtime_error(
          "ObjectPool::reserve: cannot reserve more than size limit.");
    } else {
      return false;
    }
  }

  const size_type to_create = target_size - current_size;
  objects_.reserve(target_size);

  // Batch create objects
  try {
    for (size_type i = 0; i < to_create; ++i) {
      objects_.emplace_back();
      ReleasePolicy::on_release(objects_.back());
    }
  } catch (...) {
    objects_.resize(current_size);  // Rollback on error

    if constexpr (ThrowOnError) {
      throw;  // Propagate the exception
    } else {
      return false;  // Indicate failure
    }
  }

  return true;
}

}  // namespace intns::memory