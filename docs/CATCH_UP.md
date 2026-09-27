# Catching up from the surface-based playground

This is a guided comparison with `5d95005fb5bc4b79718cde528c80fe97648dcd6a`,
the `tetrass -> playground` commit. It includes the current working-tree cleanup.
It is a learning route, not another specification to memorize. For exact API
contracts use [UI reference](ui/REFERENCE.md), [UI guide](ui/GUIDE.md), and
[rendering contracts](render/CONTRACTS.md).

The substantial changes really did introduce a new engine layer. This is not
just your old classes with names you haven't learned yet. But most of the new
machinery answers three familiar questions:

1. Who owns this state, and how long is it valid?
2. What can we derive from it, and when must we recompute that result?
3. At what point is it safe to apply the change?

Read sections 1–5 to resume application/UI work. Sections 6–9 explain the engine
and rendering math; they are not prerequisites for changing a button label.

## 1. What changed, and what you already knew

| Checkpoint | Main change |
|---|---|
| `5d95005` | AppHost, props, resource RAII, shared asset registry, manually positioned display objects and SDL surfaces |
| `561422b` | Retained UI/layout, runtime services, app migration, geometry boundaries, settings and backend-neutral interfaces |
| `0c0b5eb` | Software/GPU rendering, scenes, resource submission/completion, shaders, import, recovery and profiling |
| `0d916a0` | Telemetry and scene-boundary corrections |
| Current cleanup | Classified frame failures, tested host sequencing, creation policy, branch invalidation and private writable targets |

RAII, unique ownership, shared cached assets, props/patches, the registry's LRU
clock and deferred app requests were already in your baseline. The genuinely new
parts are a retained layout/runtime protocol and separation of drawing intent
from native execution. We did not replace C++ with a new programming paradigm.

### Old responsibilities → current locations

Some responsibilities split across several types; these are not drop-in renames.

| At your checkpoint | Now | Important difference |
|---|---|---|
| `IDisplayObject` held transform, visibility, drawing and input | [Node](../include/ui/Node.hpp), [NodeProps](../include/ui/NodeProps.hpp), parent container | Bounds are a layout result, not usually an authored absolute rectangle |
| `IDrawable::render(SDL_Surface&)` | `Node::paint(PaintContext&)`, public `Node::render` | The base orchestrates transforms, clipping, decoration and descendants |
| `IInteractable`, `InteractionState` and manual child loops | [UIRoot](../include/ui/UIRoot.hpp), `UIEvent`, [Button](../include/ui/controls/Button.hpp) | Central hit testing, capture/target/bubble routing, pointer capture and focus |
| `ITickable` | `IRuntimeObject::update`, `UIRoot::update`, `Scheduler` | No replacement tick interface on every node; use the applicable runtime boundary |
| `DrawableSurface` fit/blit/appearance responsibilities | [ContentTypes](../include/ui/content/ContentTypes.hpp), [PaintImage](../include/rendering/PaintImage.hpp), [PaintContext](../include/rendering/PaintContext.hpp) | Placement, immutable image data and drawing are separate |
| `DisplayImage` + `ImageSurface` | `ui::Image` + `PaintImageHandle`; SDL source via `SurfacePaintImage` | The node chooses layout/content fitting; backend prepares compatible pixels |
| `DisplayVector` + `VectorSurface` | `ui::Vector`, `SVGDocument`, AssetRegistry | SVG rasterization follows required density; native Path drawing is a separate API |
| `DisplayText` + `TextSurface` | `ui::Text`, `TextFlow`, `Font`, optional `TextImagePreparer` | Measure/flow/fit first, then prepare pixels or atlas-based output |
| `ui/Rectangle.hpp` | `ui/content/Rectangle.hpp`, or a node's background/border style | A colored box need not be a separate owned surface |
| Button icon/label display-object members | Button content composed from child nodes | Reuse Box/Stack/Image/Text/Vector rather than specialized icon positioning |
| `support/SDLPrimitives.hpp` | `math/*`, `layout/*`, `platform/sdl/SDLGeometry.hpp` and formatters | Project values no longer alias native SDL geometry throughout the engine |
| `RectTransform` | `math::Rect` and `math::Transform2D` | A rectangle describes a box; an affine transform maps coordinates |
| `Vec2i = SDL_Point`, `Vec2f = SDL_FPoint` | Project `math::Vec2<int/float>`, `Point2`, `Size2` | Vector, point and extent have explicit roles; SDL conversion occurs at adapters |
| `IRuntimeObject::render(ctx, surface)` | `render(ctx, RenderFrame&)` | Apps borrow frame services; AppHost owns presentation |
| `IApp`, `AppContext`, `AppHost` | Same app boundary, expanded contracts | Preferred-content measurement, renderer-change notification, settings, transactions and recovery |
| Window config/state | Window metadata + view policy + presentation preferences + observed state | Requested behavior is not the same thing as what the OS actually did |
| `Demo2DUI` vector of absolute-position buttons | [makeDemo2DUI](../include/demo2d/Demo2DUI.hpp) returning a Box/ZStack tree | Composition describes relationships instead of manually computing each position |
| Minesweeper UI/Grid/Cell | Same app-specific types using Box/Grid/Button nodes | Game responsibilities remain app-specific; layout/input machinery is shared |
| AssetRegistry/Font/SurfaceHandle/SDLResource | Still present | Extended resources and compiled implementations; not superseded by GPU caches |
| PerformanceMonitor CPU phase durations | CPU phases plus optional asynchronous GPU samples | Submission time, execution duration and completion latency are different measurements |

