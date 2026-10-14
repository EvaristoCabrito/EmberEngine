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
      ops_ = inlineOps<Fn>();
    } else {
      *reinterpret_cast<Fn**>(storage_) = new Fn(std::forward<F>(f));
      ops_ = heapOps<Fn>();
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

  // Function templates and function-local tables: MSVC (C4268) can zero-fill a static data member
  // template instantiated for some lambda types, which would crash the first call.
  template <class Fn>
  static R callInline(void* s, Args&&... args) { return (*static_cast<Fn*>(s))(std::forward<Args>(args)...); }
  template <class Fn>
  static void moveInline(void* dst, void* src) {
    new (dst) Fn(std::move(*static_cast<Fn*>(src)));
    std::destroy_at(static_cast<Fn*>(src));
  }
  template <class Fn>
  static void destroyInline(void* s) { std::destroy_at(static_cast<Fn*>(s)); }
  template <class Fn>
  static R callHeap(void* s, Args&&... args) { return (**static_cast<Fn**>(s))(std::forward<Args>(args)...); }
  static void moveHeap(void* dst, void* src) { *static_cast<void**>(dst) = *static_cast<void**>(src); }
  template <class Fn>
  static void destroyHeap(void* s) { delete *static_cast<Fn**>(s); }

  template <class Fn>
  static const Ops* inlineOps() {
    static constexpr Ops ops{&callInline<Fn>, &moveInline<Fn>, &destroyInline<Fn>};
    return &ops;
  }
  template <class Fn>
  static const Ops* heapOps() {
    static constexpr Ops ops{&callHeap<Fn>, &moveHeap, &destroyHeap<Fn>};
    return &ops;
  }

  alignas(std::max_align_t) std::byte storage_[InlineBytes];
  const Ops* ops_ = nullptr;
};

}  // namespace ember
