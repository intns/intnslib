#ifndef INTNS_MEMORY_OBJECTPOOL_HPP
#define INTNS_MEMORY_OBJECTPOOL_HPP

#include <cassert>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace intns::memory {
/**
 * @brief Concept to check if a type supports the acquire hook.
 * Type `T` must implement `Policy::on_acquire(T&)`, which is `noexcept`.
 * @tparam T The type to check for the acquire hook.
 */
template <typename Policy, typename T>
concept AcquireHook = requires(T& obj) {
  { Policy::on_acquire(obj) } noexcept;
};

/**
 * @brief Concept to check if a type supports the release hook.
 * Type `T` must implement `Policy::on_release(T&)`, which is `noexcept`.
 * @tparam T The type to check for the release hook.
 */
template <typename Policy, typename T>
concept ReleaseHook = requires(T& obj) {
  { Policy::on_release(obj) } noexcept;
};

/**
 * @brief A no-operation pool policy for resource management.
 *
 * This policy provides empty implementations for acquire and release hooks,
 * intended for use in scenarios where no special actions are required when
 * acquiring or releasing resources from a pool.
 *
 * @tparam U The type of resource managed by the pool.
 */
template <typename U>
struct NoOpPoolPolicy {
  static void on_acquire(U&) noexcept {}
  static void on_release(U&) noexcept {}
};

/**
 * @brief A thread-safe object pool for managing reusable objects of type T.
 *
 * ObjectPool efficiently manages reusable objects, reducing allocation costs.
 * It supports size limits, customizable policies, and ensures exception safety.
 *
 * @tparam T The type of objects managed (default and move constructible).
 * @tparam AcquirePolicy Class with static on_acquire(T&) called on acquisition.
 * @tparam ReleasePolicy Class with static on_release(T&) called on release.
 *
 * @note All public methods are thread-safe.
 *
 * @section Usage
 * Use take() or try_take() to acquire objects.
 * Use add() or try_add() to return objects.
 * Construct the pool with an initial size and optional size limit.
 *
 *@section Exception Safety
 * Construction throws std::runtime_error if initial size exceeds limit.
 * Methods throw std::runtime_error if pool is empty or size limit is exceeded.
 * Exceptions during object creation or policy methods are propagated.
 */
template <typename T, typename AcquirePolicy = NoOpPoolPolicy<T>,
          typename ReleasePolicy = AcquirePolicy>
  requires AcquireHook<AcquirePolicy, T> && ReleaseHook<ReleasePolicy, T>
class ObjectPool {
  static_assert(std::is_default_constructible_v<T>,
                "T must be default constructible");
  static_assert(std::is_move_constructible_v<T>,
                "T must be move constructible");

 public:
  using queue_type = std::deque<T>;
  using size_type = size_t;
  using value_type = T;

  ObjectPool() = default;
  ~ObjectPool() = default;

  /**
   * @brief Constructs an ObjectPool by moving the given queue of objects.
   *
   * @param src The source queue containing objects to be managed by the pool.
   * Ownership of the objects is transferred to the pool.
   */
  ObjectPool(queue_type&& src) : objects_(std::move(src)) {}

  /**
   * @brief Creates an ObjectPool with a set initial size and optional max
   * limit. Objects are default-constructed via `value_type` and processed by
   * `ReleasePolicy::on_release` upon creation. If creation fails, the queue
   * resets and the exception propagates.
   *
   * @param init_size The number of objects to initially create in the pool.
   * @param limit The maximum number of objects allowed in the pool (optional,
   * default is 0 for unlimited).
   * @throws std::runtime_error If `init_size` is greater than the specified
   * size limit.
   * @throws Any exception thrown during object creation or by
   * `ReleasePolicy::on_release`.
   */
  ObjectPool(size_type init_size,
             std::optional<size_type> limit = std::nullopt);

  /**
   * @brief Removes and returns an object from the pool.
   *
   * @return value_type The acquired object from the pool.
   * @throws std::runtime_error If the pool is empty.
   */
  [[nodiscard]] value_type take();

  /**
   * @brief Attempts to take an object from the container.
   *
   * @return std::optional<value_type> The acquired object if available,
   * otherwise std::nullopt.
   */
  [[nodiscard]] std::optional<value_type> try_take();

  /**
   * @brief Adds a new object to the pool.
   *
   * @param back The object to add to the pool (rvalue reference).
   * @throws std::runtime_error If the pool size limit has been reached.
   */
  void add(value_type&& back);

  /**
   * @brief Attempts to add a new object to the container.
   *
   * @param back The object to be added, passed as an rvalue reference.
   * @return true if the object was successfully added; false if the size limit
   * was reached.
   */
  [[nodiscard]] bool try_add(value_type&& back);

  /**
   * @brief Reserves space for a minimum number of objects in the pool.
   *
   * @param target_size Minimum number of elements to reserve space for.
   * @throws std::runtime_error if the target size is zero and `ThrowOnError`
   * is enabled.
   */
  void reserve(size_type target_size) { reserve_impl<true>(target_size); }

  /**
   * @brief Reserves space for a minimum number of objects in the pool without
   * throwing exceptions.
   *
   * @param target_size Minimum number of elements to reserve space for.
   * @return true if the reservation was successful; false otherwise.
   */
  [[nodiscard]] bool try_reserve(size_type target_size) {
    return reserve_impl<false>(target_size);
  }