To inspect an old file without altering your tree:

```powershell
git show 5d95005:include/ui/DisplayText.hpp
git diff 5d95005 -- include/app/IApp.hpp
```

### The ownership picture

```text
AppHost owns window, settings, assets, backend and the active IApp
  IApp owns application state and a UISession
    UISession owns UIRoot and maps SDL/window coordinates
      UIRoot owns one Node tree, identity table and runtime services
        containers own children via unique_ptr<Node>
        content nodes retain shared source handles

AppHost borrows one frame from its backend
  frame lends PaintContext and optional SceneRenderer
  backend owns native realizations and queued GPU work
```

That last branch is intentionally not inside the Node inheritance tree. A button
does not need to become a Vulkan object to be drawn using Vulkan.

## 2. Three sorts of state, not hundreds of independent concepts

Consider a text label:

| State | Examples | Authority |
|---|---|---|
| Authored | String, font request, box padding, wrapping/fit policy | App and node props |
| Derived | Measured size, chosen fitted font, arranged content box, resolved crop | Layout/content implementation |
| Realized | CPU raster, uploaded texture, GPU glyph data, cached layer | Resource preparation/backend |

Changing the string should invalidate dependent results. Moving the label should
not reload its font file. Changing the GPU device should invalidate its GPU
representation, not its string or game state.

This is the purpose of revisions, dirty flags, cache keys and resource domains.
They are different answers to “is this cached result still usable?”:

- A **revision** identifies a version of authored input.
- **Dirty flags** identify work that must be reconsidered: measure, arrange, paint,
  hit testing or semantics.
- A **cache key** includes every relevant input, not just the object address.
  Node measurement also depends on offered constraints, direction, density and
  environment revision.
- A **resource domain** identifies compatible realizations, especially one GPU
  device lifetime. It is not a completion counter.

Setters validate a candidate, compare it, publish the change, then invalidate the
appropriate results. This is your old getter/setter-plus-side-effects pattern,
but with a defined protocol for notifying the rest of the tree.

### Props are values; patches are operations

`Props` describe the desired state. `Patch<T>` is `variant<Keep, T, Reset>`:

- Keep: do not alter this property.
- Set(value): explicitly assign it, including false, zero or an empty value.
- Reset: use the documented baseline, not a remembered previous value.

For `Patch<optional<T>>`, Set(nullopt) means “clear this optional value”; it is
not Keep. Scene patches currently use optional overrides rather than Reset.
There is no benefit in forcing those distinct contracts into one clever template.

This resembles an explicit TypeScript discriminated union. `std::variant` stores
one alternative plus its tag; it is not itself a heap-allocated class hierarchy.
Its chosen value may still allocate internally, for example a string.

## 3. Measure → arrange → prepare → paint

Read [Node.cpp](../src/ui/Node.cpp), then
[Stack.cpp](../src/ui/containers/Stack.cpp). Do not start with the entire Node header.

| Stage | Question | Output |
|---|---|---|
| Measure | Given these permitted sizes, what size would you choose? | Finite size and optional text baselines |
| Arrange | Here is your actual rectangle; where do your children belong? | Child rectangles and content/overflow bounds |
| Prepare | With those bounds and this pixel density/backend, what resources are needed? | Ready images, fitted text, scene output |
| Paint | What should be drawn in local coordinates? | Pixels on software, or recorded native drawing work |

