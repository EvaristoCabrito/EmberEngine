#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace ember {

template <class Signature, size_t InlineBytes = 48>
class InlineFunction;

// Move-only callable that stores small captures inline, so queuing a job or an I/O callback never
// touches the heap. Captures larger than `InlineBytes` fall back to one heap allocation.
template <class R, class... Args, size_t InlineBytes>
class InlineFunction<R(Args...), InlineBytes> {
 public:
  InlineFunction() = default;

  template <class F>
    requires(!std::is_same_v<std::remove_cvref_t<F>, InlineFunction> && std::is_invocable_r_v<R, F&, Args...>)
  InlineFunction(F&& f) {  // NOLINT(google-explicit-constructor): lambdas convert implicitly, like std::function
    using Fn = std::remove_cvref_t<F>;
    if constexpr (fitsInline<Fn>()) {
      new (storage_) Fn(std::forward<F>(f));
      ops_ = &kInlineOps<Fn>;
    } else {
      *reinterpret_cast<Fn**>(storage_) = new Fn(std::forward<F>(f));
      ops_ = &kHeapOps<Fn>;
    }
  }

  InlineFunction(InlineFunction&& other) noexcept : ops_(other.ops_) {
    if (ops_) ops_->move(storage_, other.storage_);
    other.ops_ = nullptr;
  }

  InlineFunction& operator=(InlineFunction&& other) noexcept {
    if (this != &other) {
      reset();
      ops_ = other.ops_;
      if (ops_) ops_->move(storage_, other.storage_);
      other.ops_ = nullptr;
    }
    return *this;
  }

  InlineFunction(const InlineFunction&) = delete;
  InlineFunction& operator=(const InlineFunction&) = delete;
  ~InlineFunction() { reset(); }

  R operator()(Args... args) { return ops_->call(storage_, std::forward<Args>(args)...); }
  explicit operator bool() const { return ops_ != nullptr; }

  void reset() {
    if (ops_) ops_->destroy(storage_);
    ops_ = nullptr;
  }

 private:
  struct Ops {
    R (*call)(void*, Args&&...);
    void (*move)(void* dst, void* src);  // move-constructs into dst and destroys src
    void (*destroy)(void*);
  };

  template <class Fn>
  static constexpr bool fitsInline() {
    return sizeof(Fn) <= InlineBytes && alignof(Fn) <= alignof(std::max_align_t) &&
           std::is_nothrow_move_constructible_v<Fn>;
  }

  template <class Fn>
  static constexpr Ops kInlineOps{
      [](void* s, Args&&... args) -> R { return (*static_cast<Fn*>(s))(std::forward<Args>(args)...); },
      [](void* dst, void* src) {
        new (dst) Fn(std::move(*static_cast<Fn*>(src)));
        std::destroy_at(static_cast<Fn*>(src));
      },
      [](void* s) { std::destroy_at(static_cast<Fn*>(s)); }};

  template <class Fn>
  static constexpr Ops kHeapOps{
      [](void* s, Args&&... args) -> R { return (**static_cast<Fn**>(s))(std::forward<Args>(args)...); },
      [](void* dst, void* src) { *static_cast<Fn**>(dst) = *static_cast<Fn**>(src); },
      [](void* s) { delete *static_cast<Fn**>(s); }};

  alignas(std::max_align_t) std::byte storage_[InlineBytes];
  const Ops* ops_ = nullptr;
};

}  // namespace ember