  /**
   * @brief Reduces the memory usage of the internal object storage to fit its
   * current size. This may free unused memory and optimize resource usage.
   */
  void shrink_to_fit() {
    std::scoped_lock<std::mutex> lock(mutex_);
    objects_.shrink_to_fit();
  }

  /**
   * @brief Returns the pool's maximum capacity without reallocation.
   * @return Maximum objects before reallocation.
   */
  size_type capacity() const {
    std::scoped_lock<std::mutex> lock(mutex_);
    return objects_.capacity();
  }

  /**
   * @brief Returns the number of objects currently managed.
   * @return The number of objects managed.
   */
  [[nodiscard]] size_type size() const {
    std::scoped_lock<std::mutex> lock(mutex_);
    return objects_.size();
  }

  /**
   * @brief Checks if the object pool is empty.
   * @return true if the pool contains no objects, false otherwise.
   */
  [[nodiscard]] bool empty() const {
    std::scoped_lock<std::mutex> lock(mutex_);
    return objects_.empty();
  }

  /**
   * @brief Returns the maximum allowed size for the container.
   *
   * @return The size limit as a value of type size_type.
   */
  [[nodiscard]] std::optional<size_type> size_limit() const noexcept {
    std::scoped_lock<std::mutex> lock(mutex_);
    return size_limit_;
  }

  /**
   * @brief Sets the max pool size; nullopt makes it unlimited.
   * @param limit The new size limit.
   */
  void set_size_limit(std::optional<size_type> limit) noexcept {
    std::scoped_lock<std::mutex> lock(mutex_);
    size_limit_ = limit;
  }

 private:
  /**
   * @brief Ensures the object pool contains at least the specified number of
   * objects.
   *
   * Reserves space for `target_size` objects; Succeeds if capacity allows, else
   * fails or throws. Rolls back on failure.
   *
   * @param target_size The minimum number of objects to reserve in the pool.
   *
   * @return If `ThrowOnError` disabled, true on success; false on failure.
   * @throws If `ThrowOnError` enabled, throws `std::runtime_error` on failure.
   */
  template <bool ThrowOnError>
  [[nodiscard]] bool reserve_impl(size_type target_size);

  // The internal queue object
  queue_type objects_;

  // Mutex for thread safety
  mutable std::mutex mutex_;

  // Customizable size limit object, unlimited unless specified
  std::optional<size_type> size_limit_ = std::nullopt;
};

/**
 * @brief RAII wrapper for managing objects leased from a Pool.
 *
 * PoolLease acquires a Pool object on creation and releases it on destruction
 * unless explicitly released; guarantees exception safety and prevents leaks.
 *
 * @tparam Pool The ObjectPool providing value_type, take(), and add() methods.
 */
template <typename Pool>
class PoolLease {
  using value_type = typename Pool::value_type;

 public:
  /**
   * @brief Creates a PoolLease and acquires an object from the given pool.
   * @param p Reference to the Pool to lease from.
   */
  PoolLease(Pool& p) : pool_(&p), obj_(p.take()) {}

  /**
   * @brief Move constructor for PoolLease.
   * Transfers ownership and deactivates the source to prevent double release.
   *
   * @param o The PoolLease instance to move from.
   */
  PoolLease(PoolLease&& o) noexcept
      : pool_(o.pool_), obj_(std::move(o.obj_)), active_(o.active_) {
    o.active_ = false;
  }

  PoolLease(const PoolLease&) = delete;
  PoolLease& operator=(const PoolLease&) = delete;

  /**
   * @brief Destructor for the PoolLease class.
   */
  ~PoolLease() {
    if (active_) {
      pool_->add(std::move(obj_));
    }
  }

  /**
   * @brief Returns a reference to the internal object.
   * @return Reference to the stored object.
   */
  [[nodiscard]] auto& get() noexcept { return obj_; }

  /**
   * @brief Provides pointer-like access to the underlying object.
   * @return A pointer to the managed object.
   */
  [[nodiscard]] value_type* operator->() noexcept { return &obj_; }

  /**
   * @brief Returns a constant reference to the managed object.
   * @return const reference to the object stored in the pool.
   */
  [[nodiscard]] const auto& get() const noexcept { return obj_; }

  /**
   * @brief Provides pointer-like access to the underlying object.
   * @return A constant pointer to the managed object.
   */
  [[nodiscard]] const value_type* operator->() const noexcept { return &obj_; }

  /**
   * @brief Releases ownership of the managed object.
   *
   * Marks the object inactive and returns it, transferring ownership;
   * it is no longer considered active.
   *
   * @return value_type The managed object, moved out of this instance.
   * @note This function is noexcept and guarantees not to throw exceptions.
   */
  [[nodiscard]] value_type release() noexcept {
    active_ = false;
    return std::move(obj_);
  }

 private:
  // Pointer to the pool from which the object was leased
  Pool* pool_;

  // The object being managed by this lease
  value_type obj_;

  // Indicates whether the lease is still active
  bool active_ = true;
};

}  // namespace intns::memory

#include "ObjectPool.tpp"

#endif  // NSLIB_MEMORY_OBJECTPOOL_HPP