Measure is not a command to resize the window. Arrange is not a command to scale
an image. Prepare is not permission for a worker to mutate the live UI tree.
Paint is not the place to restructure that tree.

Constraints travel down; measurements return up; final rectangles travel down.
This describes data flow, not a guarantee of exactly two tree walks. A container
may remeasure after assigning width—text height depends on width. Current stack
planning happens during both measurement and arrangement; compatible Node
measurement results are cached.

### Start with one axis

An `AxisConstraints` is an interval:

```text
minimum <= chosen extent <= maximum
```

An absent maximum means unbounded, not zero. `tight(120)` means both limits are
120. `bounded(0,120)` permits any finite size from 0 through 120. `fill()` needs
a finite available maximum; without one it falls back to content and can report
a diagnostic. Percent also needs a definite basis.

Box width/height rules describe the border-box size in this implementation.
Padding and border occupy space inside it; margins belong to the parent's
placement entry. This is why `StackPlacement` is not stored as universal Node
state: “grow weight” means something to a stack, not to every possible parent.

### A complete numerical row

Suppose the root gets 320 logical units of width. It has 10 units of padding
on each side, no border, and a horizontal stack with a 10-unit gap. Two children
have natural widths 80 and 120, grow weights 1 and 2, and no limiting maximum:

```text
content width  = 320 - 10 - 10 = 300
child budget  = 300 - 10 gap  = 290
natural total = 80 + 120      = 200
extra         = 290 - 200     = 90

first width   = 80  + 90 * (1 / 3) = 110
second width  = 120 + 90 * (2 / 3) = 180

first x       = 10
second x      = 10 + 110 + 10 = 130
right edge    = 130 + 180 = 310; remaining outer padding = 10
```

If the first child caps at 100, it takes only 20 of the extra 90. The second
takes the remaining 70, becoming 190. This is bounded weighted redistribution:
freeze a child at its limit, redistribute what remains among eligible children.

Shrinking uses `shrink * original basis` as weight. With the same natural sizes,
equal shrink factors and only 150 units left for children, the 50-unit shortage
splits 20/30, producing 60 and 90. Authored fixed-size children do not participate
in flex redistribution. If minima/fixed sizes prevent fitting, overflow is a real
result; clipping is a separate decision, not permission to invent negative sizes.

The implementation is [allocateStack](../src/layout/LayoutAlgorithms.cpp).
Its repeated clamping passes have O(n²) worst-case work, usually much less for
small rows. That is a deliberate comprehensible algorithm, not a claim of a
universally optimized flex engine.

### Alignment is spare-space arithmetic

In a 60-unit-high row, a 20-unit-high child has 40 units spare:

```text
start:  y = 0
center: y = (60 - 20) / 2 = 20
end:    y = 60 - 20 = 40
```

Add the parent's content origin and applicable margins. Stretch changes the
child's allocation rather than merely its offset. Horizontal Start/End also
respect layout direction; they are not always physical Left/Right.

Text baseline alignment is another offset calculation: choose a shared baseline,
then position each child at `sharedBaseline - childBaseline`. That aligns letters
with different font sizes more usefully than centering their rectangles.

### Other containers are different allocation rules

| Container | Main operation |
|---|---|
| Box | One content box, insets and alignment |
| HStack/VStack | Allocate along one axis; align along the other |
| ZStack | Place several children over the same available region |
| Grid | Allocate row/column tracks, spans and gaps |
| Flow | Greedily start a new line when the next outer extent exceeds capacity |
| AnchorLayout | `position = parentFraction * available - selfFraction * childExtent + offset` |
| ConstraintLayout | Solve declared linear relations using Kiwi, within this container |
| AdaptiveStack | Select stack axis according to available space; not arbitrary tree replacement |
| ScrollView | Maintain an offset and viewport over larger content |
| VirtualList/Grid/TrackGrid | Realize relevant items instead of allocating a Node for every model item |
| Layer | Retain a raster result when its inputs are unchanged; not a layout algorithm |

A constraint such as `a.width = 2 * b.width` is linear. Arbitrary text wrapping,
perspective projection and “find the best-looking layout” are not jobs for that
solver. Most normal stacks need sums, differences, min/max and ratios—not matrix
algebra or calculus.

## 4. A box, an image and a transform are different things

The parent gives a node a box. The node fits its content into that box. A visual
transform then maps the node and its descendants for painting and hit testing.
These operations should not all overwrite one `RectTransform`.

