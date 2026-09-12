# C++ study notes

## Prefer uniform initialization

Prefer `{}` over `=` for most initialization:

```cpp
int pi = 3.14;    // Implicitly truncates the value.
int pi{3.14};     // Fails: narrowing conversion is rejected.
float pi{3.14};   // Works: the constant expression fits in a float.
```

`3.14` has type `double`, while `3.14f` has type `float`. A constant
expression may still be used in list-initialization when its value can be
represented by the destination type; list-initialization rejects narrowing
conversions that could silently lose information.

## Structured bindings

Structured bindings unpack an object into named variables. They work with
aggregate-like types such as vectors and with many standard-library containers
and tuple-like objects:

```cpp
Vec3 someVec{0.0, 0.0, 0.0};
auto [x, y, z] = someVec;
```

The binding must account for every element in the object. Use references when
the bindings should refer to the original object instead of making a copy:

```cpp
auto &[x, y, z] = someVec;
```

## Reserve known container capacity

When the expected capacity is known, reserve it before inserting elements:

```cpp
std::vector<int> list{0, 1};
list.reserve(100);
```

`reserve` allocates capacity without changing the vector's logical size. It
helps avoid repeated reallocations, but it does not create 100 initialized
elements; use `resize` when elements themselves must exist.

## References, pointers, and ownership

Use a reference when an object is required and the function does not take
ownership:

```cpp
void draw(const Surface &surface); // Required, borrowed, read-only.
void update(State &state);          // Required, borrowed, mutable.
```

Use a pointer when null is meaningful, pointer arithmetic is needed, or the
API is explicitly expressing an address/handle:

```cpp
void draw(const Surface *surface); // May be null; not owned here.
```

Raw pointers do not communicate ownership. Prefer `std::unique_ptr<T>` for
exclusive ownership and `std::shared_ptr<T>` only when shared lifetime is
actually required. `get()` borrows the managed pointer; it does not create a
new owner. `release()` gives up ownership and requires the caller to manage the
returned pointer.

## RAII and non-copyable resources

Resource Acquisition Is Initialization (RAII) ties cleanup to scope:

```cpp
{
  SDLResource<SDL_Surface, SDL_DestroySurface> surface{createdSurface};
  // use surface
} // SDL_DestroySurface runs automatically
```

Delete copying when two objects must not destroy the same resource:

```cpp
Resource(const Resource &) = delete;
Resource &operator=(const Resource &) = delete;
```

Keep moving when ownership may transfer:

```cpp
Resource(Resource &&) noexcept = default;
Resource &operator=(Resource &&) noexcept = default;
```

`std::move` does not move anything by itself. It casts an object to an
rvalue so that a move constructor or move assignment operator can consume its
 state. The moved-from object remains valid but its value is generally
 unspecified.

## `const`, `constexpr`, `inline`, and `noexcept`

`const` prevents modification through that name. A `const` member function
promises not to modify the observable state of its object:

```cpp
int width() const;
```

`constexpr` permits compile-time evaluation when its inputs allow it. A
`constexpr` variable is also `const`:

```cpp
constexpr int cellSize{50};
```

`inline` on a function is primarily an ODR/linkage rule, not a command to the
compiler to substitute the function body. It allows the same definition in
multiple translation units, which is why header-defined functions and C++17
inline variables commonly use it. The optimizer decides whether to inline the
call.

`noexcept` states that a function will not let exceptions escape. It improves
the guarantees of move operations and allows containers to move elements more
efficiently, but it does not turn SDL error returns into success:

```cpp
T *get() noexcept; // must still return a valid value or a documented null
```

## Copy versus move

Copying duplicates a value or resource representation. Moving transfers the
resource and is usually preferable for owning objects:

```cpp
Thing a{source};            // copy constructor
Thing b{std::move(source)}; // move constructor
b = other;                  // copy assignment
b = std::move(other);       // move assignment
```

Pass small, cheap value types by value. Pass large read-only objects as
`const T&`, and pass ownership-consuming parameters by value or `T&&` with a
clear consuming contract.

## Enums and formatting

Prefer scoped enums for states and modes:

```cpp
enum class RenderMethod {
  Surface,
  Texture,
};
```

`enum class` prevents accidental conversion to integers and prevents enumerator
names from leaking into the surrounding scope. Use a `switch` to make state
handling explicit, and provide a formatter or `toString` helper at the module
boundary when the value must be logged or displayed.

## Properties and derived state

Store authoritative state once. Values such as bounds, pixel size, or a cached
surface size are derived state and should either be recomputed or invalidated
when their source properties change:

```cpp
void setTransform(RectTransform value) {
  _transform = value;
  _surfaceDirty = true;
}
```

Group values that change together into a property struct. Use a patch struct
when callers should be able to change only selected fields without rebuilding
the entire configuration.

## Events and frame flow

An event is input or a system notification; it is not persistent object state.
Keep event dispatch separate from per-frame update and rendering:

```text
poll SDL events
  -> dispatch events to active objects
  -> update(dt)
  -> render(surface)
```

An object may remember a result such as `hovered`, `pressed`, or `disabled`, but
the SDL event itself should not be stored as long-lived state unless it is
queued deliberately. Handle mouse release using the button/capture semantics
needed by the UI rather than assuming the release remains inside the button.

## SDL resource boundaries

SDL handles are opaque C pointers with library-specific destruction functions.
Wrap each owning handle with the correct deleter and keep borrowed views as
references or non-owning pointers:

```cpp
using SurfaceHandle = std::shared_ptr<SDL_Surface>;
```

The handle type alone does not prove that the correct SDL deleter was used;
centralize construction in an asset/resource registry. A registry can cache
fonts and surfaces by path plus relevant properties, then return shared handles
to display objects without duplicating loads.

When a cache has a size budget, track last-use time or frame number per entry.
Evict least-recently-used entries only when the budget is exceeded, while
letting active `shared_ptr` users keep their resources alive until they release
them.
