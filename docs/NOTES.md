# C++ study notes

For the project-specific changes since `5d95005`, see
[CATCH_UP.md](CATCH_UP.md): old-to-new responsibilities, layout and rendering math,
ownership, scheduling, and a reading route through the current implementation.

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
void setBounds(playground::math::Rect value) {
  _bounds = value;
  _surfaceDirty = true;
}
```

Group values that change together into a property struct. Use a patch struct
when callers should be able to change only selected fields without rebuilding
the entire configuration.

## Retained UI composition

The chosen direction for the layout work is a retained UI tree: construct nodes,
keep their state alive, and update their properties or children explicitly. A
nested construction syntax does not require rebuilding descriptions every frame.
Reconciliation can be added later as a producer of those same tree updates; it
is not required for selective updates, cached layout, or virtualization.

Keep three kinds of data separate:

- Properties describe what is requested: padding, sizing rules, text, etc.
- Local state remembers interaction: focus, capture, selection, scroll offset.
- Derived data records calculated results: measured size, arranged bounds,
  rasterized content, and cached drawing commands.

There need not be a second persistent description tree alongside the live tree.
Properties stored on a live node can be its authoritative description.

## Properties versus composition boundaries

Configure the same object with properties. Introduce another node when another
independently configurable boundary is needed. For example, padding inside one
box is a property; padding around an already-sized box can be another box.

Repeated assignments replace a property on that object. Repeated wrappers nest
and retain both operations. Do not silently turn a setter into a wrapper just
because the property was already specified.

Construction and later mutation should use the same property validation and
invalidation rules. Expose properties for inspection through const access;
route changes through setters or patches so derived data cannot silently become
stale. An omitted patch field means "leave unchanged," not "reset to default."
Resetting an optional value needs a separate, explicit representation.

Use `Patch<T>` for a requested change: `Keep`, `Set(value)`, or `Reset`.
`Reset` uses the property's documented baseline, not its previous value and not
an implicit theme lookup. For an optional property, explicitly setting nullopt
clears it; that differs from resetting when the baseline contains a value.
`Default<T>` would describe a fallback value, not these three update operations.

Name placement by its direction: `placementInParent` means this node's
relationship to its parent; `childrenAlignment` means how this container aligns
its children. Parent-specific types such as `StackPlacement` and `GridPlacement`
prevent unrelated rules from becoming one ambiguous property bag.

The current vocabulary, behavioral contracts, and deferred extensions live in
[UI reference](ui/REFERENCE.md). The real Minesweeper UI is the retained composition example;
[CONTRIBUTING.md](../CONTRIBUTING.md) explains small, focused module tests.

## Invalidation is not reconciliation

Invalidation means some derived result is no longer usable. Reconciliation means
matching a new description against existing instances to decide what to change.
They solve different problems.

A retained tree can mark measurement, placement, drawing, or hit-test data dirty
without recreating its nodes. A cached measurement is reusable only when its
inputs still match, including constraints and relevant content revisions. A
clean child may still need measurement when its parent offers a new width.

Changes that affect a child's measured size may affect its parent and siblings.
Changes to parent transforms or clipping can affect descendants. Propagate dirtiness
according to dependencies, not just downward through the ownership tree.

Skipping measurement or rasterization does not necessarily mean skipping drawing.
If the frame target is cleared, unchanged visible content must still be drawn or
copied back from a cache. Skipping presentation also needs to account for window
exposure, resizing, animations, and other reasons the output becomes invalid.

## Persistent nodes and temporary storage

A vector of unique_ptrs can grow without moving the pointed-to nodes, although
references to the vector's pointer elements can be invalidated. Removing a node
still destroys it and invalidates borrowed pointers to that node.

For long-lived external references, an index plus generation can identify a live
node without making it shared-owned. A generation check detects a deleted slot
that has since been reused; it does not extend the node's lifetime or make access
thread-safe. A model item's stable key is a different identity from a live node's
allocation handle, especially when list items are virtualized.

Use persistent allocation for live nodes and reusable storage for temporary
layout work. An arena reset releases storage, not the C++ object lifetimes by
itself: destroy objects/containers that use that storage before resetting it,
and do not retain pointers into it afterward.

## Layout geometry and render adapters

A layout rectangle is a calculated position and size, not a general transform.
Keep it separate from visual translation/rotation/scaling and from the camera
that might later project an entire UI panel into 3D.

Keep backend-independent geometry and sizing rules independent from SDL types.
SDL conversion, pixel rounding, and SDL-specific formatting belong at the
backend boundary. An alias such as `using Vec2f = SDL_FPoint` is still an SDL
type, not a distinct type with independent ownership or operator lookup.

An interface's public, non-virtual operation can manage validation and caching,
then call protected virtual hooks for the customizable behavior. This avoids
requiring every derived class to remember mandatory base-class bookkeeping.

## Events and frame flow

An SDL user event containing a pointer has not copied the pointed-to value.
If a queued event outlives a cell/grid, that pointer dangles. Queue a value
snapshot or a checked handle/generation instead. A synchronous borrowed pointer
can be fine; the time boundary is what changes the lifetime requirements.

In the retained UI, capture, target, and bubble are routing phases, while a
button's default activation is a separate action. stopPropagation stops routing;
preventDefault suppresses the action. Defer structural edits until dispatch
finishes, so a callback cannot destroy the node whose member function is running.

Keep the Connection returned by a subscription. Its destructor disconnects it;
discarding that temporary disconnects immediately. Likewise, a timer handle owns
the registration, not the object referenced inside its callback. Capture checked
node handles when the callback may outlive a node.

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