For a 200×100 image inside a 100×100 content box:

```text
contain scale = min(100/200, 100/100) = 0.5 -> 100×50, center y = 25
cover scale   = max(100/200, 100/100) = 1   -> crop excess width
stretch       = x scale 0.5, y scale 1     -> 100×100, distorted aspect
shrink        = min(1, contain scale)      -> never upscale
```

The current Cover implementation crops source coordinates then fills the
destination. Source rectangles use image pixels; destination rectangles use
local logical units. That is the bridge your old `renderInto` was beginning to
describe. See [ContentTypes.cpp](../src/ui/content/ContentTypes.cpp).

### Affine 2D math

`Transform2D` has six useful numbers:

```text
x' = a*x + c*y + tx
y' = b*x + d*y + ty
```

Translation uses tx/ty. Scaling changes the two basis directions. Rotation mixes
x and y using sine/cosine. Multiplying transforms composes mappings: with this
project's column-vector convention, `parent * child` applies child first.

The node's visual transform is applied around its chosen pivot, then translated
into its arranged position. Visual rotation does not ask the stack to reflow its
siblings around a newly rotated silhouette; layout still allocated a box.

Hit testing reverses the mapping: transform the pointer into a node's local
coordinates, then test its local shape. A zero-scale transform cannot be inverted,
so the system rejects/skips that hit rather than dividing by zero. This is why
painting and hit testing must agree on transform composition and clipping.

### Units and resolution

Keep these distinct: logical UI units, window units, drawable pixels, source
image pixels and scene-world units. `UISession` supplies the conversions.
A 100-unit label at density 2 may need a roughly 200-pixel raster; its logical box
is still 100 units. SVG/text rerasterization and font fitting are not the same
operation as stretching a low-resolution bitmap.

Changing whole-frame resolutionScale changes render target resolution, not UI
input coordinates. SceneView has its own render scale. A fixed-canvas viewport
scales a logical composition; reflow offers a new logical size and lays it out
again. Window sizing policy decides whether initial preferred content should
resize the window. These are separate knobs, not one “size” setting.

## 5. Input, callbacks, identities and safe times to mutate

The old Demo2DUI manually iterated children in reverse to offer SDL events.
UIRoot now finds a target using reverse paint order, transforms and clipping,
then routes an event along its ancestor path: capture → target → bubble.
Handling, stopping propagation and preventing default behavior are distinct.
Pointer capture keeps a drag/release routed to the capturing node even outside
its bounds; it is unrelated to the capture phase of event routing.

This is DOM-like routing, not a DOM, browser accessibility implementation or a
React reconciler. Nodes are retained objects; props mutate those objects. Stable
collection keys preserve item identity through reorder without rebuilding a full
description tree each frame.

`NodeHandle` combines a weak identity-table reference with slot index/generation.
If slot 7's old button disappears and a new node occupies slot 7, the generation
prevents a delayed callback from mistaking the new node for the old button.
Scene `ObjectId` adds an explicit scene owner identity for the same reason.
Neither handle owns the node/object. Borrowed pointers are still valid choices
inside an owner that guarantees a child's lifetime; they are not safe delayed
references to independently removable children.

Structural mutations are deferred during traversal. Removing the node whose
callback is currently running would otherwise invalidate the iterator/path or
destroy its executing function. `UIRoot::defer` records the intent and applies it
at a safe boundary. This is not multithreading; it is disciplined single-threaded
reentrancy management.

### Three queues and one slot

| Mechanism | Purpose | Execution |
|---|---|---|
| PendingAppCommand | Coalesce host intent; Quit wins | Host safe boundary; **one optional slot, not a queue** |
| UIRoot deferred mutations | Structural changes after traversal | Owner thread; newly posted work waits for a later flush |
| Scheduler priority queue | Earliest timer deadline first; stable tie order | Owner-thread advance; not a background timer thread |
| CompletionQueue | Bounded worker-to-owner callback mailbox | Mutex-protected posting, owner-thread draining |

GPU submissions are a further, independent queue/lifetime problem, discussed below.

`Connection` is RAII for a subscription; destroying it disconnects. If you discard
the returned Connection immediately, you just disconnected immediately. TimerHandle
uses the same cancellation idea. Keep these handles in the object that owns the
subscription, just as you keep a resource handle in its owner.

The completion mailbox is many-producer/single-consumer, not a worker pool. It
does not launch work. A callback is popped before execution; a throwing callback
is attempted once and the untouched tail remains queued. Posting and closing are
synchronized, but live UI mutation remains owner-thread-only. Use a node handle
and request revision to reject results that arrive after a new request or removal.

