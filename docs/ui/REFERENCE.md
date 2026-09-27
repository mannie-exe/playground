# Retained UI reference

C++23 retained UI: ownership, properties, layout, input, rendering and resource
contracts. This is a project-specific model, not CSS or SwiftUI conformance.

Read [GUIDE.md](GUIDE.md) for a progressive introduction,
[TESTING.md](TESTING.md) for test scope and coverage boundaries, and
[CONTRIBUTING.md](../../CONTRIBUTING.md#testing) for test commands and contribution rules.
This reference defines behavior; linked headers provide declarations and small
inline/template definitions, with compiled implementations under src/. Changes to
a contract require corresponding documentation and tests.

## Contents

- [Architecture and vocabulary](#architecture)
- [Values and geometry](#values)
- [Properties, patches and invalidation](#properties)
- [Ownership and composition](#ownership)
- [Layout contract](#layout)
- [Node index and reference](#nodes)
- [Runtime, input and services](#runtime)
- [Rendering, resources and caches](#rendering)
- [Host integration](#integration)
- [Cost model](#cost-model)
- [Scope and extension boundaries](#scope)
- [Background references](#references)

<a id="architecture"></a>
## Architecture and vocabulary

The UI contract is independent of a windowing API or painting implementation.
[CONTRACTS.md](CONTRACTS.md) defines responsibilities for hosts, custom nodes,
containers, resource providers and callbacks. [2D rendering](../render/2D.md)
defines the painting boundary; [GPU](../render/GPU.md) and
[3D](../render/3D.md) describe separate rendering services and their availability.

```text
host owns platform lifetime, rendering backend, assets and active application
application owns model/composition and a UIRoot (directly or through an adapter)
UIRoot owns one persistent Node
containers own children + parent-specific placement records
content nodes borrow asset services and retain shared image/font handles
platform adapters translate native input; a borrowed PaintContext paints the tree
```

The core dependency direction is UI runtime → layout → own math. A node neither
owns a window nor presents a frame. Measurement/arrangement use logical geometry;
preparation can realize images for the selected backend without changing layout.
Frames, painting contexts and preparation services are borrowed for a pass, never
stored in nodes or captured by asynchronous work.

Image accepts a generic PaintImageHandle. Text/Vector currently depend on the
project's AssetRegistry and font/SVG providers. Their rendering output uses the
same image boundary, but replacing a painter does not replace shaping, text layout
or asset decoding. This is backend-neutral composition, not a claim that every
resource implementation has no platform dependencies.

Scene viewport nodes bridge application models into this same layout tree:

| Node | Properties and preparation contract |
|---|---|
| [`SceneView`](../../include/ui/content/SceneView.hpp) | Shared read access to a `scene::Scene3D`, camera, preferred size, clear color, resolution scale, lighting, exposure and tone mapping. Requires the frame's separate scene renderer; caches output by model revision, view properties, logical aspect, pixel size and renderer domain. See [materials](../render/MATERIALS.md) for GPU requirements. |
| [`Scene2DView`](../../include/ui/content/Scene2DView.hpp) | Shared read access to a `scene::Scene2D`, affine camera and preferred size. Prepares ordered image/solid items and clips them to its content box through the 2D painter. |

Both expose complete props and Keep/Set/Reset patches, honor box insets, and leave
world interaction/model updates to the application. Their scenes do not become UI
children. Pass `RenderFrame` to `UISession::render` when using SceneView so its
preparation receives the scene renderer. See [scene contracts](../render/3D.md)
and the [additive guide](GUIDE.md#add-a-scene-viewport-without-changing-ui-layout).

| Term | Precise meaning |
|---|---|
| Props | Authored configuration, not mutable interaction state or calculated geometry |
| State | Focus, press, selection/model data, scroll offset |
| Derived result | Measured size, assigned bounds, raster, layout plan |
| Padding | Space inside the border box |
| Margin | Space on the parent-child relationship, outside the child's border |
| Gap | Parent-owned minimum spacing between participating siblings |
| Alignment | Position within offered space |
| Distribution | Use of remaining positive main-axis space |
| Intrinsic size | A measurement under constraints, not an unconditional stored size |
| `placementInParent` | This node's placement as its parent's child |
| `childrenAlignment` | A container's policy for its children |
| `contentAlignment` | Content within a leaf/single-child boundary |
| Retained | Instances survive frames and are explicitly mutated |
| Virtual | Only a working subset of model items has live nodes |

Properties modify one boundary; wrapping creates another boundary.
Calling setPadding twice replaces padding; nesting two Boxes keeps two paddings.
No arbitrary inherited property cascade, implicit scene system, or
whole-description reconciliation pass is involved. Node is not IApp, a window,
or a surface.

<a id="values"></a>
## Values and geometry

Sources: [Geometry2D.hpp](../../include/math/Geometry2D.hpp),
[Color.hpp](../../include/math/Color.hpp),
[LayoutPrimitives.hpp](../../include/layout/LayoutPrimitives.hpp).

| Type | Public meaning / key operations |
|---|---|
| `Vec2<T>`, `Vec2i`, `Vec2f` | Component arithmetic; integer indices/pixels versus float vectors |
| `Point2` | Float x/y position; point − point is a vector |
| `Size2` | Float width/height; distinct from a position |
| `Rect` | position + size; x/y/w/h, edges, center, contains, intersection/union, inset/outset |
| `Insets` | left/top/right/bottom; all, symmetric, horizontal/vertical totals |
| `Gap2` | horizontal/vertical spacing, not a position |
| `CornerRadii` / `RoundedRect` | Four elliptical corners, proportional normalization, inset and containment |
| `ClipShape` | Rect or RoundedRect, not arbitrary vector paths |
| `ColorRGBA8` | Own 8-bit RGBA value |
| `Transform2D` | Affine a/b/c/d/tx/ty; compose, inverse, mapPoint/mapBounds, around-pivot helper |
| `Length` | units or percent; resolve against optional basis; not every prop accepts it |
| `SizeRule` | content(), fixed(units), percent(fraction), fill() |
| `AxisConstraints` | minimum=0, maximum=nullopt; tight/bounded/unbounded, clamp, deflate |
| `SizeConstraints` | width/height constraints |
| `MeasureResult` | border-box size, optional first/last baselines measured from its top |
| `LayoutResult` | borderBounds in parent space; contentBounds/overflowBounds in local space |
| `TrackSize` | fixed, content, fraction; bounds belong to content/fraction tracks |
| `AnchorAxis` | variant of AnchorPosition and StretchBetween |

Geometry values are owned by the project, not aliases of platform types.
Conversions, checked pixel rounding and native-type comparisons belong at the
[adapter boundary](../render/2D.md#platform-adapter). Never reinterpret_cast
between unrelated geometry types.

Use named inBounds/inBoundsExclusive/isPositive/hasArea checks, not vector
ordering. Rectangle containment is half-open: leading edges included, trailing
edges excluded. Exact equality is for props/cache keys; almostEqual is an
explicit tolerance-based numerical comparison, not hash-map equality.

Logical layout coordinates are floats; raster requests and grid indices are
integers. Valid measured sizes, margins, padding, borders and gaps are finite
and nonnegative. Positions/visual translations may be negative. Geometry value
structs can temporarily hold arbitrary numbers; layout/API boundaries validate.
An absent maximum means unbounded; infinity/NaN are not authored sentinel values.
Checked pixel conversion includes rounding shared edges consistently and guarding
integer/area overflow.

Relevant enums:

| Family | Values |
|---|---|
| Axis / LayoutDirection | Horizontal, Vertical / LeftToRight, RightToLeft |
| Align | Start, Center, End, Stretch (both axes; vertical Start means top) |
| CrossAlignment | Start, Center, End, Stretch, FirstBaseline, LastBaseline |
| Distribution | Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly |
| Visibility | Visible, Hidden, Collapsed |
| OverflowPolicy | Visible, Clip |
| HitTestPolicy | None, ChildrenOnly, Self, SelfAndChildren |
| ContentFit | None, Stretch, Contain, Cover, Shrink |
| Sampling | Nearest, Linear |
| DirtyFlags | None, Measure, Arrange, Paint, HitTest, Semantics, All |

Formatting is opt-in via [GeometryFormatters.hpp](../../include/math/GeometryFormatters.hpp),
[LayoutFormatters.hpp](../../include/layout/LayoutFormatters.hpp),
[UIFormatters.hpp](../../include/ui/UIFormatters.hpp). Native-type formatting
belongs to the platform adapter.
UI/layout enum formatters use readable names; DirtyFlags supports combinations.
Content placement uses logical `layout::Alignment`; Start/End follow the layout
direction rather than encoding physical Left/Right.

<a id="properties"></a>
## Properties, patches and invalidation

Source: [NodeProps.hpp](../../include/ui/NodeProps.hpp), [Patch.hpp](../../include/support/Patch.hpp).

Operation-only `playground::Patch<T>`, `Keep` and `Reset` live below both layout
and UI. Common Node values use `settings()/setSettings()/applySettingsPatch()`.
Reusable groups use `contentProps/setContentProps/applyContentPatch` (Box),
`buttonProps/setButtonProps/applyButtonPatch` (Button), and
`gridProps/setGridProps/applyGridPatch` (Grid). Leaf/app-specific values use
`props/setProps/applyPatch`; there is no inherited ambiguous `Node::props`.

| Common group | Fields / baseline |
|---|---|
| NodeProps | visibility=Visible; debugName/key=nullopt |
| BoxProps | width/height=Content; minWidth/minHeight=0; maxWidth/maxHeight=nullopt; padding/borderWidths=0; aspectRatio=nullopt |
| PaintStyle | background/borderColor=nullopt; opacity=1, within [0,1] |
| VisualProps | transform=identity; pivot={0.5,0.5}; overflow=Visible; clipRect=nullopt |
| InputProps | hitTest=ChildrenOnly; focusable/focusScope/modal=false; wrapNavigation=true; optional next/previous/left/right/up/down neighbors |
| SemanticProps | role=None; name/description empty; value=nullopt; enabled=true; exposure=Auto; optional labelledBy/describedBy NodeIds |

Passive content leaves set hitTest=None. Button sets SelfAndChildren, focusable,
and role=Button. Concrete controls derive enabled/value/action semantics from
their own props; use Button::setEnabled for state/color/capture cleanup. On a
generic node, semantic enabled=false disables its subtree. Pointer hit testing
does not control keyboard or assistive eligibility. WindowServices publishes
UIRoot snapshots through AccessKit; see [accessible interaction](ACCESSIBILITY.md).

Common APIs: nodeProps/setNodeProps/applyNodePatch, boxProps/setBoxProps/
applyBoxPatch, paintStyle/setPaintStyle/applyPaintPatch, visualProps/setVisualProps/
applyVisualPatch, inputProps/setInputProps/applyInputPatch,
semanticProps/setSemanticProps/applySemanticPatch.

`Node::settings()/setSettings()/applySettingsPatch()` uses combined NodeSettings/NodeSettingsPatch.
Concrete nodes use separate names for their own Props/Patch group; use qualified
`node.Node::applySettingsPatch(...)` or a Node reference for a combined common update.
A Button inherits Box's setContentAlignment; to edit BoxContentProps explicitly,
qualify Box::setContentProps/applyPatch rather than accidentally passing ButtonProps.

### Patch operations

`Patch<T>` is an operation, not nullable storage:

| Operation | Meaning |
|---|---|
| keep() / default construction | Leave existing value |
| set(value) | Replace, including false, zero, empty string or nullopt |
| reset() | Use the declared baseline; reject if the field has no baseline |

Inspect with isKeep/isSet/isReset/value/visit; appliedTo(current, baseline)
resolves an operation. appliedTo(current) rejects Reset.
Required Rectangle fill, Image image, Text font and Vector source reject Reset.
Common-group Reset uses the common struct baseline, **not** the object's
constructor arguments or derived control's overrides. Reapply a preset explicitly.

Setters validate a candidate then commit through one route; no mutable props
references. Most prepare throwing work before commit. Do not infer transactional
rollback across user callbacks, resource preparation, or collection reconciliation:
failures must leave ownership valid and work dirty, not undo arbitrary model changes.
Notifications occur after commit and are coalesced by the root; a throwing
notification does not roll back props.

| Change | Required dependent work |
|---|---|
| Text/font/wrap, size, padding/border width, child membership | Measure → arrange → prepare/paint; relevant hits/semantics |
| Colors | Paint; Text also needs a new baked-color raster |
| Visual transform/clip | Paint/hits, no sibling reflow |
| Alignment/placement | Arrange/paint/hits; implementations may conservatively remeasure |
| Hidden | Retain layout, omit paint/input |
| Collapsed | Also exclude layout participation |
| Scroll offset | Arrange/paint/hits/visible range |
| Semantic label | Semantics, not geometry |

Invalidation propagates dependency revisions to ancestors. A single measurement
cache entry includes constraints, local/dependent revision, direction, environment
revision and pixel scale. A clean flag alone is insufficient.
Arrangement also invalidates enclosing layer rasters without scheduling endless
layout passes. Container plans may be recomputed in arrange; no promise
of a persistent Grid/Flow plan cache is made.

<a id="ownership"></a>
## Ownership and composition

Sources: [Node.hpp](../../include/ui/Node.hpp),
[Container.hpp](../../include/ui/containers/Container.hpp), [Builders.hpp](../../include/ui/Builders.hpp).

- Nodes are noncopyable/nonmovable, owned by unique_ptr. Reparenting moves the
  pointer, not the instance. Parent and runtime-service links are borrowed.
- Containers store ordered child owners and parallel typed placement records.
  ChildEntry is authoring metadata, not another runtime geometry node.
- NodeId is a root-local index + generation. NodeHandle<T> includes weak root
  lifetime and checked type resolution; get() returns null if invalid.
  Handles neither retain nodes nor authorize worker-thread access.
- Construct children, retain temporary pointers if useful, attach with
  root.setContent, then obtain handle<T>(). Detached nodes have no usable NodeId.
- Removal/detach invalidates handles and clears focus/capture. Same-root
  compatible reparent preserves identity, validates cycles, and invalidates
  old/new parent layout. Cross-root transfer is take + append, with new IDs.
- onDetach is noexcept and runs while services and dynamic objects are alive.
  App/component-owned Connection/TimerHandle tokens must be released before
  borrowed callback state disappears.
- Do not edit ownership during measure, arrange, input, notifications, or
  lifecycle callbacks. Use root.defer. Collections' prepareChildren is the
  controlled pre-measure realization exception, not permission for arbitrary
  user measurement callbacks to restructure the tree.

Container operations: append/insert return Node&; takeChild transfers ownership;
remove destroys; moveChild reorders. Index overloads support construction.
Attached APIs include placementOf(NodeId), setPlacement(NodeId, Placement),
takeChild/remove/moveChild(NodeId), and root.reparent(childId, from, to, placement).
Placement patches accept index or NodeId on Stack, Grid, Flow, ZStack,
AnchorLayout and AdaptiveStack. Box patches its sole child without a selector.

| Placement record | Fields / defaults | Owner |
|---|---|---|
| BoxPlacement / LayerPlacement | margin=0, alignmentOverride=nullopt | Box / ZStack |
| StackPlacement | margin=0, grow=0, shrink=1, crossAlignmentOverride=nullopt | Stack/Flow/AdaptiveStack |
| GridPlacement | row/column=nullopt (automatic), spans=1, margin=0, alignmentOverride=nullopt | Grid |
| AnchorPlacement | horizontal/vertical=AnchorPosition::start(), margin=0 | AnchorLayout |

No duplicate child-side authoritative placement. Margins surround the border box;
anchor equations place the margin box before insetting it.
Builders: make<T>(args...), children(...), stackChild/gridChild/layerChild/
anchorChild, makeHStack/makeVStack/makeGrid/makeFlow/makeZStack/makeBox.
They accept move-only owners without initializer_list<unique_ptr<Node>>.

Allocation uses individual nodes plus vectors/maps. PMR arenas, pools, packed
results and scratch reuse are optional optimizations; arena release does not
run client destructors. Never retain pass-local views in persistent callbacks.

<a id="layout"></a>
## Layout contract

Implementation: [Node.hpp](../../include/ui/Node.hpp) and
[LayoutAlgorithms.hpp](../../include/layout/LayoutAlgorithms.hpp).

1. measure(context, constraints) reports finite border-box size and baselines.
   It can update private caches but must not emit application actions.
2. arrange(context, bounds) assigns a border box in the **parent's local border
   coordinate system**. Container content bounds already include its padding/
   border offset; do not add that offset twice.
3. prepare(context) resolves raster/resource work after final dimensions.
4. render(context) paints local geometry and descendants through transform/clip
   scopes. Logical application state must not change during painting.

Incoming constraints win over authored size requests. A parent may deliberately
allocate overflowing child minima; it must not invent negative gaps or mutate
authored props. Padding/border subtraction clamps inner dimensions at zero.
Hidden participates, Collapsed does not. No margin collapse.

Content uses measurement. Fixed requests an extent; Percent uses the finite
offered content dimension (1=100%); Fill takes finite offered maximum.
Percent/Fill on an indefinite axis fall back to content with a diagnostic.
Fill is not a stack grow weight. No repeated parent enlargement to solve cycles.

AspectRatio is positive width/height: derive one missing definite axis; otherwise
enclose measured content; two definite axes win with a conflict diagnostic.
Hard constraints can break the ratio. ContentFit is separate from box sizing.

Start/End are logical on x, top/bottom on y; RTL does not reverse source/focus order.
Safe alignment makes oversized children overflow toward End. Baselines fall back
to the border-box bottom where absent. Shared pixel edges are rounded from the
same boundaries, not independent positions and widths.

<a id="nodes"></a>
## Node index and reference

The index covers the available node types. Unavailable capabilities are listed
separately under [scope and extension boundaries](#scope).
Common props apply throughout. Concrete value groups have props/setProps/applyPatch;
callbacks and child ownership are configured separately.

| Category | Types |
|---|---|
| Runtime/extensions | [UIRoot, Component, CustomView](#root-components) |
| Linear/box | [Box, Stack, HStack, VStack, Spacer](#linear) |
| Other arrangements | [ZStack, Grid, Flow, AnchorLayout](#arrangements), [ConstraintLayout](#constraint-layout) |
| Content | [Rectangle, Text, Image, Vector, Path](#content) |
| Interaction | [Button](#button) |
| Boundaries | [Transform, Clip, Layer, LayoutBoundary](#boundaries) |
| Viewports/collections | [ScrollView, AdaptiveStack, Repeat, VirtualList, VirtualGrid](#collections), [VirtualTrackGrid](#track-grid) |

<a id="root-components"></a>
### UIRoot, Component, CustomView

[UIRoot](../../include/ui/UIRoot.hpp) is an owner, not a child. See [runtime](#runtime).
[Component and CustomView](../../include/ui/containers/Box.hpp):
Component extends Box; replaceContent explicitly replaces one subtree. Its
subclass owns model references, handles and subscriptions; no automatic rebuild.
CustomView accepts optional measure/paint/event callbacks (empty defaults).
It inherits ChildrenOnly hit policy; set Self for an interactive leaf.
Subclass hooks can supply lifecycle or richer typed behavior.

<a id="linear"></a>
### Box, Stack, HStack, VStack, Spacer

[Box.hpp](../../include/ui/containers/Box.hpp): zero/one child, content size zero
when empty. BoxContentProps.contentAlignment defaults Start/Start; setChild
attaches replacement before removing old content; takeChild empties it.
Use setContentAlignment or BoxContentPatch; BoxPlacementPatch edits the relation.

[Stack.hpp](../../include/ui/containers/Stack.hpp): Stack takes Axis; HStack/VStack
choose it. StackProps/StackPatch: gap=0, distribution=Start,
childrenAlignment=CrossAlignment::Start. StackPlacementPatch mirrors placement.
Intrinsic main size sums outer children + gaps; cross size is their maximum.
Positive space uses grow weights, negative space shrink × initial basis, freezing
at limits. Fixed sizing locks its axis; zero basis has no shrinkable extent.
Children remeasure at assigned widths; wrapped height/baselines remain correct.
Unresolved negative space overflows; SpaceBetween with one child means Start.
Stretch respects flexible sizing limits; vertical baseline alignment is rejected.
Horizontal stacks export the topmost and bottommost positioned child baselines,
independent of child order or RTL direction. Either available child baseline
contributes to this envelope. Explicit baseline alignment synthesizes a missing
baseline at the child's bottom edge; otherwise baseline-free children contribute
none. Collapsed children are excluded. Alignment groups position children but do
not override the exported envelope. Vertical stacks export no baseline.
Bound-freezing can be O(n²), not guaranteed linear.

Spacer is an empty passive leaf configured through BoxProps. Plain Stack::append
gives Spacer grow=1; explicit placement overrides that. Other parents infer no growth.

<a id="arrangements"></a>
### ZStack, Grid, Flow, AnchorLayout

[ZStack.hpp](../../include/ui/containers/ZStack.hpp): ZStackProps/Patch
childrenAlignment=Start/Start; LayerPlacement uses BoxPlacementPatch.
Measures maximum outer child extent. Paints source order; hits reverse order.

[Grid.hpp](../../include/ui/containers/Grid.hpp): GridProps/GridPatch:
columns/rows each default to one Content track; gap=0; autoPlacementAxis=Horizontal;
childrenAlignment=Stretch/Stretch; allowOverlap=false; implicitTrack=Content.
setTracks/setGap are conveniences; GridPlacementPatch mirrors all placement fields.
A nonempty grid requires nonempty track lists. Indices are zero-based; spans positive.
Explicit cells reserve occupancy first; sparse automatic placement skips occupied
cells and appends implicit tracks, never backfilling or deliberately overlapping.
Explicit overlap requires allowOverlap. Width-first contributions process shorter
spans first; fixed tracks do not grow; eligible content/fraction tracks absorb
deficits within limits. Fractions distribute remaining finite space by weight;
unbounded fractions behave as content. Remeasure at span widths before sizing rows.
This is not CSS Grid/subgrid or a bidirectional constraint solver.

[Flow.hpp](../../include/ui/containers/Flow.hpp): FlowProps/Patch mainAxis=Horizontal,
itemGap/lineGap=0, distribution=Start, childrenAlignment=Start.
StackPlacement/Patch applies within lines. Decide line membership from outer
bases before flex allocation; an oversized item occupies one line, an unbounded
main axis one line. No dense packing/shared columns or iterative optimal packing.

[AnchorLayout.hpp](../../include/ui/containers/AnchorLayout.hpp): common BoxProps,
AnchorPlacement/Patch. AnchorPosition has parentFraction/selfFraction in [0,1]
and signed offset, with start/center/end factories. StretchBetween has nonnegative
startInset/endInset. Fixed size + stretch on the same axis is rejected.
Position is parentFraction × parentExtent − selfFraction × childExtent + offset.
Children do not drive intrinsic size; indefinite axes use minima with diagnostics.
Sibling references are not supported.

<a id="content"></a>
### Rectangle, Text, Image, Vector, Path

[Path.hpp](../../include/ui/content/Path.hpp) is authored vector geometry, distinct
from the SVG-document Vector node. PathProps contains Path2D, a required positive
viewBox, PathPaint, ContentFit and alignment; PathPatch follows normal Keep/Set/Reset
rules (viewBox Reset is invalid). Measure reports viewBox size, and paint maps it
into the assigned content box with the selected fit/alignment and clipping.
PathPaint has optional fill/stroke colors, strokeWidth and NonZero/EvenOdd fillRule.
Curves flatten at output density with bounded work; fills close contours implicitly,
strokes close only explicitly and use round caps/joins. GPU draws coverage directly
from segments, not from a CPU raster. No SVG CSS/gradient/filter interpretation is
implied. See [path rendering](../render/2D.md#vector-paths).

[Rectangle.hpp](../../include/ui/content/Rectangle.hpp): RectangleProps/Patch
fill required, border=nullopt; borderWidths come from BoxProps. Zero intrinsic
content; supply sizing or stretch it. It fills inside the border, optionally
drawing its own border. cornerRadii defaults to zero; shared RoundedRect geometry
supports elliptical corners, border insets and hit testing. Filling a rounded
shape does not itself clip descendants.
Rounded border and fill are sampled together, so an opaque join does not acquire
an alpha seam from separately composited edge coverage.

[Text.hpp](../../include/ui/content/Text.hpp): requires AssetRegistry&, TextProps
and optional BoxProps. TextPatch mirrors its fields:

| Field | Default / meaning |
|---|---|
| value / font | empty UTF-8 string / required FontHandle |
| foreground / background | opaque white / opaque black |
| method | Blended; also Solid, Shaded, LCD |
| wrap | None; AvailableInlineSize wraps to inner width horizontally, inner height vertically |
| paragraphAlignment | Start; also Center/End, resolved with direction |
| contentAlignment | Start/Start; positions the resulting text within its box |
| fontFit | None; ShrinkToFit tests descending quantized font sizes |
| minFontSize / maxFontSize / fitStep | 6 / 256 / 0.5; positive finite interval, at most 4096 steps |
| flow | TextFlowProps: no truncation/line cap, ellipsis=…, HorizontalTb, Mixed |

setValue/setFont/effectiveFont complement props/setProps/applyPatch.
The base FontHandle is unchanged; registry clones are shared by complete props.
Measurement wraps at the offered width; fitting checks width **and** height.
Failure to fit keeps the minimum with a diagnostic; clipping is an explicit policy.
Empty/zero-area content skips raster creation. Prepare uses drawable density.
[Text flow](#text-flow) adds grapheme-safe ellipsis, line caps and vertical columns.
No editor/selection or character-count-based sizing. displayedValue/isTruncated
report the logical-layout result without modifying the authored value.

[Image.hpp](../../include/ui/content/Image.hpp): ImageProps/Patch image required
(PaintImageHandle),
sourceRect=nullopt, assetDensity=1, content=ContentStyle defaults. Natural logical
size is source pixels / assetDensity, not current display density.

[Vector.hpp](../../include/ui/content/Vector.hpp): AssetRegistry& plus VectorProps/Patch
source required (asset-path string or SVGDocumentHandle), styles={},
intrinsicSize=nullopt, content defaults, rasterScale=1,
maximumRasterPixels=16×1024², useTheme=false. Opting into useTheme replaces paint
tint with theme text/mutedText (including disabled ancestors); use white monochrome
SVGs for icons. It does not replace arbitrary SVG fills or recolor artwork intelligently.
Raster size follows final dimensions and display
density; pure movement reuses the raster. Tint is whole-output modulation;
[styles](#svg-styling) edits supported element properties in an immutable variant.

Shared [ContentStyle](../../include/ui/content/ContentTypes.hpp):
fit=Contain, alignment=Center/Center, paint.tint=opaque white,
paint.sampling=Linear. ImagePaint and Sampling are in playground::rendering,
from [PaintImage.hpp](../../include/rendering/PaintImage.hpp); their formatters
are in rendering/RenderFormatters.hpp, also included by UIFormatters.hpp.
None preserves natural size; Stretch independently scales axes; Contain preserves
aspect inside the box; Cover fills and crops; Shrink only downsizes.
Content alignment supports Start/Center/End, not Stretch (use fit=Stretch).
ContentStyle is replaced as a group via its parent's patch, not a property cascade.

<a id="button"></a>
### Button

[controls/Button.hpp](../../include/ui/controls/Button.hpp).
Single arbitrary content subtree, centered by default. ButtonProps/ButtonPatch:
enabled=true; normal={70,70,70,255}, hover={95,95,95,255},
pressed={45,45,45,255}, disabled={60,60,60,255};
focus={255,215,80,255}, focusWidth=2 logical units (finite and nonnegative).
The focus border is drawn after child content and does not change layout size.
setEnabled/isEnabled/isHovered/isPressed; onActivate returns a Connection to retain.

Primary press captures its pointer and button; matching release inside activates,
outside release cancels; unrelated release does not activate. Focused Space/Enter
activates on matching key release, ignoring repeat. Disable/cancel/detach never
activates. Compose icon/text with an HStack rather than hard-coded label/icon slots.

### Stepper

[controls/Stepper.hpp](../../include/ui/controls/Stepper.hpp) composes two Buttons
and a caller-supplied readout. No font or asset acquisition is hidden in the control.
Buttons are 40 logical units wide and at least 40 high, with centered content;
the row gap is 8. Demo 2D supplies decorative Material add/remove SVGs, not glyphs.
StepperProps/StepperPatch: value=0, minimum=0, maximum=100, step=1, enabled=true,
name empty. The range is inclusive; step must be positive. Invalid setters/patches
are rejected before changes. A caller changing limits must supply a valid value.

`props/setProps/applyPatch`, `value`, `stepBy` and `onValueChanged` are public.
stepBy uses direction's sign and saturates at bounds using widened arithmetic.
Programmatic setters update state/semantics without emitting a user-action signal.
User steps emit only after a changed value is committed; throwing observers do
not roll it back. Retain the returned Connection and update readout content in
that callback. Limits disable the corresponding button; disable makes both inert.
Arrow keys adjust the focused stepper, while Enter/Space activate its buttons.
Names, range and value are exposed through SemanticState; WindowServices
translates range, increment/decrement and set-value operations to AccessKit.
Secondary-button behavior requires a subclass/event policy, not onActivate.

### Accessible controls and editing

Contracts and platform limits are in [ACCESSIBILITY.md](ACCESSIBILITY.md).
All controls are retained nodes; they do not create native widgets or windows.
Programmatic setters are silent; user/native operations commit state before signals.

| Header / nodes | Properties and behavior |
|---|---|
| `controls/Choice.hpp`: ToggleButton, Checkbox, Switch | ToggleProps/TogglePatch: checked, allowMixed, name; inherits ButtonProps. Switch rejects Mixed. |
| `controls/Choice.hpp`: ListBox, RadioGroup, Menu | SelectionProps/SelectionPatch: selected key, enabled, required, name; ChoiceItems have keys, labels, content and enabled flags. List/radio arrows select; menu arrows move the active item, Enter/Space invokes. |
| `controls/Slider.hpp`: Slider | SliderProps/SliderPatch: RangeValue, enabled, readOnly, axis, name, colors. Drag, arrows, Home/End and numeric actions. |
| `controls/Slider.hpp`: ProgressBar | ProgressProps: read-only range, name, track/fill colors. |
| `controls/TextField.hpp`: TextField, TextArea | TextFieldProps/TextFieldPatch: FontHandle, TextEditProps, enabled/required, name/placeholder/validation message, colors. TextArea starts multiline. |
| `controls/TextField.hpp`: NumberField | NumberFieldProps: range and integer; drafts do not change the committed number until valid commit. |
| `controls/Composite.hpp`: Disclosure | ExpansionProps/ExpansionPatch: expanded, enabled, name. Retained header/body; closed body is collapsed. |
| `controls/Composite.hpp`: Select | Noneditable trigger plus root-presented scrollable ListBox; SelectionProps and setExpanded. Arrows preview, Enter/click commits, Escape cancels, Tab closes. Supply/update visible trigger content. |
| `controls/Composite.hpp`: Tabs | Keyed TabItems, SelectionProps; retained panels, roving tab stop, arrows/Home/End. Only selected panel participates in layout. |
| `controls/Composite.hpp`: Dialog | DialogProps/DialogPatch: open, modal, dismissOnEscape, name/description. Centered root portal with modal backdrop; UIRoot traps/restores focus. Retain with its logical owner. |
| `controls/Composite.hpp`: Field, FieldGroup | FieldProps/FieldPatch: label/description; supplied visible label/control/help nodes. Field links IDs after arrangement; FieldGroup supplies named grouping. |
| `controls/Composite.hpp`: Status, Tooltip | Supplied content and semantic message. Status is a polite live region. Tooltip is a passive anchored popup; owner controls anchor, open state and timing. |

`Theme.hpp` provides ColorSchemePreference, ContrastPreference, ThemePalette,
SystemAppearance and resolveTheme. `UIRoot::setTheme` publishes the palette;
`Node::setTheme` optionally overrides a subtree (root high contrast wins).
Text and controls default to `useTheme=true`; explicit artwork colors require
false. `PaintStyle::themeBackground` opts a node into the palette surface fill,
without forcing all sessions to cover underlying scenes. Background color, when
provided, takes precedence. Palette changes invalidate prepared color resources.

`containers/Popup.hpp` defines Portal and Popup. `PopupProps/PopupPatch` contain
open, anchor NodeId, placement/fallbacks (Below/Above Start/End or Center),
Content/MatchAnchor width, gap=4, viewportPadding=8, maximumHeight=320,
dismissOutside/dismissOnEscape/closeOnTab/autoFocus=true, and optional backdrop.
Use popupProps/setPopupProps/applyPopupPatch, setOpen/setAnchor, onDismissed.
UIRoot places and paints portals outside ordinary flow/ancestor clips, within
the usable viewport; logical ownership, event bubbling and semantics are retained.
Outside dismissal consumes down through up/cancel; Escape respects IME composition.
Dialogs retain their separate DialogProps and set policy for centered modal use.
See [appearance and overlays](ACCESSIBILITY.md#appearance-and-transient-surfaces)
for platform defaults, focus, placement and ownership contracts.

### Concrete control appearance and layout defaults

These are the implemented defaults, not a native-widget or Material Design theme.
All dimensions below are logical units. Theme affects paint, not spacing or fonts.

| Element | Appearance | Layout / behavior |
|---|---|---|
| Box, Stack, Flow, Grid, groups | Transparent; no implicit border | Content-sized unless constrained; zero box padding/border and zero stack gap; Start alignment by default. Parent placement controls stretching. |
| Button | Elevated fill; hover/pressed fills; disabled surface/muted outline; 1-unit border (2 in high contrast); 2-unit keyboard focus outline over children | Centered content; no implicit padding. Demo action buttons supply padding 10. |
| ToggleButton | Button chrome; inset accent outline when checked | Padding 10 if caller supplied none; leading/vertically centered label. |
| Checkbox / Switch | No resting outer button border; hover/pressed row fill; checkbox square/check/mixed bar or switch track/thumb; disabled indicator muted | Padding 10, plus 30/48 leading units for indicator. Indicators vertically center even with wrapped labels. |
| ListBox / Select options / Menu | No resting per-option border or button background; full-row selected, hover, pressed fills; one leading marker per row, active color taking precedence over selected color | Rows stretch to parent width, padding 10, leading/vertically centered content. Short labels do not shrink hit areas. Focus does not add nested Button chrome. |
| RadioGroup | Borderless rows plus ring/dot indicator | Same stretching rows; leading padding 40. |
| Select / Disclosure trigger | Bordered button with vector chevron, muted when disabled | Stretch to owning component width; padding 10, right padding 34; vertically centered chevron. |
| Tabs | Padded buttons, selected accent underline | Padding 10; horizontal labels; retained active panel. |
| Stepper | Two ordinary themed buttons with caller-owned content | Width 40/min-height 40 per button, row gap 8, expanding readout. SVGs avoid font coverage/baseline dependence. |
| Slider | Thin border-color track, accent thumb; hover outline, thicker drag outline, outer keyboard focus; disabled/read-only thumb muted | Natural size 160×24 (or 24×160), track thickness 4, thumb 16×20; geometry clamps in tiny bounds. Hover/drag reset on cancellation. |
| ProgressBar | Border-color track and accent fill | Natural 160×16, read-only. |
| TextField / TextArea / NumberField | Elevated input surface, themed text/selection/caret/focus; high-contrast selection outline preserves text readability | Text layout and caret scrolling belong to field; TextArea enables multiline; Field composes caller-supplied label/help. |
| Text / monochrome Vector | Text uses inherited ink by default; Vector must opt into useTheme; disabled ancestors select muted ink | Text needs an explicit font; wrap/fit are explicit. Vector defaults to centered Contain and linear sampling. |
| ScrollView | Authored gray scrollbar, not palette-derived | Vertical, Fill sizing, Auto scrollbar, thickness 8, minimum thumb 16, wheel step 32; Content sizing is opt-in. |
| Popup / Tooltip / Dialog | Elevated surface; dialog optionally dims background | Root portal, viewport-clamped. Popup defaults: match anchor width, gap 4, viewport inset 8, maximum height 320. Dialog centers; Tooltip is passive. |
| Status / FieldGroup | No extra chrome | Status is a polite live region; grouping is semantic, not a forced box decoration. |

Normal palette values (opaque sRGB):

| Role | Light | Dark |
|---|---|---|
| surface / elevated | #f2f4f7 / #ffffff | #181c22 / #252c36 |
| text / mutedText | #17212e / #506074 | #f0f3f7 / #aeb9c8 |
| border / accent | #687a90 / #195bb5 | #748297 / #90bfff |
| hover / pressed | #e0e9f5 / #cbdaf0 | #344153 / #435570 |
| focus / selection | #754400 / #d4e4fa | #ffdb80 / #344d70 |
| onAccent | #ffffff | #102340 |

System appearance is the default. High contrast uses the supplied native palette
when available, otherwise black/white with contrasting accent/focus colors; root
high contrast overrides local palettes. Explicit authored artwork remains opt-out.

Demo 2D keeps help/status outside the controls' scroll view. At width >=800 it
reserves a 300-unit right column; below that, the panel spans available width
above the controls. Outer inset/gap are 16. On short windows the information
panel can scroll independently (narrow mode caps it at 45% of usable height),
so it never obscures controls or consumes their entire viewport. The status
precedes the longer help text. Modal/popup portal ordering is unchanged.

TextEditModel (`ui/TextEdit.hpp`) is SDL-independent. Selections are UTF-8 byte
offsets at ICU grapheme boundaries. Controls and native adapters exchange UIAction
values through UIRoot::performAction. Native queries never traverse live nodes.

<a id="boundaries"></a>
### Transform, Clip, Layer, LayoutBoundary

LayoutBoundary takes a fixed logical extent and one child. `extent/setExtent`
changes its border-box reservation. It exports no descendant baseline and does
not clip; see [cache and isolation contracts](#rendering) for dirty-work behavior.

[Boundaries.hpp](../../include/ui/containers/Boundaries.hpp), all single-child Boxes:

- Transform takes VisualProps and optional BoxProps. Visual transforms do not
  reflow siblings. Pivot is fractional within assigned bounds. The painting
  backend must support the requested transform; singular geometry produces no pixels.
- Clip takes optional clip rectangle and BoxProps; setClipRect changes it.
  Default clip is the **local content box**; explicit rectangle is local.
  ClipProps/ClipPatch holds cornerRadii (zero by default); setClipProps,
  setCornerRadii and setClipShape are available. Painting and hit testing both
  intersect ancestor shapes, not only their axis-aligned bounds.
  Ordinary Node overflow=Clip defaults to its border box; ScrollView clips content.
- Layer takes LayerProps/LayerPatch: cachePolicy=None or WhenUnchanged,
  rasterScale=1 multiplier, byteLimit=16 MiB. Common opacity handles composition.
  dropCache discards only pixels; estimatedCacheBytes reports backing pixel estimate.
  Layout/paint changes in descendants invalidate cached output. Root budget is
  shared, default 64 MiB; cached-layer budget refusal draws uncached.
  Required group-opacity composition cannot fall back to per-child alpha.
  Allocation/backend failures propagate with scoped state/resource cleanup.

<a id="collections"></a>
### ScrollView, AdaptiveStack, Repeat, VirtualList, VirtualGrid

[ScrollView.hpp](../../include/ui/collections/ScrollView.hpp) owns one optional child.
ScrollProps/ScrollPatch: axes=Vertical (Horizontal/Both supported), wheelStep=32,
scrollbar=Auto (Never/Always supported), scrollbarThickness=8, minimumThumb=16,
scrollbarColor={160,160,160,220}, sizing=Fill. Visible scrollbars reserve gutters
inside the border/padding-adjusted area. Content is clipped to the remaining
viewport; tracks and thumbs paint afterward in the scroll view's chrome pass.
`viewportExtent()` excludes gutters; the node's bounds still include them.
Auto gutters are resolved in a bounded pass because one axis can induce overflow
on the other; resizing to fit removes them. Never/zero thickness reserves no space.
setChild/takeChild/child, setOffset/scrollBy, offset/viewportExtent/contentExtent.
scrollIntoView accepts a content-space Rect or descendant Node and optional
Alignment; absent alignment means minimal movement. Here Start/End on x mean
physical left/right because offsets are physical content coordinates.
Content measures unbounded on scrollable axes. Fill requires finite viewport
constraints there. Content sizing reports the child's natural extent clamped to
the offered constraints and also accepts unbounded measurement. Use Content for
content-fitting windows: scrolling is needed only after real viewport constraints
reduce the available size. This does not choose a monitor or resize a window.
Offsets clamp; the entire gutter intercepts hits ahead of content, not only its
thumb. Track clicks move the thumb toward the pointer and can continue as a drag;
cancellation releases capture. Nested wheel
handlers consume available delta and pass residual movement upward.
Clipping all children is not virtualization.

Popups stretch their child to the presented content box. MatchAnchor fixes the
popup width before measuring wrapped content; option rows stretch to the inner
scroll viewport, excluding its gutter. Root portals remain above the underlying
page (including page scrollbars); each portal's own scrollbar is above its own
content. There is no global scrollbar layer above dialogs.

[AdaptiveStack.hpp](../../include/ui/collections/AdaptiveStack.hpp):
AdaptiveStackProps: breakpoints={rules={}, fallback=Vertical}, stack=StackProps{}.
AdaptiveStackPatch patches breakpoints.rules/fallback and stack independently.
This is the renamed Adaptive node; there is no compatibility alias.
Select from externally offered space, not the layout's own result. selectedAxis
reports current choice; child IDs/state survive a row/column switch.
StackPlacement/Patch applies. No automatic content-branch replacement/hysteresis.

[Breakpoints.hpp](../../include/layout/Breakpoints.hpp) provides reusable selection,
not another node or component wrapper:

| Type | Contract |
|---|---|
| SizeRange | minimum={0,0}; maximumWidth/maximumHeight=nullopt; finite nonnegative half-open ranges. Empty/reversed intervals are invalid. contains(SizeConstraints) and overlaps(SizeRange) validate inputs. |
| Breakpoint<Mode> | availableSpace, required mode, optional descriptive name (empty string). Mode is an application-owned value type, commonly an enum. |
| BreakpointProps<Mode> | rules plus required fallback; adjacent ranges allowed, overlapping 2D ranges rejected. |
| BreakpointPatch<Mode> | Keep/Set/Reset for rules and fallback. Reset uses the caller's explicit default props. |
| BreakpointSet<Mode> | Validated constructor, props, setProps, applyPatch(patch, defaults), select(SizeConstraints/Size2). Invalid replacement preserves prior props. |

Selection uses the parent offer's maximum on each axis, or a concrete available
size. An unbounded/unknown axis matches only an unrestricted range axis
(minimum zero, no maximum), not a guessed large screen. No matching rule means
fallback. Selection is pure: custom compositions choose what to patch or rebuild
outside layout/painting, retaining stable nodes when only placement changes.
Validation is O(rules²), selection O(rules). Rules use logical units, not DPI pixels.

[Collection.hpp](../../include/ui/collections/Collection.hpp):
CollectionSource supplies size(), keyAt(index), and revision() (default zero);
keys are unique strings. Revisioned applyChanges(CollectionChangeSet) is optional
alongside existing applyCollectionChanges. See [model deltas](#track-grid).
ItemFactory.create(key) returns unique_ptr<Node>; optional update(node,key)
captures the typed model. There is no any/void* data protocol or automatic observer.
Model snapshots/indexes cost O(n) at construction/membership refresh.

[Repeat.hpp](../../include/ui/collections/Repeat.hpp):
required source/factory; RepeatProps/Patch axis=Vertical, stack defaults,
layout=RepeatLayout::Stack, grid/flow defaults.
setArrangement overloads select Stack(axis,props), Grid(props), Flow(props).
applyCollectionChanges validates keys, preserves surviving instances and updates
retained items through the factory. refreshItem(key) targets changed data.
Membership edits can partially commit if user lifecycle/update callbacks throw;
ownership remains valid, not transactionally rolled back. Same-key different
widget kinds require explicit replacement policy, not unchecked state transfer.

[VirtualList.hpp](../../include/ui/collections/VirtualList.hpp):
required source/factory; VirtualListProps/Patch axis=Vertical,
extentMode=Fixed (or Estimated), itemExtent=32, gap=0, overscan=64, wheelStep=32.
Positive itemExtent; finite viewport on both axes. setOffset/offset, scrollToKey,
refreshItem, applyCollectionChanges, viewportExtent/contentExtent/visibleKeys.

[VirtualGrid.hpp](../../include/ui/collections/VirtualGrid.hpp):
required source/factory; VirtualGridProps/Patch columns=1, cellExtent={80,32},
gap=0, overscan=64, wheelStep=32. Columns and both extents must be positive.
setOffset/offset, scrollToCell(row,column), refreshItem, applyCollectionChanges,
viewportExtent/contentExtent/visibleKeys. Fixed cells only, no spans/content tracks.

Both realize viewport + overscan, temporarily pinning focused/captured items;
removed model keys are not pinned. Persistent selection/edit/game state belongs
in the keyed model, not disposable nodes. Vertical offset begins at top;
horizontal virtual offset begins at logical Start (right in RTL).
Do not wrap in another same-axis unbounded ScrollView.
Estimated lists use [ExtentIndex.hpp](../../include/ui/collections/ExtentIndex.hpp):
Fenwick updates/search O(log n), model edits may rebuild O(n); preserve visible
key + within-item offset when estimates change. At most four correction passes
per layout; further work stays dirty. Empty measured extents get a 0.01-unit
indexing floor. Live nodes are bounded; total model-index memory is not O(visible).

<a id="constraint-layout"></a>
### ConstraintLayout

Sources: [Constraints.hpp](../../include/layout/Constraints.hpp),
[ConstraintLayout.hpp](../../include/ui/containers/ConstraintLayout.hpp). Core values
do not include Kiwi; only the solver container requires playground_constraints.

| API / value | Contract |
|---|---|
| LayoutAnchor | Optional parent-local child key; absent key means parent content box |
| AnchorAttribute | Start/End, Top/Bottom, CenterX/CenterY, Width/Height, Baseline |
| LinearExpression | Constants plus anchor terms; +, -, scalar *, /; no products of unknowns |
| LayoutConstraint | left, relation=Equal, right, strength=Required |
| ConstraintRelation | Equal, LessEqual, GreaterEqual |
| ConstraintStrength | Required, Strong, Medium, Weak |
| append(key,child), takeChild(key) | Unique nonempty local keys; detach refuses still-referenced children |
| addConstraint / removeConstraint | Returns/takes ConstraintId; removal of an unknown ID fails |
| setConstraints(vector) | Validates a whole candidate, returns its newly assigned IDs |
| ConstraintLayoutProps/Patch | maximumPasses=8 (1–64), tolerance=0.01 (positive finite) |

Keys resolve before root attachment, but must already be appended. No cross-tree
references. Start/End select physical edges according to LayoutDirection; offsets
are signed physical coordinates, not automatically mirrored distances. Baseline
uses the child's measured first baseline, falling back to its bottom.

Kiwi solves equalities/inequalities. Each pass rebuilds a local solver:
required nonnegative/min/max dimensions and fixed sizes; strong Fill/Percent
preferences; intrinsic dimensions at strength 0.1; origin preferences 0.001;
soft containment 0.01; unbounded parent minimization 0.0001. A finite offered
maximum is preferred strongly and always bounded by incoming constraints.
These defaults resolve unspecified geometry reproducibly; authored priorities
are not CSS priority/cascade rules. Kiwi soft strengths are numeric weights,
not an arbitrary number of perfectly lexicographic tiers.

Measure intrinsic content, solve, then remeasure at solved widths. Stop at the
configured tolerance/pass limit. Failure to converge throws; child placement
does not start until the complete solve succeeds. Candidate constraints are
checked without a fixed parent size in either LTR or RTL before commitment;
unmeasured child baselines are bounded unknowns rather than guessed bottom edges.
During reflow, a provisional baseline solve may supply measurement widths, but
only a solution consistent with actual measured baselines is committed. A later incompatible
viewport or changed child minimum can still fail layout. Required contradictions
reject the candidate; valid cyclic equations are allowed. User child callbacks
remain nontransactional. Simple parent anchoring remains AnchorLayout.

<a id="track-grid"></a>
### VirtualTrackGrid and explicit model changes

Source: [VirtualTrackGrid.hpp](../../include/ui/collections/VirtualTrackGrid.hpp).

| API / value | Contract |
|---|---|
| GridSource | CollectionSource plus placementAt(index) |
| GridItemPlacement | Zero-based row/column; positive rowSpan/columnSpan (default 1) |
| TrackExtent | value=32, estimated=false, limits={0,unbounded}; fixed or grow-from-estimate |
| VirtualTrackGridProps/Patch | columns={{80}}, rows={{32}}, gap=0, frozen=0, overscan=64, wheelStep=32 |
| FrozenTracks | rowsStart/rowsEnd/columnsStart/columnsEnd counts |
| item(key) | Actual child returned by the factory; realized(key) is its owned Clip wrapper |
| setOffset/offset, scrollToCell | Logical-start offsets, clamped to scrollable content |
| contentExtent / visibleKeys | Current estimates and selected keys; pinned nodes may additionally exist |
| CollectionChangeSet | expectedRevision, resultingRevision, sequential Insert/Erase/Move/Update |
| applyChanges(batch) | Source must already represent resulting revision; validate keys/indices before applying |

The explicit track model requires finite viewport dimensions. Track extents use
indexed prefix sums; estimated tracks only **grow**, capped by limits, from live
intrinsic measurements and width-dependent reflow. A props reset resets estimates.
Never imply that unrealized content was fully measured. Extent indexing has a
0.01-unit floor. Refinement keeps track-relative scroll position; model updates
preserve a surviving visible body key and its within-cell offset when feasible,
then clamp. Deleting that anchor falls back to the clamped old offset.

Row interval indexing includes spans that begin above the viewport. Frozen rows,
columns and corners occupy disjoint clipped panes, without duplicate node
ownership. Spans crossing a frozen boundary fail; frozen extents exceeding the
viewport fail, even for an empty model. Refinement leaves layout dirty for the next
flush, which recalculates visibility instead of caching the old estimated range.
Overlapping model placements paint in source order and pick in
reverse; this container does not perform automatic occupancy placement.
Focused/captured items stay pinned until removed from the source. Parent offsets
and columns mirror for RTL; child-local content is otherwise unchanged.

Model/key/placement indexes are O(model size); explicit edits currently rebuild
those indexes. Revisioned deltas are a correctness/notification interface, not
a claim of O(changes) index updates. Live realization is viewport/overscan/pins;
a long span or a large frozen strip can increase the intersection candidate set.
Repeat, VirtualList and uniform VirtualGrid also accept these change sets.
Only surviving Update requests run a delta's update callback (Update then Erase
does not refresh the erased key); full Repeat refresh updates all retained items.
Consecutive batches do not require an intervening layout.
Callbacks may partially commit before throwing; the attempted node remains dirty,
but callback side effects are not rolled back or automatically retried. Resume with
an explicit refresh as appropriate. There is no whole-description reconciler or
general recycled-widget rebinding protocol.

<a id="text-flow"></a>
### Text flow and truncation

Sources: [TextFlow.hpp](../../include/support/TextFlow.hpp),
[TextColumns.hpp](../../include/platform/sdl/TextColumns.hpp).

TextFlowProps is a TextProps/TextPatch group: truncation=None (EllipsisStart,
EllipsisMiddle, EllipsisEnd), maximumLines=nullopt, ellipsis="…",
writingMode=HorizontalTb, orientation=Mixed. Invalid UTF-8 is rejected.

Font-size selection precedes truncation. Descending candidate selection uses
extended grapheme boundaries and actual font-provider shaping; it does not
assume widths are monotonic or split combining/emoji clusters. Original value
stays intact. If the ellipsis itself cannot fit, show no glyphs. A maximumLines
without ellipsis mode hard-cuts on the same boundaries. Explicit newlines are
measured and rasterized through matching multiline paths. No editor/selection.

VerticalRl/VerticalLr fill columns top-to-bottom and progress right-to-left or
left-to-right. AvailableInlineSize wraps to offered height; maximumLines counts
columns. Paragraph alignment positions runs within columns; contentAlignment
positions the whole result. Vertical text reports no horizontal baseline.
Adjacent same-orientation graphemes shape as runs: Upright uses HarfBuzz TTB,
Sideways shapes horizontally and rotates that run clockwise, Mixed chooses
orientation using Unicode 17 data (enclosing marks keep the cluster upright).
This is not rotation of an entire already-wrapped horizontal paragraph.

Capability limits (errors, not silent approximations):

- Vertical LCD subpixel rendering is rejected.
- Vertical RTL-script/bidi-control paragraphs are rejected.
- Mixed Unicode Tr characters are rejected: the current font provider does not expose enough
  vertical-substitution information to verify their required rotated fallback.
  Explicit Upright/Sideways remains available for those glyphs.
- Vertical breaks are grapheme-safe greedy breaks, not language-specific UAX #14
  typography. Font coverage/vertical alternates remain font-dependent.

Candidate shaping allocates temporary layouts and can be quadratic in string
length. This is suitable for current UI labels, not an optimized document editor.
Unicode data and its license are in support/VerticalOrientation.hpp and
[UNICODE.txt](../licenses/UNICODE.txt).

<a id="svg-styling"></a>
### SVG document styling

Source: [SVGDocument.hpp](../../include/support/SVGDocument.hpp).
VectorSource is an asset-path string or shared immutable SVGDocumentHandle.
SVGStyleOverrides is an ordered element-ID map; SVGElementStyle contains optional
fill/stroke (SVGPaint: ColorRGBA8 or NoSVGPaint), strokeWidth and opacity.
Absent override leaves original data; no-paint explicitly writes none.
Stroke width is finite/nonnegative; opacity is in [0,1].

pugixml parses/copies XML; setters update presentation attributes and trailing
inline declarations. Missing/duplicate IDs, unsupported target kinds and
!important inline cascade overrides fail. Supported targets are svg/g and
path/rect/circle/ellipse/line/polyline/polygon. This is not a full SVG/CSS DOM,
stylesheet cascade, text layout, animation or gradient editing API; rendering
still follows SDL_image's SVG subset.

AssetRegistry::getSVGDocument caches immutable source by path. Styled vector
requests serialize a separate variant; raster cache keys include serialized
document content and dimensions (map hashes are not the equality predicate).
Shared originals never mutate. VectorPatch.source/styles triggers preparation
and intrinsic-size refresh; ContentStyle.paint.tint remains output modulation.
Document entries participate in count trimming and clear; there is no file
watcher/revision invalidation protocol.

<a id="runtime"></a>
## Runtime, input and services

Sources: [UIRoot.hpp](../../include/ui/UIRoot.hpp), [UITypes.hpp](../../include/ui/UITypes.hpp),
[RuntimeServices.hpp](../../include/ui/RuntimeServices.hpp).

| Public surface | Contract |
|---|---|
| setContent/content/resolve | Root ownership and checked root-local lookup |
| defer/flushMutations | Move-only commands at safe boundaries; work enqueued during flush waits |
| update(seconds) | Apply matching completions, advance timers, flush mutations/changes/layout |
| flushLayout(Size2, direction, revision) or LayoutEnvironment | Synchronize dirty geometry |
| prepare() / prepare(PrepareContext) | Defaults to current environment density; explicit context can override |
| render(PaintContext&) | Full visible paint; unchanged nodes still draw after target clear |
| dispatch(UIEvent&) / hitTest(Point2) | Synchronize layout, then route/query; HitResult carries handle, local point, target-to-root IDs |
| requestFocus/focusNext/capturePointer/releasePointer | Focus and pointer routing |
| flushChanges / onChanged | Coalesced committed-prop notifications |
| completionSink | Thread-safe enqueue; delivery on UI thread only |
| inspectTree / stats / diagnostics | Observability |
| needsPaint / requestPaint | Presentation seam, not a damage scheduler |

LayoutEnvironment: viewport required, pixelScale={1,1}, direction=LeftToRight,
usableInsets=0, revision=0. Insets reduce root space; scale must be finite/positive.
Context carries direction, revision, density, services, counters and diagnostics;
ArrangeContext aliases MeasureContext. No allocator arena is injected.

Node customization hooks: measureContent (required), arrangeChildren,
prepareChildren, prepareContent, paint/paintSubtree, onEvent/onDefaultEvent,
onAttach/onDetach/onPropsChanged; containsLocal/containsClip/applyContentClip
customize matching picking and clipping geometry. Public nonvirtual measure/arrange/prepare/render
own validation/traversal/caching. Do not bypass their bookkeeping.

Input order: ancestors capture → target → ancestors bubble → default actions.
handled records consumption; stopPropagation stops routing; preventDefault
suppresses activation/scroll defaults. Stopping propagation alone is not a veto.
Hover enter/leave, pointer cancel, focus gained/lost, wheel and keyboard events
are separate types. Tab uses eligible logical tree order; disabled/hidden/collapsed
subtrees are excluded. Hits use inverse transforms and reverse paint order, honoring
clips and overlay controls. Singular transforms are non-hittable with diagnostics.

UIServices contains scheduler*, diagnostic callback and shared rasterBudget,
not a copied AppContext. Asset services are explicit constructor dependencies.
Scheduler is UI-thread-only; schedule(delay, callback, interval=0) returns TimerHandle.
It advances using supplied dt, fires repeating timers at most once per advance,
and defers newly scheduled work to a later advance. Retain the token or it cancels.
Connection similarly owns a Signal subscription. Both support move-only callbacks;
no claim of allocation-free registration. Disconnect/cancel is noexcept.

CompletionSink::post(handle, sourceRevision, callback) may run on a worker.
Only the delivered callback may access the node; stale root/handle/revision drops
the result. Include any additional request-specific revision in your own job data.
There is no built-in worker pool or automatic async resource loading.

UIRoot's optional CompletionQueueProps defaults to 4096 pending callbacks and
256 attempts per update. post returns false when the mailbox is closed or full;
the producer must choose whether to drop, retry or report the undelivered result.
The UI wrapper delegates storage to runtime::CompletionQueue and adds node/revision
validation; the runtime queue itself can also deliver non-UI results.
Completion delivery attempts at most the configured budget and the queue length
present at update entry. Newly posted callbacks wait for a later update.
Each callback is removed immediately before invocation, outside the mailbox lock.
If it throws, the exception propagates and unattempted callbacks remain queued,
ahead of callbacks posted in the meantime. The failed callback is not retried;
its already-performed side effects are not rolled back. Recursive update is
rejected. The caller may catch the exception and resume with a later update;
AppHost's current top-level failure policy still exits the application.
Platform error translation and AppHost's failure policy are documented in
[the runtime boundary](../render/GPU.md#runtime-contract).

UIWorkStats is cumulative: requests, executed measurements/arrangements, cache
hits/evictions, arrangement skips, invalidation visits, container-plan builds,
text-layout work, font-fit attempts, preparation, painting and realization.
Realized counts attachments, **not** live population. Scratch counters currently
instrument Stack plan/flex reservations only; peak is the largest individual
instrumented plan's capacity, not total live memory or all heap allocations.
`UIRoot::workSample()` includes its stable diagnostic root identity and optional
root-phase timings. UISession supplies deltas to PerformanceMonitor when given
its borrowed monitor in `synchronize`; bounded frame history keeps roots separate.
`reportEveryFrames` controls reporting, not sampling. UI timings overlap host
CPU phases and one another; never add them to the total frame duration.
LayoutDiagnostics retains at most 256 entries: node ID, phase, issue, message.
Invalid inputs generally throw. Ordinary sizing conflicts diagnose and apply their
documented fallback; required ConstraintLayout conflicts and nonconvergence throw,
as do virtual viewports too small for their frozen panes.
InspectTree returns bounds/dirty/semantic snapshots.
Notification failure after commit leaves recoverable dirty work.
Revision counters are 64-bit change tokens, not time; numerical wrap is not a
supported live-session boundary. Generation slots retire before reuse would wrap.

<a id="rendering"></a>
## Rendering, resources and caches

The [2D painting contract](../render/2D.md) owns the complete PaintContext API:
state scopes, transforms, clipping, images, group opacity and capture.
UI nodes request those operations; they do not choose a device, submit commands,
clear the window, or present it. Unsupported operations fail explicitly.

Preparation follows final layout. It composes ancestor affine transforms before
deriving local raster density; rotated content under unequal axis scales cannot
use independent scale multiplication. Layer rasterScale increases descendant
preparation density. Singular nodes skip preparation and painting.
Failed preparation keeps content unready; retry before rendering it again.

Image, Text and Vector retain source/prepared PaintImageHandle values.
Sources are immutable by contract: changed pixels require a new identity.
PrepareContext::images is an optional borrowed ImagePreparer; null means identity
preparation. The returned image must preserve pixel size and alpha interpretation.
Text's paragraph/font semantics and Vector's final-size raster requests are
independent of whether their resulting images stay in CPU memory or are uploaded.

Keep these caches separate:

| Cache | Reuse boundary | Lifetime/budget |
|---|---|---|
| Node measurement | Constraints, dependency/environment revisions, direction, density | Derived per-node state |
| AssetRegistry | Equal authored resource requests and output-affecting properties | Shared handles pin resources; trimming is not a hard total-memory cap |
| Layer | Subtree visual/geometry changes, bounds and density | Root reservation defaults to 64 MiB; per-layer policy applies |
| Backend realization | Source identity and backend/device compatibility | Backend-owned policy; not the root's raster budget |

Node normally retains one measurement. `canReuseMeasurementOffers()` opts a
stateless implementation into two exact offers (currently Rectangle); custom and
virtualized nodes remain conservative. A completed arrangement is reused only
with identical bounds, environment/direction/density and layout revisions.
Text separately retains two density-keyed resolved layouts and one preparation-density
layout. Geometry changes discard them; paint-only color changes do not reshape.
Entry counts are bounded, not a hard total-byte budget on user-supplied strings.

`LayoutBoundary(extent, child)` in `containers/Boundaries.hpp` is a single-child
fixed border-box boundary. It exports no child baseline and accepts nonnegative
finite extents. Children can overflow unless separately clipped. Descendant
layout changes queue the boundary by generation-checked identity; painting,
ancestor layer revisions and overflow still propagate. Extent/box changes on
the boundary itself propagate normally. Nested queued boundaries coalesce under
an ancestor; failed work stays queued. Root traversal/dirty clearing and overflow
maintenance still exist: this is not an O(changed-nodes) scheduler.

Retained raster capture is optional; group opacity is a visual correctness
requirement. If a retained-cache budget refuses an allocation, draw uncached.
Do not substitute per-child alpha for group opacity. Backend allocations may fail;
state/resource scopes must unwind without claiming pixel rollback.
Backend changes require invalidating incompatible prepared assets and Layer caches;
there is no automatic live-backend-switching feature.

Resource preparation and backend ownership details live in
[GPU.md](../render/GPU.md#resource-contracts). Current dependency choices do not
change node identity, parent placement or input-routing contracts.

<a id="integration"></a>
## Host integration

A host supplies a finite LayoutEnvironment, translated UIEvents, elapsed time,
resource services and a compatible PaintContext. Own the UIRoot across frames.
The usual order is environment/input → update/deferred changes → layout →
prepare → render. Pass the painter's scale and imagePreparer to preparation.

An application can own a UIRoot directly. The current platform session adapter is
a convenience implementation of those responsibilities, not a required UI base
class. AppHost passes RenderFrame to IApp::render; the app chooses paint2D() for
its UI. Presentation stays in the host. See
[the host/frame API](../render/GPU.md#runtime-contract) and
[the current adapter](../render/2D.md#platform-adapter).

`UIRoot::preferredSize(maximum, pixelScale)` measures the root under loose bounded
constraints without arranging it. An app can expose this after constructing its
content so the host can fit the window once. It does not continuously resize the
window during layout. Padding/borders are included in the measured result.
Window sizing, Reflow versus FixedCanvas, input mapping and separate raster scale
are specified in [platform/WINDOWING.md](../platform/WINDOWING.md); file settings
and precedence live in [platform/SETTINGS.md](../platform/SETTINGS.md).

Do not retain transient AppContext, RenderFrame, PaintContext or pass contexts.
Retain your model, nodes and resource handles instead. Record app-switch/window
intent during callbacks and let the host process it at a safe boundary.
The UI does not turn an app into a node or give every node host-wide authority.

<a id="cost-model"></a>
## Cost model

Let n be live siblings/nodes as relevant, h tree depth, m model items, v realized
items, t timers, and e cache entries. These describe current algorithms, not
performance promises or measured asymptotic benchmarks.

| Layer | Main cost / consequence |
|---|---|
| Geometry/patch values | Most operations O(1); strings/vectors copy their contents. Integer operators keep normal C++ arithmetic preconditions. |
| Flex stacks | Typical scans are linear; iterative limit freezing can be O(n²). Temporaries are O(n). |
| Flow | Greedy line breaking O(n); flex allocation/remeasurement within lines adds work. No optimal packing solver. |
| Grid | Placement candidates scan occupied cells; spans and track bounds add repeated scans. Cost depends on sparsity, candidate count and span widths, not a simple O(n) guarantee. Plans are rebuilt for measure/arrange. |
| Tree bookkeeping | Invalidation walks ancestors O(h). Whole-root layout/paint and ancestor revisions can make deep trees expensive; cached measurements are not a full incremental dependency scheduler. |
| Picking/routing | Picking may visit all live nodes; routing follows ancestor paths. World transforms are recomputed along ancestry. No spatial index or cached world-matrix system yet. |
| Keyed children | Hash lookup is expected O(1), but vector search/reorder can be O(v²), or O(n²) for a fully realized Repeat. This is local keyed reconciliation, not rebuilding/reconciling the entire UI each frame. |
| Virtual collections | Model/key/index memory O(m), realized nodes roughly viewport + overscan + pinned items. Fenwick update/search O(log m); visible-range prefix queries add O(v log m); membership refresh can rebuild O(m). |
| Scheduler/signals | Timer heap operations O(log t), lazy cancellation; signal emission copies an O(subscriber-count) shared-slot snapshot to allow safe subscription edits. |
| Text/SVG | Grapheme candidate selection and vertical run remeasurement can be quadratic; bounded font-size fitting scans bounded font-size candidates (up to 4096 steps); expensive misses rasterize. SVG work depends on paths and target pixels. Cache hits do not eliminate all measurement/key-construction work. |
| Caches | Hash lookup plus key construction; current LRU trimming repeatedly scans candidates and may be O(e²) when removing many entries. Layer backing cost follows pixels; referenced assets can exceed the nominal budget. |
| ConstraintLayout | Local Kiwi solver rebuilt for each bounded intrinsic pass; no constant-time or global incremental-solver claim. |
| Painting backend | Pixel coverage, clipping, compositing and upload costs are backend-specific; see [2D](../render/2D.md) and [GPU](../render/GPU.md). |
| Host frame | Explicit app activity/demand allows whole-frame idle skipping. A requested frame still clears/draws/presents completely; no partial damage rendering. See [activity](../platform/ACTIVITY.md). |

<a id="scope"></a>
## Scope and extension boundaries

The available UI API is retained and 2D. Renderer availability is tracked separately
in [GPU](../render/GPU.md), [2D](../render/2D.md) and [3D](../render/3D.md).
The following capabilities remain planned; supporting primitives do not imply
an implemented renderer, node or host policy.

| Future family | Retained requirements |
|---|---|
| More controls | Tree view, editable autocomplete, calendar/date inputs, rich editing and spreadsheet interaction remain extensions; the core controls above exist. |
| Partial damage rendering | Whole-frame idle/wake is implemented. Old/new damage bounds, overlap-correct regional repaint and persistent-target ownership remain extensions. |
| Native accessibility | Desktop AccessKit adapters exist; screen-reader acceptance remains platform-specific verification. Mobile bridges and virtualized offscreen item realization are extensions. |
| GPU extensions | GPU 2D/3D, atlas text, bounded recovery, asynchronous timing, ordered batching and budgeted target/residency caches exist. General render graphs, adaptive hardware-memory budgets and further batching optimizations remain. |
| 3D extensions | Scene3D/SceneView, model and rigid-clip import/playback, software/GPU unlit triangles, GPU PBR/IBL, clipping, depth, alpha modes and picking exist. Shadows, skinning, VAT, GPU instancing and spatial acceleration remain. |
| 2D panels in 3D | Independent 2D layout, per-viewport camera and ray-to-panel input conversion |

Whole-description reconciliation is outside the design scope. Stable-key collection
updates are supported, but do not constitute rebuilding the whole UI description.

### Optional extension points

These additions are not required to use the public API.

| Area | Optional work |
|---|---|
| Layout policies | AdaptiveStack hysteresis, unsafe alignment, subgrid/named areas |
| Throughput | Cached Grid/Flow plans and UI world transforms, broader dirty-subtree scheduling, more efficient eviction; explicit LayoutBoundary scheduling already exists |
| Allocation | Measured use of PMR/pools/packed storage and lifetime-safe reusable scratch |
| Resource lifecycle | Asset watching/revisions, broader memory accounting, backend-independent content providers |
| API/runtime convenience | editProps helpers and animation abstractions; bounded Executor and ModelPreparation already exist, further asynchronous loaders need explicit owner-publication contracts |
| Collection reuse | Opt-in recycling keyed by compatible node kind with explicit rebind/reset and subscription cleanup |

<a id="references"></a>
## Background references

### Implementation dependencies

Dependency pins and linking rules are defined in [CMakeLists.txt](../../CMakeLists.txt).

| Dependency | Responsibility |
|---|---|
| [utf8proc](https://juliastrings.github.io/utf8proc/) | UTF-8 validation, Unicode properties and extended grapheme boundaries; not glyph shaping |
| [pugixml](https://pugixml.org/docs/manual.html) | Parse/copy/edit SVG XML before SDL_image rasterizes it |
| [Kiwi](https://kiwisolver.readthedocs.io/en/latest/basis/basic_systems.html) | Header-only linear constraints with required/soft strengths in ConstraintLayout |

utf8proc and pugixml are linked statically through playground_ui_resources;
playground_constraints compiles the Kiwi-backed solver privately, without its
Python bindings. Non-template implementations live under `src/`; public APIs
remain under `include/`. Link playground_ui_core for retained layout/runtime,
playground_sdl for SDL adapters/content, and playground_constraints for the solver.
Unicode 17 vertical-orientation data is a checked-in table, not a runtime library.
SDL_ttf/HarfBuzz still handle glyph shaping; SDL_image still handles SVG rendering.
Dependency licenses are included by the install rules.

### Design and API reading

Inspiration, not standards-conformance claims or replacements for this contract.

- [Apple HIG layout](https://developer.apple.com/design/human-interface-guidelines/layout),
  [Android layout vocabulary](https://developer.android.com/design/ui/mobile/guides/layout-and-content/layout-basics)
- [Swift API guidelines](https://www.swift.org/documentation/api-design-guidelines/),
  [SwiftUI custom layout](https://developer.apple.com/documentation/swiftui/custom-layout),
  [Compose modifier order](https://developer.android.com/develop/ui/compose/modifiers)
- [CSS box model](https://www.w3.org/TR/css-box-3/),
  [sizing](https://www.w3.org/TR/css-sizing-3/),
  [alignment](https://www.w3.org/TR/css-align-3/),
  [flexbox](https://www.w3.org/TR/css-flexbox-1/),
  [grid](https://www.w3.org/TR/css-grid-2/)
- [Yoga incremental layout](https://www.yogalayout.dev/docs/advanced/incremental-layout),
  [React identity/state](https://react.dev/learn/preserving-and-resetting-state)
- [C++ memory resources](https://eel.is/c++draft/mem.res.monotonic.buffer)
- [Kiwi constraints and strengths](https://kiwisolver.readthedocs.io/en/latest/basis/basic_systems.html)
- [utf8proc](https://juliastrings.github.io/utf8proc/), [pugixml manual](https://pugixml.org/docs/manual.html)
- [Unicode vertical orientation](https://www.unicode.org/reports/tr50/),
  [pinned Unicode 17 data](https://www.unicode.org/Public/17.0.0/ucd/VerticalOrientation.txt)
- [SDL_ttf font direction](https://wiki.libsdl.org/SDL3_ttf/TTF_SetFontDirection)
