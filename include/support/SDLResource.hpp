#pragma once

template <typename T, auto DestroyFn> class SDLResource {
  T *_ptr{nullptr};

public:
  // Empty constructor
  SDLResource() = default;

  // Initialized constructor
  explicit SDLResource(T *ptr) : _ptr{ptr} {}

  // Destructor
  ~SDLResource() { reset(); }

  // Move constructor/move assignment operator
  SDLResource(SDLResource &&other) noexcept : _ptr{other.release()} {}

  SDLResource &operator=(SDLResource &&other) noexcept {
    if (this == &other)
      return *this;
    reset(other.release());
    return *this;
  }

  // Copy constructor/copy assignment operator
  SDLResource(const SDLResource &) = delete;
  SDLResource &operator=(const SDLResource &) = delete;

  // Pointer-like operators
  T &operator*() const { return *_ptr; }

  T *operator->() const { return _ptr; }

  explicit operator bool() const { return _ptr != nullptr; }

  T *get() const { return _ptr; }

  T *release() {
    T *ptr = _ptr;
    _ptr = nullptr;
    return ptr;
  }

  void reset(T *next = nullptr) {
    if (_ptr == next)
      return;
    if (_ptr)
      DestroyFn(_ptr);
    _ptr = next;
  }
};