Cancellation asks work to stop. Stale-result rejection prevents obsolete work
from being applied. You often want both; neither implies the other.

## 6. The C++ tools being used to manage this

| Tool/pattern | Why it appears here | Cost or constraint |
|---|---|---|
| `unique_ptr<Node>` tree | One clear owner, polymorphic children | Separate allocations; child objects have stable addresses while owned |
| `shared_ptr<const Source>` | Several consumers/caches retain immutable resources | Reference-count overhead; no synchronization of mutable data |
| `weak_ptr` + generation | Safe non-owning delayed identity | Resolve/check before use |
| `span<const T>` | Borrow contiguous input without copying a vector | Never retain beyond the owner's lifetime |
| `variant` and enums | Closed choices, explicit operation states | Must handle alternatives; no implicit property cascade |
| Abstract interfaces | Substitute painters/backends and test doubles | Virtual dispatch, explicit lifetime contract |
| Templates/concepts | Typed builders, placement containers, small sequence helpers | Compile-time work and visible definitions; not runtime reflection |
| Move-only callbacks | A task may capture unique ownership | Cannot copy the callable; lifetime matters during cancellation |
| RAII scopes | Restore paint state, flags, leases, locks on exceptions | Cleanup must not throw; cannot undo already submitted work |
| Staging/transaction helpers | Validate/build candidate before publishing; restore on failure | Rollback may fail and cannot undo arbitrary external callback effects |
| PImpl/private implementation headers | Hide native GPU internals from consumers | Indirection; helps include boundaries, not a magical runtime optimization |

The builder syntax is ordinary C++: `make<T>` forwards constructor arguments;
`children` forms a tuple; `std::apply` unpacks it; a fold expression appends its
children. Unlike JSX/reactive evaluation, calling a builder immediately allocates
real retained nodes. `std::move` enables ownership transfer; it does not schedule
work or make an object faster by itself.

`Node::measure`/`render` use the **template method pattern**: the nonvirtual public
workflow enforces shared rules, then calls protected virtual customization hooks.
That name is unrelated to C++ `template` syntax. Override `measureContent`/`paint`
when writing a node, rather than reimplementing all border/cache/transform rules.

Substantial implementations moved to `.cpp` files and are grouped into CMake
modules. A header declares a contract; a `.cpp` defines one compiled implementation.
You do not need alternate implementations to justify that split. Templates and
small constexpr operations stay visible where instantiated/evaluated. This usually
reduces repeated parsing and implementation dependency leakage; runtime inlining
still depends on optimization/LTO, not simply whether the file ends in `.hpp`.

### Important algorithms/data structures

| Implementation | What it buys us | Limit worth remembering |
|---|---|---|
| Vector of unique child owners | Ordered composition/traversal | Insertion/reorder shifts pointer entries; tree nodes are not packed records |
| Free slots + generation IDs | Reuse storage without stale-identity aliasing | IDs still belong to their owner/table |
| Weighted bounded redistribution | Flex/track sizing with minima/maxima | May take repeated clamping passes |
| Greedy flow breaking | Predictable wrapping of boxes | Not optimal typography/paragraph balancing |
| Fenwick tree in ExtentIndex | Prefix extents, point updates, visible-index lookup in O(log n) | Rebuild O(n); virtualizing nodes does not eliminate model/index memory |
| Heap in Scheduler | O(log n) timer insertion/removal, earliest deadline at top | Not a general task dependency graph |
| Hash maps + last-use counters | Asset lookup and cache eviction order | Current LRU victim selection scans entries; not every cache operation is O(1) |
| Stable transparent sorting | Back-to-front draw order while preserving ties | Center-depth ordering cannot solve intersecting transparency |
| Iterative scene hierarchy traversal | Avoid recursion for scene cache/subtree work | Dirty refresh still scans slots and rebuilds draw descriptions |
| Clip polygons + triangle edge functions | Reference software rasterization | CPU cost scales with geometry and covered pixel work |
| Reusable streams + completion-aware targets | Reduce native allocation churn safely | Bounded capacity can refuse work instead of blocking |

For the Fenwick tree, imagine item heights `[20, 30, 10]`: prefix boundaries are
`[0, 20, 50, 60]`. Scroll offset 35 lies in the second item. Changing its height
updates O(log n) stored partial sums rather than rebuilding every later prefix.
The bit operations merely navigate which sums cover which intervals.

## 7. How a 3D point reaches a 2D pixel

The current renderer is rasterization, not ray/path tracing. Picking rays are for
input queries; they do not make the renderer a ray tracer. Start with
[Geometry3D.cpp](../src/math/Geometry3D.cpp) and
[SoftwareSceneRenderer.cpp](../src/platform/sdl/SoftwareSceneRenderer.cpp) if you
want to see the math execute on the CPU.

### Vectors first

A point is a location; a direction/displacement describes movement between points.
Subtract positions to get a displacement. Its length is `sqrt(x*x + y*y + z*z)`.
Normalize by dividing by length when you need direction with unit length; zero
has no direction and is rejected.

`dot(a,b)` measures how strongly vectors align: for unit vectors it is the cosine
of the angle. Projecting displacement onto a camera basis direction gives that
coordinate. `cross(a,b)` gives a perpendicular direction; order matters. These
build camera right/up/forward in `lookAtLH`.

### Matrices are composable coordinate mappings

This project uses left-handed camera coordinates, +Y up, camera forward +Z,
column vectors, column-major storage, and radians. UI coordinates remain +Y down.
Storage order and multiplication convention are separate choices; don't infer one
from the other when adapting a library.

```text
local point -> M -> world point -> V -> camera point -> P -> clip coordinates
clip = P * V * M * (x, y, z, 1)
```

M is the model's local-to-world transform; a child's world transform includes its
parents. V changes from world coordinates to the observer's coordinates. P projects
the camera volume into a normalized clipping volume. Read the product right-to-left.

The fourth component lets translation and perspective participate in matrix math.
A point begins with w=1; an affine direction uses w=0 so translation does not move
it. After projection, divide x/y/z by w. This *homogeneous divide* is a real stage,
not an incidental conversion to a three-element vector.

`Transform3D::matrix()` is T*R*S: scale, rotate, then translate. Rotation uses a
normalized quaternion to represent orientation without storing three ordered Euler
angles. You can use `axisAngle(axis, radians)` and learn quaternion internals later.
Quaternion multiplication composes rotations; its four values are not RGBA or four
independent angles.

### Perspective is your distance-scaling intuition, made precise

For the current left-handed perspective camera:

```text
f = 1 / tan(verticalFov / 2)
x_ndc = (f / aspect) * x_camera / z_camera
y_ndc = f * y_camera / z_camera
```

At a 90-degree vertical FOV and aspect 1, f=1. A camera-space point (1,0,2)
projects to x=0.5; (1,0,4) projects to x=0.25. Twice as far from the camera halves
the apparent offset and size. This assumes positive camera-space depth inside the
camera's valid volume—not arbitrary Euclidean distance from the camera.

Map normalized coordinates into a W×H target:

```text
pixel_x = (x_ndc + 1) * W/2
pixel_y = (1 - y_ndc) * H/2
```

For W=200, x=0.5 maps to 150 and x=0.25 maps to 125. The y subtraction flips
3D +Y-up into pixel +Y-down. Native GPU conventions are handled at the backend;
do not add an extra Vulkan flip to application coordinates.

Near/far clipping and depth are also part of P. With positive camera-space z:

```text
depth = far/(far-near) - (near*far)/((far-near)*z)
```

This maps near to 0 and far to 1 nonlinearly. It is not “store world distance in
the depth buffer.” Geometry crossing clipping planes must be clipped before the
divide; otherwise points near/behind the camera produce invalid giant triangles.
Orthographic projection omits the distance shrink while still providing depth.

### A triangle covers pixels, not just its three vertices

The software path clips each triangle, projects the surviving polygon, splits it
into triangles, and scans each screen-space bounding rectangle. Edge tests decide
whether a pixel center is inside. A consistent top-left edge rule prevents shared
edges from being counted twice or leaving cracks.

Inside the triangle, barycentric weights `(l0,l1,l2)` sum to 1. They describe the
point as a mixture of the three vertices. Depth is interpolated in screen space
and compared with stored depth; opaque/masked accepted fragments write it.
Blended fragments test depth but do not write it, and need appropriate draw order.

UVs need perspective-correct interpolation:

```text
u = (l0*u0/w0 + l1*u1/w1 + l2*u2/w2)
    / (l0/w0 + l1/w1 + l2/w2)
```

Do the same for v. Interpolating raw u/v after perspective would make a texture
look incorrectly stretched across a receding surface. Hardware performs the
corresponding rasterization/interpolation when we submit geometry and shaders.

Picking reverses the camera projection: convert pointer position to normalized
viewport coordinates, unproject near/far points with inverse(P*V), then form a
ray between them. Scene3D tests world-space bounds and then triangles. This is
currently a linear object traversal, not a spatial acceleration tree.

## 8. Scene integration and GPU lifetime

Scene3D stores objects, parent relationships and shared meshes. It produces
`MeshDraw` snapshots with computed world matrices. A SceneView owns view policy
and camera access, obtains scene output during preparation, then paints that
output as an image in its arranged content box. Scene2D similarly describes
image objects/2D transforms; it is not the UI layout engine wearing another name.

This lets the same 3D view sit in a Box, scroll area or split layout without
teaching PaintContext about camera ownership. “Render scene to an image, then
compose the image” is the useful bridge. It does add offscreen work and storage;
caching unchanged output avoids paying the whole cost every frame.

### The lifetime distinction that C++ ownership cannot solve alone

```text
CPU source alive
  -> backend realization created
  -> commands record a use
  -> submission accepted
  -> GPU actually completes it
  -> storage can be reused when no published CPU owner still observes it
```

A shared_ptr can keep an image alive. It cannot tell you whether a device is
finished reading it. Submission returns before execution finishes. Fences establish
completion; per-domain submission numbers label that sequence. Resource leases
keep usage/accounting tracked through recording and in-flight submissions.

An uploaded image is published after successful upload submission. An internal
writable ColorTarget becomes a published const image after initializing submission.
It need not wait for GPU completion before a later ordered consumer reads it.
But overwriting/reusing it requires CPU ownership and completion checks. If native
submission fails ambiguously, the domain is invalidated and leases are retained
until retirement rather than optimistically recycled.

The allocation policy is a set of safeguards, not a discovery of free RAM/VRAM.
The pool's live-byte cap includes its retained, published and in-flight targets;
it does not count every allocation made by SDL_ttf or every driver overhead byte.
Image/mesh residency is a different budget. Eviction drops the cache's owner,
not the validity of a caller's live handle.

### Color and text are representation contracts too

GPU composition uses linear-light, premultiplied RGBA. For source-over blending:

```text
out_rgb = source_rgb + destination_rgb * (1 - source_alpha)
out_a   = source_a   + destination_a   * (1 - source_alpha)
```

RGB here is already multiplied by alpha. sRGB image bytes must be decoded before
linear blending; premultiplying encoded bytes and labeling them linear is wrong.
The existing software UI painter blends encoded sRGB and advertises that limitation;
do not expect universal pixel equality with the GPU backend. Scene software
rasterization does its shading/composition math in linear values internally.

Text is not one quad per Unicode code point. HarfBuzz shapes runs into glyphs and
positions; FreeType/SDL_ttf rasterize them. Glyph atlases place many glyph bitmaps
in textures, and draw geometry samples those regions. Current blended horizontal
text can use that path; other supported styles use CPU raster/upload compatibility.
Font state, fitted size, wrapping, direction and actual pixel representation must
agree across those paths. An atlas is a storage/rendering strategy, not a replacement
for shaping or measurement.

## 9. Exceptions and recovery without pretending everything is transactional

The host's activation sequence keeps the old app alive while preparing/entering
the candidate. On success it cleans the old app; on failure it cleans the candidate
and restores captured host/window/renderer state. Cleanup commands are suppressed.
Rollback failure is terminal. This is stronger than “catch everything and carry on,”
but it cannot undo network messages or arbitrary file writes in an app callback.

Native frame failures are classified for bounded recreation. Invalid props,
allocation-policy refusal and arbitrary application exceptions do not become
device-loss events. The frame dies before backend replacement; the new device gets
a new resource domain; the next update delta is rebased instead of including the
whole recovery pause. The failed update is not replayed.

CPU performance timers describe how long the owner thread spent in phases. Native
timestamp queries describe GPU execution intervals, arriving asynchronously. A
completion-latency sample measures yet another interval. Comparing those separate
measurements is useful; adding them together as if sequential work is misleading.

## 10. Complexity boundaries and the follow-up refactor

The subsequent [C++ asset/reconstruction refactor](platform/ASSETS.md) adds a
frozen typed catalog alongside the cache, app-owned view resources and model state,
and optional bounded CPU model preparation. UI/app authoring remains C++; no
document app or automatic filesystem reload was introduced. Minesweeper's model
is no longer stored in UI cells, and its old SDL user-event pointer protocol was
removed in favor of typed model actions and revision-based visual synchronization.

The first six pressures below motivated the current follow-up refactor:
PresentationSession now owns native presentation; property groups no longer hide
Node settings; Patch lives in support; UIWorkStats feeds per-root monitor history;
text has bounded-entry layout reuse; and ancestor invalidation is a single walk.
LayoutBoundary isolates explicit extents and arrangement reuse avoids clean work.
See the UI reference for the precise conservative collection/cache contracts.
The remaining rows are directions to revisit when measured workloads justify them.

| Pressure observed | Sensible direction | Avoid doing by default |
|---|---|---|
| AppHost combined presentation and orchestration | PresentationSession owns window/backend/checkpoints; host coordinates app/settings transactions | A universal manager/service locator for every object |
| App-specific and engine property names hid base groups | Named reusable groups and component-specific props/applyPatch | Inventing setters for immutable values |
| LayoutPrimitives imported UI's Patch | Operation-only support/Patch.hpp shared by layout and UI | Moving all layout values into UI or templating every setter |
| UI hot paths allocate temporary plans/path vectors | Measure, then reuse bounded scratch storage or introduce a frame arena | A custom allocator for every persistent Node before profiling |
| Constraint-specific measurement can retain state | Two-offer opt-in for pure nodes, separate Text layout cache, conservative collections | Unbounded memoization or approximate floating-point hash keys |
| Broad layout and deep invalidation | Single ancestor walk, checked arrangement reuse and explicit LayoutBoundary | Calling the current engine O(changed nodes) when it isn't |
| Scene refresh scans slots and copies snapshots | Dirty work lists/stable submission views if large dynamic scenes demand them | An ECS rewrite merely because meshes have properties |
| Asset eviction scans for oldest eligible entries | Add an explicit LRU index if eviction cost becomes visible | Assuming hash lookup makes all cache management O(1) |
| Native escape hatches remain powerful | Keep them narrow; require declared reads and ownership contracts | Publishing mutable native resources as generic images |
| Specialized content still knows SDL resources | Add a second resource provider when another backend actually needs one | An abstract interface for every SDL call today |
| Broad Vulkan readback test has many assertions | Split future regressions into focused contracts with shared readback fixtures | Whole-program screenshots as the only correctness evidence |

The current timer/mailbox system is not a parallel task graph. The current scene
hierarchy is not an ECS or physics world. The retained UI is not a reconciler.
Those can be deliberate additions later; none is required just to compose an app.

## 11. A reading and experimentation route

Do these in order; stop when you can predict the output, not when you have memorized
the whole implementation.

1. **Application ownership:** read [Demo2DApp](../include/demo2d/Demo2DApp.hpp) and
   [Demo2DUI](../include/demo2d/Demo2DUI.hpp). Trace creation, input, update, render and
   exit. Identify which objects are persistent versus borrowed per call.
2. **Layout arithmetic:** read BoxProps and `allocateStack`; calculate a two-child
   row by hand. Change only gap/padding/alignment and predict the bounds.
3. **Layout protocol:** read `Node::measure`, `Node::arrange`, then Stack's two
   overrides. Notice where the second measurement gets a fixed width.
4. **Invalidation:** follow one `Text::setProps` change through layout invalidation,
   prepare and paint. Contrast a color-only change with changing the string.
5. **Input ownership:** read Button and `UIRoot::dispatch`; follow press, drag out,
   release. Then follow a deferred child removal without storing a raw callback pointer.
6. **Resource preparation:** trace Image → ImagePreparer → backend image. Identify
   the CPU source that must survive a device change.
7. **Scene math:** use identity transforms, then translation, then perspective.
   Predict one point's normalized/pixel coordinate before reading the rasterizer.
8. **Scheduling/failure:** read HostTransitions tests and CompletionQueue. Ask which
   operations may throw, what was already published, and what remains queued.

Useful focused test runs (build the tests first):

```powershell
cmake --build --preset debug --target playground_tests
ctest --test-dir build/debug -R '^(layout_arithmetic|ui_layout_variants|ui_content_fit)$' --output-on-failure
ctest --test-dir build/debug -R '^(transform3d|scene_invalidation|host_transitions)$' --output-on-failure
```

For a new RPS app, start with its model and one composed UI tree. Use the existing
UISession and IApp hooks. You do not need a new node base class, custom renderer,
worker pool or general state-binding framework to begin. Let actual composition
pressure reveal the next helper you want.
