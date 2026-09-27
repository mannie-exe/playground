# Working with retained UI

For app update/paint cadence, read [application activity](../platform/ACTIVITY.md).
Retained nodes do not require continuous repaint: an app can delegate demand to
its UISession and opt out of continuous update/paint. Keep the session's timers
and completion wake endpoint connected; a clean layout alone is not a reason to
sleep. Repaint requests are acknowledged by successful host presentation, not by
merely clearing a root's paint flag. Animated/custom apps remain continuous until
they provide explicit demand. These policies do not change node composition.

This guide teaches how to think with the retained UI system, from its lifetime
and environment to individual nodes and larger compositions. It is not an API
catalog: use [REFERENCE.md](REFERENCE.md) for exact fields, defaults and limits, and
[TESTING.md](TESTING.md) for module coverage.

Read in order the first time. Each section adds one responsibility to a running
mental model: a panel with a visual preview, a status label and an action. Later
sections describe how that same composition can adapt, scroll and display model
items. There is no separate example application to install or maintain.

## Using the code additions

In the application, acquire a typed resource bundle through AppContext::resources
before constructing a view. Keep durable model data outside nodes; building a
fresh tree from that model is reconstruction, not a reset or reconciliation pass.
Demo2DApp::rebuildView and MinesweeperApp::rebuildView show this separation. The
low-level AssetRegistry parameters below remain useful inside content components.
See [C++ authoring and assets](../platform/ASSETS.md) for catalog and async lifetimes.

Each numbered step adds file-scope C++ declarations to the earlier steps; prior
source is omitted. Paste the blocks in order into one C++23 translation unit if
you want to compile the complete set of helpers. Includes are introduced with
the first step that needs them. There is no main, app registration, or standalone
example target.

Steps 1–11 build a mountable interactive panel. Steps 12–21 add alternative
composition factories: choose them while building a detached subtree rather
than moving children out of an already-mounted tree. Steps 22–25 add independent
component/runtime techniques using the same APIs. Mounting is explicit, not a
side effect of calling a factory.

Supply a live AssetRegistry, a valid FontHandle and resolved sources from your
existing app. The surrounding host owns platform/resource initialization and must
outlive resource usage. These composition examples do not choose a renderer;
Text/Vector use the current resource providers, whose adapter is described in
[2D.md](../render/2D.md#platform-adapter). Keep connection/timer/worker owners
between frames. The prose beside each block states additional preconditions and
the intended call site. [CONTRACTS.md](CONTRACTS.md) summarizes author obligations.

## Reading path

- [First: the runtime and its boundaries](#runtime): lifetime, environment and work.
- [Then: one node and one boundary](#first-node): geometry, props and ownership.
- [Then: content and interaction](#content): text, images, vectors and buttons.
- [Then: composition](#composition): stacks, grids, overlays and constraints.
- [Then: larger populations](#populations): viewports, collections and identity.
- [Finally: reusable behavior and management](#management): components, callbacks,
  background results, caches and diagnosis.

<a id="runtime"></a>
## 1. Establish who owns the environment

The host owns the window, rendering backend, resource services and active app.
An application owns its model and UI composition. A Node is neither an application
nor a window; it should not take over host orchestration because it draws something.

Own a UIRoot across frames. It owns one content node, which can own a hierarchy.
Build that tree when entering the app, attach it with setContent, and keep it.
Construction creates live retained objects, not descriptions awaiting reconciliation.

Asset-backed leaves borrow providers and retain handles. Keep providers alive
longer than the nodes that use them. Release subscriptions/timers before destroying
their borrowed state. Removing a node does not destroy every shared asset it used.

### Logical size is not pixel density

Supply LayoutEnvironment with a logical viewport and positive pixelScale.
Layout uses logical units; image preparation uses output pixels. Direction,
usableInsets and revision belong to the environment too. A higher-density target
should sharpen content, not silently double its logical width.

Choose the surrounding viewport policy separately from the tree. Reflow gives it
the currently available logical size; FixedCanvas preserves a declared logical
canvas and maps it into the window using contain/cover/stretch. The same inverse
mapping must be applied to pointer input. Changing whole-frame render resolution
changes pixel quality, not those layout/hit coordinates. The host's UISession
adapter provides these mappings; do not apply another scale in every child.

For a content-sized window, construct the tree first and expose
`UIRoot::preferredSize(maximum, density)` through the app's preferred-content query.
The host measures once on entry or an explicit fit request, so adding a padded Box
does not require copying its padding into a hard-coded window-size calculation.
It does not repeatedly force the user's window to your content size. See
[window contracts](../platform/WINDOWING.md) for app fields and synchronization,
and [settings](../platform/SETTINGS.md) for project/user TOML overrides.

Native event translation and window/drawable-size queries are host/adapter work.
The UI accepts normalized UIEvent values; there is no window-system event type
in the composition below. See [the SDL adapter](../render/2D.md#platform-adapter)
for wiring the existing AppHost without duplicating this orchestration.

### C++ addition

**Add at file scope.** Keep GuideUI alive between calls. The host supplies the
environment and resource lifetimes; this owner does not initialize a platform.

```cpp
#include <memory>
#include <utility>
#include <ui/UIRoot.hpp>

using namespace playground; // Guide .cpp only; never put this in a header.

struct GuideUI {
  ui::UIRoot root;
};

void synchronizeGuide(GuideUI &guide, const ui::LayoutEnvironment &environment) {
  guide.root.flushLayout(environment);
}
```

## 2. Understand the recurring work

There are four different jobs: **measure**, **arrange**, **prepare**, and **render**.
Measurement asks what content requests under constraints. Arrangement assigns
its actual rectangle. Preparation obtains resources at the resolved size/density.
Rendering paints prepared content through a borrowed PaintContext.

A narrower label can become taller. An SVG needs its fitted destination before
choosing raster resolution. Moving either item need not change its content or
reload its source. A backend upload is another preparation step, not layout.

Synchronize the environment, route input, call update, prepare with the selected
painter's services, then render. UIRoot::prepare synchronizes pending layout when
an environment is known. Clearing/presenting remain host responsibilities.
Retaining nodes or caches does not implement idle or damage-region rendering.

Rendering must not change model state. A button action changes state during input;
drawing reflects it later. Layout may update private derived caches, but must
not trigger an application action just because measurement ran.

### C++ addition

**Add host-facing forwarding functions.** Inputs are already translated. These
are not a second main loop. Do not retain event/pass references after a call.

```cpp
void handleGuideEvent(GuideUI &guide, ui::UIEvent &event) {
  guide.root.dispatch(event);
}

void updateGuide(GuideUI &guide, double seconds) {
  guide.root.update(seconds);
}

void renderGuide(GuideUI &guide, rendering::PaintContext &paint) {
  guide.root.prepare({.pixelScale = paint.pixelScale(),
                      .images = paint.imagePreparer(),
                      .text = paint.textPreparer()});
  guide.root.render(paint);
}

void leaveGuide(GuideUI &guide) {
  guide.root.setContent({});
}
```

Removing content invalidates its node handles. It does not cancel an arbitrary
owner's timers, discard all deferred commands or clear your model automatically;
destroy/reset those owners deliberately on exit.

## 3. Keep three kinds of data distinct

**Props** describe authored choices: padding, color, wrapping or a sizing rule.
**State** describes what is happening: selection, focus, a press or scroll offset.
**Derived results** describe what those choices produced: bounds, measured size
or a cached raster.

For the panel, the chosen preview color can be model state. The Rectangle's fill
prop reflects that choice. Its assigned bounds are a layout result. Do not create
a second authoritative rectangle in the model just to keep the two synchronized.

The model need not live inside an IApp. It belongs with the behavior and lifetime
that own it. App-level setup can assemble defaults and services; reusable layout
decisions can live with the view. You do not need to anticipate every helper
before constructing the first useful composition.

### C++ addition

**Add the model type.** There is no cached rectangle here: selected color and status are authored state, while layout owns geometry.

```cpp
#include <string>
#include <math/Color.hpp>

struct PanelState {
  bool selected{};
  math::ColorRGBA8 previewColor{70, 120, 170, 255};
  std::string status{"Ready"};
};
```

<a id="first-node"></a>
## 4. Give the preview a shape: Rectangle

Start with a `Rectangle` for the preview. Its props control fill, optional border
color and corner radii. It has no intrinsic content size: a color does not tell
layout how wide it wants to be. Supply a size request or let a parent stretch it.

Use `SizeRule::fixed` for an intentionally fixed dimension, `content` for a
measured request, `fill` for a finite offered maximum, and `percent` for a fraction
of a finite offer. Min/max limits bound requests. A request is not a guarantee:
the parent's constraints determine what can actually be assigned.

A `Rect` combines position and size, but they are distinct values. Use `Point2`
for a location, `Size2` for width/height, and `Vec2f` for a displacement or scale.
Use integer vectors for indices or pixel requests. Convert to native types at the
adapter boundary rather than storing platform rectangles throughout the UI.

Layout bounds are relative to the parent's local border origin. The container
already accounts for its content inset. Adding the same padding again in a child
offset is a common reason a composition drifts out of alignment.

### C++ addition

**Add the first content factory.** Calling it returns a new, detached owner; it does not attach or draw. The fixed dimensions make this first leaf visible once arranged.

```cpp
#include <ui/content/Rectangle.hpp>

std::unique_ptr<ui::Rectangle> makePreview(const PanelState &state) {
  return std::make_unique<ui::Rectangle>(
      ui::RectangleProps{.fill = state.previewColor},
      layout::BoxProps{.width = layout::SizeRule::fixed(160),
                       .height = layout::SizeRule::fixed(90)});
}
```

## 5. Give the preview a boundary: Box

Put the Rectangle inside a `Box`. A Box owns zero or one child and contributes a
layout boundary. Give the Box padding to keep its child away from its border;
choose its content alignment to place or stretch that child inside the available
content area.

Keep three spacing decisions separate. Padding belongs inside the parent's
border. Margin belongs to a child's placement in its parent. Gap belongs to a
parent that arranges siblings. A Box's padding does not become an extra margin
on the Rectangle.

This also explains the hybrid composition style. Setting padding twice replaces
one property on one boundary. Nesting two padded Boxes creates two boundaries,
so both paddings participate. Wrapping order has meaning; setter order is not
an ordered chain of layout effects.

A colored Box background does not require a separate Rectangle child. Keep the
Rectangle when it is the content being arranged, or when its own shape/hit
geometry is useful. Use background paint when color is simply part of a boundary.

### Transfer ownership, not the node instance

Construct nodes as `unique_ptr`s and move those owners into parents. Nodes
themselves are noncopyable and nonmovable. Reparenting transfers ownership without
relocating the instance. A builder such as `makeBox` or `makeVStack` is construction
convenience, not a second runtime representation.

Before attachment, a temporary raw pointer can help finish construction while
the owner is known to live. After attachment, obtain a `NodeHandle<T>` when you
need checked lookup later. Handles expire on removal; they do not retain nodes.
A NodeId alone is meaningful only within its root.

### C++ addition

**Add a wrapper and the first mount operation.** The wrapper consumes its argument. Call `mountPreview` outside traversal, then use step 2 to update/render. Replacing root content destroys the previous tree; this is setup, not something to repeat every frame.

```cpp
#include <ui/containers/Box.hpp>

std::unique_ptr<ui::Box> makePreviewBox(std::unique_ptr<ui::Node> content) {
  auto box = std::make_unique<ui::Box>(
      layout::BoxProps{.padding = math::Insets::all(12)},
      ui::BoxContentProps{layout::Alignment::center()});
  box->setBackground(math::ColorRGBA8{30, 30, 30, 255});
  box->setChild(std::move(content));
  return box;
}

void mountPreview(GuideUI &guide, const PanelState &state) {
  guide.root.setContent(makePreviewBox(makePreview(state)));
}
```

## 6. Change authored choices through setters and patches

Change the preview's fill through its props API. Do not modify a private raster,
or mutate props through a borrowed reference. Setters validate changes and mark
the dependent layout or paint work dirty.

Use a complete props value when replacing a group is intentional. Use a patch
when only selected fields should change. `Patch<T>` distinguishes Keep, Set and
Reset: Set can contain false, zero, an empty string or nullopt; none of those
means Keep. Reset uses the field's declared baseline, not whatever happened to
be supplied to this object's constructor. Required fields can reject Reset.

Concrete nodes expose their own `props`, `setProps` and `applyPatch`. Common
groups remain accessible through `setBoxProps`, `applyBoxPatch`, `setPaintStyle`
and their peers. A Button's props are not BoxContentProps even though Button
inherits Box. Prefer the named group APIs when the intended boundary could be
ambiguous.

For temporary absence, choose deliberately: Hidden preserves layout but omits
painting/input; Collapsed also removes layout participation. Neither destroys
the node or its retained state.

### C++ addition

**Add targeted update functions.** The patch changes only fill. The second helper demonstrates Set(nullopt) versus Reset without replacing unrelated box props. Reset padding means the baseline zero padding—not the 12 supplied in step 5.

```cpp
#include <optional>

void setPreviewColor(ui::Rectangle &preview, math::ColorRGBA8 color) {
  preview.applyPatch(
      ui::RectanglePatch{.fill = Patch<math::ColorRGBA8>::set(color)});
}

void clearBoxOverrides(ui::Box &box) {
  box.applyBoxPatch({
      .padding = Patch<math::Insets>::reset(),
      .aspectRatio = Patch<std::optional<float>>::set(std::nullopt)});
}

void hidePreview(ui::Node &preview, bool hide) {
  preview.setVisibility(hide ? ui::Visibility::Hidden : ui::Visibility::Visible);
}
```

<a id="content"></a>
## 7. Add meaning: Text

Add a `Text` node for the panel's status. It takes a registry and a FontHandle,
plus the string and text props. Keep the authored string separate from the
displayed result. Wrapping, fitting and truncation can change what is rendered
without changing the model's text.

There are three separate alignment decisions. The parent places the Text node's
box. `contentAlignment` places the complete text result inside that box.
`paragraphAlignment` aligns lines within the text layout. Changing paragraph
alignment does not center the node in its parent.

Wrapping needs an inline constraint: width for horizontal text, height for
vertical text. `AvailableInlineSize` uses that offer. Font fitting instead changes
font size; `ShrinkToFit` tests bounded candidates against both dimensions without
mutating the original font. Truncation chooses a shorter displayed string at
grapheme boundaries. These are different ways to respond to limited space, not
interchangeable names for scaling.

Choose the policy you want before combining them. Let ordinary status text wrap
when reading all of it matters. Use bounded fitting when the box is fixed but
font size may vary. Use ellipsis when revealing that some content is omitted is
preferable. Clipping is another explicit decision; a failed fit does not imply it.

Text method controls raster treatment: Blended, Solid, Shaded or LCD. A baked
foreground/background change can require a new raster even though layout size
is unchanged. Empty text skips raster creation.

Vertical writing uses columns, not a horizontal paragraph rotated as a whole.
The engine supports upright, sideways and mixed orientation with documented
script/method restrictions. Read the [text-flow contract](REFERENCE.md#text-flow) before
using it for unfamiliar scripts; grapheme-safe output is not a complete
publishing or text-editing engine.

### C++ addition

**Add a shared text factory and an optional overflow policy.** Pass an existing registry-backed font handle. `limitText` changes that node’s policy; it does not mutate the shared font or the model string. Horizontal wrapping/ellipsis still needs a finite offer from the parent.

```cpp
#include <ui/content/Text.hpp>

std::unique_ptr<ui::Text> makeGuideText(AssetRegistry &assets, FontHandle font,
                                      std::string value) {
  return std::make_unique<ui::Text>(
      assets, ui::TextProps{.value = std::move(value),
                           .font = std::move(font),
                           .wrap = ui::TextWrap::AvailableInlineSize});
}

void limitText(ui::Text &text) {
  auto props = text.props();
  props.flow.maximumLines = 2;
  props.flow.truncation = ui::TextTruncation::EllipsisEnd;
  text.setProps(std::move(props));
}
```

## 8. Add a bitmap: Image

The panel can now show a bitmap alongside its status. An `Image` receives a
PaintImageHandle supplied by a resource provider, while the node determines
where and how its pixels appear. A source rectangle selects part of the bitmap.
Asset density describes how many source pixels represent a logical unit.

The parent's assigned box and the image's fitted destination are not necessarily
the same rectangle. Contain preserves aspect inside the box; Cover fills it and
crops; Stretch scales axes independently; None preserves natural size; Shrink
only downsizes. Alignment places the fitted result within spare space. None of
these changes the space the parent allocated to the Image node.

Nearest and Linear choose sampling, not layout or image quality recovery. A
larger destination cannot recover detail absent from a bitmap. Use a vector
source when the artwork should be rerasterized at a new size.

### C++ addition

**Add an image factory.** The caller supplies a decoded or prepared image handle
compatible with its painter. File paths and decoding are not this node's concern;
see [image sources](../render/2D.md#image-sources) for the current adapter.

```cpp
#include <ui/content/Image.hpp>

std::unique_ptr<ui::Image> makeGuideImage(rendering::PaintImageHandle image) {
  return std::make_unique<ui::Image>(
      ui::ImageProps{.image = std::move(image),
                     .content = {.fit = ui::ContentFit::Contain}},
      layout::BoxProps{.width = layout::SizeRule::fixed(160),
                       .height = layout::SizeRule::fixed(90)});
}
```

## 9. Add resolution-independent artwork: Vector

A `Vector` takes a path or immutable SVG document handle and asks the registry
for a raster matching its resolved size and density. Moving it can reuse that
raster; changing its scale can require another. Its ContentStyle uses the same
fit/alignment vocabulary as Image.

For a whole-output color modulation, use tint. For a particular SVG element's
fill or stroke, use the supported ID-based style overrides. Those create an
immutable variant; they do not edit the shared source in place. No-paint and an
absent override mean different things: one writes none, the other leaves the
source declaration alone.

The document provider resolves style edits, a rasterizer produces pixels, and
the registry shares equal requests. The current provider dependencies are listed
in the [reference](REFERENCE.md#references). This is not a browser SVG DOM. Missing IDs and unsupported
style/cascade cases can fail rather than silently approximating the request.

### C++ addition

**Add an SVG factory.** This tiny inline document gives the styling example a known element ID without assuming an external asset. The immutable source and overrides are separate; `mark` exists in the document.

```cpp
#include <support/SVGDocument.hpp>
#include <ui/content/Vector.hpp>

std::unique_ptr<ui::Vector> makeGuideSymbol(AssetRegistry &assets) {
  auto document = std::make_shared<const SVGDocument>(
      R"(<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32">
           <circle id="mark" cx="16" cy="16" r="12" fill="white"/>
         </svg>)");
  SVGStyleOverrides styles;
  styles["mark"].fill = SVGPaint{math::ColorRGBA8{220, 170, 50, 255}};
  return std::make_unique<ui::Vector>(
      assets, ui::VectorProps{.source = document, .styles = std::move(styles)},
      layout::BoxProps{.width = layout::SizeRule::fixed(32),
                       .height = layout::SizeRule::fixed(32)});
}
```

## 10. Add an action: Button

Give the panel an action by placing a content subtree inside `Button`. That child
can be Text, an icon, or a layout containing both. Button does not need separate
built-in label and icon slots because the child tree expresses their composition.

Subscribe with `onActivate` and retain the returned `Connection`. Destroying the
token disconnects the callback. Let the callback change the chosen preview state,
then update the Rectangle and status Text through their setters. Do not rebuild
the entire panel to change a color and a string.

Button owns its interaction state: hover, press, capture, focus and disabled
appearance. Primary press captures; the matching release inside activates.
Release outside cancels. Keyboard activation follows focused Space/Enter release.
Use `setEnabled` to disable the control and clean up its interaction state;
changing only a background color does not disable anything.

Do not capture a transient AppContext reference. If an action must switch apps
or reconfigure the host, record intent in stable state and consume it at the app
boundary with the current context.

### C++ addition

**Add the action factory and its connection helper.** The connection helper expects handles obtained after attachment. Its callback retains model state but not nodes; resolve both handles before doing work. The caller must retain the returned Connection—step 11 provides its owner.

```cpp
#include <ui/controls/Button.hpp>

std::unique_ptr<ui::Button> makeGuideAction(AssetRegistry &assets, FontHandle font) {
  return std::make_unique<ui::Button>(
      makeGuideText(assets, std::move(font), "Toggle preview"),
      ui::ButtonProps{},
      layout::BoxProps{.padding = math::Insets::all(8)});
}

ui::Connection connectGuideAction(ui::Button &button,
                                 ui::NodeHandle<ui::Rectangle> preview,
                                 ui::NodeHandle<ui::Text> status,
                                 std::shared_ptr<PanelState> state) {
  return button.onActivate([preview, status, state = std::move(state)] {
    auto *rectangle = preview.get();
    auto *label = status.get();
    if (!rectangle || !label)
      return;
    state->selected = !state->selected;
    state->previewColor = state->selected ? math::ColorRGBA8{170, 100, 60, 255}
                                          : math::ColorRGBA8{70, 120, 170, 255};
    state->status = state->selected ? "Selected" : "Ready";
    setPreviewColor(*rectangle, state->previewColor);
    label->setValue(state->status);
  });
}
```

<a id="composition"></a>
## 11. Arrange the pieces: Stack, HStack, VStack and Spacer

The preview, status and action now need relationships. Put them in a VStack to
sequence them vertically. Put icon and label in an HStack to sequence them
horizontally. Stack is the axis-selectable form; all three share the same rules.

Set the gap on the parent. Set cross-axis alignment for how children sit across
the sequence. Set each child's placement when it needs margin, a grow/shrink
weight or an alignment override. These are parent-child relationship records,
not extra fields the child must independently keep synchronized.

If the panel becomes taller, ask where the spare space belongs. Grow weights
allocate it to participating children; distribution positions the resulting
sequence within remaining space. A Spacer can absorb spare space without
pretending to be visible content. A plain Stack append gives Spacer growth;
explicit placement controls that behavior.

Fill is a size request, not a grow weight. Fixed dimensions lock their axis.
When content cannot shrink enough, it can overflow; the layout does not solve
the conflict by inventing negative spacing. Text may remeasure at its assigned
width, which is why a status line can change the column's required height.

### C++ addition

**Add the first interactive composition; use it instead of `mountPreview`.** Declare a `MountedPanel` beside your long-lived GuideUI, then assign the result of `mountGuidePanel`. Do not discard that result: it owns the activation subscription. The raw pointers below are construction-only borrows; handles are obtained after the tree is attached. The state argument must be non-null.

```cpp
#include <ui/containers/Stack.hpp>

struct MountedPanel {
  ui::NodeHandle<ui::Rectangle> preview;
  ui::NodeHandle<ui::Text> status;
  ui::Connection activation;
};

MountedPanel mountGuidePanel(GuideUI &guide, AssetRegistry &assets,
                            FontHandle font, std::shared_ptr<PanelState> state) {
  auto preview = makePreview(*state);
  auto *previewNode = preview.get();
  auto status = makeGuideText(assets, font, state->status);
  auto *statusNode = status.get();
  auto action = makeGuideAction(assets, font);
  auto *actionNode = action.get();

  auto row = std::make_unique<ui::HStack>(layout::StackProps{.gap = 8});
  row->append(makeGuideSymbol(assets));
  row->append(std::move(status), {.grow = 1});

  auto column = std::make_unique<ui::VStack>(
      layout::StackProps{.gap = 12,
                         .childrenAlignment = layout::CrossAlignment::Stretch},
      layout::BoxProps{.padding = math::Insets::all(16)});
  column->append(makePreviewBox(std::move(preview)));
  column->append(std::move(row));
  column->append(std::make_unique<ui::Spacer>()); // Absorb extra vertical space.
  column->append(std::move(action));
  guide.root.setContent(std::move(column));

  auto previewHandle = previewNode->handle<ui::Rectangle>();
  auto statusHandle = statusNode->handle<ui::Text>();
  return {previewHandle, statusHandle,
          connectGuideAction(*actionNode, previewHandle, statusHandle, state)};
}

// The host retains this owner; callbacks never borrow constructor locals.
struct InteractiveGuide {
  std::shared_ptr<PanelState> state = std::make_shared<PanelState>();
  GuideUI guide;
  MountedPanel panel; // Destroy subscriptions before destroying guide/state.

  InteractiveGuide(AssetRegistry &assets, FontHandle font)
      : panel{mountGuidePanel(guide, assets, std::move(font), state)} {}
};
```

## 12. Layer the pieces: ZStack

To place a status badge over the preview instead of below it, use a ZStack.
Its children share an offered region and paint in source order. Picking examines
the reverse order. A later child can therefore appear above an earlier one.

An overlay's paint order alone does not decide whether it intercepts input.
Passive Text/Image/Vector leaves normally do not hit-test. An interactive overlay
needs an appropriate hit policy. Keep its parent placement distinct from the
alignment of text inside the overlay.

ZStack is an arrangement, not an offscreen compositing cache. Layer has that
separate responsibility.

### C++ addition

**Add an overlay factory.** This and the following layout factories are alternative building blocks, not commands that stack every layout on the same live tree. Supply fresh detached children before mounting. To adapt step 11, use this factory where it currently builds the preview wrapper; do not move its already-attached preview owner again.

```cpp
#include <ui/containers/ZStack.hpp>

std::unique_ptr<ui::ZStack> makeOverlay(std::unique_ptr<ui::Node> content,
                                       std::unique_ptr<ui::Node> badge) {
  auto layers = std::make_unique<ui::ZStack>();
  layers->append(std::move(content));
  layers->append(std::move(badge),
                 {.alignmentOverride = layout::Alignment{
                      layout::Align::End, layout::Align::Start}});
  return layers;
}
```

## 13. Choose shared tracks or wrapping: Grid and Flow

When several panel actions should share aligned columns and rows, use Grid.
Fixed tracks keep a specified extent; content tracks size from contributions;
fraction tracks share finite remaining space. GridPlacement supplies a cell,
span, margin and optional alignment override. Automatic placement can choose
cells when explicit coordinates would add no meaning.

Spanning an item connects its size contribution to several tracks. Fixed tracks
do not grow merely because the content is large. Explicit overlap needs permission.
Grid is useful for shared alignment; it is not CSS Grid with every CSS feature.

If the actions should simply wrap onto another line as width changes, choose
Flow instead. Flow forms lines from item sizes, then applies its within-line
allocation. It does not align columns across lines or search for an optimal
packing. Use itemGap within a line and lineGap between lines.

This is a choice of relationship, not a hierarchy of better containers: Grid
shares tracks; Flow wraps a sequence; Stack keeps one sequence.

### C++ addition

**Add two alternative collection layouts.** Each function consumes its own child vector; build another vector to try the other layout. Grid shares tracks; Flow wraps independently sized items. These factories reuse the text/image/button factories from earlier steps without changing their implementations.

```cpp
#include <vector>
#include <ui/containers/Grid.hpp>
#include <ui/containers/Flow.hpp>

std::unique_ptr<ui::Grid>
makeTileGrid(std::vector<std::unique_ptr<ui::Node>> items) {
  auto grid = std::make_unique<ui::Grid>(
      layout::GridProps{.columns = {layout::TrackSize::fraction(),
                                   layout::TrackSize::fraction()},
                        .gap = {8, 8}});
  for (auto &item : items)
    grid->append(std::move(item)); // Automatic placement; implicit rows.
  return grid;
}

std::unique_ptr<ui::Flow>
makeWrappingActions(std::vector<std::unique_ptr<ui::Node>> actions) {
  auto flow = std::make_unique<ui::Flow>(
      layout::FlowProps{.itemGap = 8, .lineGap = 8});
  for (auto &action : actions)
    flow->append(std::move(action));
  return flow;
}
```

## 14. Change arrangement without replacing identity: AdaptiveStack

The panel may prefer its preview beside the actions at one width and above them
at another. AdaptiveStack chooses a stack axis from nonoverlapping available-size
conditions. Its existing children survive the choice, keeping their identities
and state.

Define conditions from the space the parent offers, not the size the composition
just calculated for itself. Otherwise the decision could depend on its own
consequence. AdaptiveStack changes arrangement; it does not automatically construct
different content branches or add hysteresis around a threshold.

### C++ addition

**Add a responsive two-child composition.** It switches to a horizontal stack at 600 logical units and keeps the same children. Use it while constructing the panel, not as a per-frame rebuild.

```cpp
#include <ui/collections/AdaptiveStack.hpp>

std::unique_ptr<ui::AdaptiveStack> makeResponsivePair(
    std::unique_ptr<ui::Node> preview, std::unique_ptr<ui::Node> controls) {
  auto pair = std::make_unique<ui::AdaptiveStack>(
      ui::AdaptiveStackProps{
          .breakpoints = {
              .rules = {{.availableSpace = {.minimum = {600, 0}},
                         .mode = layout::Axis::Horizontal}},
              .fallback = layout::Axis::Vertical},
          .stack = {.gap = 12}});
  pair->append(std::move(preview));
  pair->append(std::move(controls), {.grow = 1});
  return pair;
}
```

### Reuse breakpoint decisions outside a stack

AdaptiveStack deliberately decides only row versus column. For a composition
that also changes padding, visibility or a sidebar, choose your own mode enum and
use the same pure `BreakpointSet`. There is no responsive component wrapper to
inherit from. Decide from the parent's available logical size, not the measured
size of the content whose props you are changing.

```cpp
enum class PanelMode { Compact, Expanded };

const layout::BreakpointSet<PanelMode> panelModes{{
    .rules = {{.availableSpace = {.minimum = {720, 0}},
               .mode = PanelMode::Expanded, .name = "roomy"}},
    .fallback = PanelMode::Compact}};

PanelMode choosePanelMode(math::Size2 available) {
  return panelModes.select(available);
}
```

At exactly 720 units this selects Expanded. A maximum, if supplied, is exclusive;
overlapping ranges are rejected instead of relying on declaration order. An
unknown/unbounded width is not evidence that a panel is wide: it selects fallback
unless a rule leaves that axis unrestricted. The same applies to height.

Keep the selected mode in your composition's state if changes require work.
Compare modes, then patch existing nodes during update or a deferred mutation;
do not reconstruct on every frame or mutate the tree while measuring/painting.
For structural changes, decide explicitly which state/identity must survive.
Rules/fallback have typed patch fields; Reset requires explicit default props.

## 15. Place relative to boundaries: AnchorLayout

When the badge should stay at a particular edge or fractional point, AnchorLayout
can express that relationship without hand-updating x/y positions. It relates a
parent fraction to a child fraction plus an offset, or stretches between insets.

It needs meaningful parent space: anchored children do not determine its intrinsic
size. A fixed size and stretch on the same axis conflict. Use the margin on the
placement instead of hiding extra spacing in every offset.

AnchorLayout references its parent, not arbitrary siblings. If the badge must
relate to a sibling's edge or baseline, that is a different tool.

### C++ addition

**Add a bounded anchor composition.** The badge uses its measured size and aligns its end edge with the parent's end edge minus 12. The negative offset is intentional. Give this container finite space; its badge does not determine its intrinsic extent.

```cpp
#include <ui/containers/AnchorLayout.hpp>

std::unique_ptr<ui::AnchorLayout>
makeAnchoredBadge(std::unique_ptr<ui::Node> badge) {
  auto area = std::make_unique<ui::AnchorLayout>(
      layout::BoxProps{.width = layout::SizeRule::fixed(320),
                       .height = layout::SizeRule::fixed(180)});
  area->append(std::move(badge),
               {.horizontal = layout::AnchorPosition::end(-12),
                .vertical = layout::AnchorPosition::start(12)});
  return area;
}
```

## 16. Express sibling relationships: ConstraintLayout

ConstraintLayout gives direct children stable local names and relates their
anchors through linear equalities or inequalities. It can state that one edge
follows another, widths agree, or a dimension stays within a bound.

Required constraints must hold. Strong, Medium and Weak constraints express
preferences with numeric strengths. They are not CSS specificity levels, and
they do not make contradictory required statements valid. Append the named
children before referring to them; references do not cross the container.

The solver remeasures width-dependent content, including baselines, and requires
convergence within a bounded number of passes. It can reject a candidate or fail
layout under an incompatible viewport. Do not use a required equality where you
actually mean a preference. Clear references before detaching a named child.

Keep ordinary sequences and grids as such. Constraints communicate relationships
that those containers do not express; they need not replace every layout rule.

### C++ addition

**Add an alternative sibling-constrained layout.** Here both inputs should have content/flexible widths, not conflicting fixed widths. The first child gets 100 units; the second follows it and reaches the parent's end. These offsets express LTR geometry; they are not an automatically mirrored RTL preset. Link the `playground_constraints` target when using this header.

```cpp
#include <ui/containers/ConstraintLayout.hpp>

std::unique_ptr<ui::ConstraintLayout> makeRelatedPair(
    std::unique_ptr<ui::Node> first, std::unique_ptr<ui::Node> second) {
  using A = layout::AnchorAttribute;
  using R = layout::ConstraintRelation;
  auto anchor = [](std::string key, A attribute) {
    return layout::LinearExpression{layout::LayoutAnchor{std::move(key), attribute}};
  };
  auto parent = [](A attribute) {
    return layout::LinearExpression{layout::LayoutAnchor::parent(attribute)};
  };
  auto group = std::make_unique<ui::ConstraintLayout>();
  group->append("first", std::move(first));
  group->append("second", std::move(second));
  group->setConstraints({
      {anchor("first", A::Start), R::Equal, 0},
      {anchor("first", A::Width), R::Equal, 100},
      {anchor("second", A::Start), R::Equal, anchor("first", A::End) + 8},
      {anchor("second", A::End), R::Equal, parent(A::End)},
      {anchor("first", A::Top), R::Equal, 0},
      {anchor("second", A::Top), R::Equal, anchor("first", A::Top)}});
  return group;
}
```

## 17. Separate placement from visual effects: Transform, Clip and Layer

Wrap the preview in Transform to rotate or scale its appearance without changing
the space siblings receive. The pivot is relative to its assigned bounds. Visual
transforms affect painting and picking; they do not perform another layout pass
to move surrounding content out of the way.

Wrap it in Clip when content should be hidden outside a boundary. Clip's default
boundary is its content box; an explicit shape is local geometry. Rounded
Rectangle paint alone does not clip descendants. The same clip must govern
painting and hit testing so invisible content does not remain clickable.

Wrapper order is meaningful. A transformed child inside an outer Clip moves under
a stationary opening. A Clip inside a transformed parent moves with that parent.
Decide which coordinate space should own the boundary before choosing the order.

Use Layer when the subtree needs optional retained pixel caching. Common opacity
already composites a group correctly; reducing every child's alpha separately
would change overlap appearance. Cache policy WhenUnchanged reuses valid pixels,
but layout, density or paint changes invalidate them. A denied optional cache
falls back to drawing; required opacity composition cannot simply be skipped.

### C++ addition

**Add a visual boundary chain.** The outer clip stays stationary while the inner child rotates. The wrappers use stretch alignment so each boundary receives the same available content region. The optional outer cache does not replace the clip or transform.

```cpp
#include <ui/containers/Boundaries.hpp>

std::unique_ptr<ui::Layer> makePreviewEffects(std::unique_ptr<ui::Node> content) {
  auto rotated = std::make_unique<ui::Transform>(
      std::move(content),
      ui::VisualProps{.transform = math::Transform2D::rotation(0.08f)});
  rotated->setContentAlignment(layout::Alignment::stretch());

  auto clipped = std::make_unique<ui::Clip>(std::move(rotated));
  clipped->setContentAlignment(layout::Alignment::stretch());
  clipped->setCornerRadii(math::CornerRadii::all(8));

  auto cached = std::make_unique<ui::Layer>(
      std::move(clipped),
      ui::LayerProps{.cachePolicy = ui::LayerCachePolicy::WhenUnchanged});
  cached->setContentAlignment(layout::Alignment::stretch());
  cached->setPaintStyle({.opacity = 0.9f});
  return cached;
}
```

<a id="populations"></a>
## 18. Reveal more content through a viewport: ScrollView

If the panel acquires more content than its available area, ScrollView gives it
a finite viewport and a movable content offset. The child measures unbounded on
scrollable axes so its extent can exceed that viewport. Offsets clamp to the
resulting scroll range; scrollIntoView can bring a descendant into view.

Visible scrollbars reserve gutters; content is measured against the remaining
viewport and clipped separately from the scrollbar chrome. Thus option labels
and click targets fill the usable width without painting beneath a thumb.
Nested scroll views
can consume part of a wheel movement and pass the remainder upward. Physical
content-space offsets should not be confused with a child's placement record.

Scrolling does not reduce the number of live children. A clipped thousand-node
column still owns a thousand nodes. Use a virtual collection when population,
rather than just overflow, is the issue.

### C++ addition

**Add a finite scroll boundary.** Its child can be the column or Flow you already know how to construct. Build that child without a same-axis Fill request that depends on a finite maximum; scrolling offers it unbounded height.

```cpp
#include <ui/collections/ScrollView.hpp>

std::unique_ptr<ui::ScrollView>
makeScrollablePanel(std::unique_ptr<ui::Node> content) {
  return std::make_unique<ui::ScrollView>(
      std::move(content), ui::ScrollProps{},
      layout::BoxProps{.width = layout::SizeRule::fixed(480),
                       .height = layout::SizeRule::fixed(240)});
}
```

## 19. Repeat model items: Repeat

Turn the panel's repeated choices into a model with unique stable keys. Supply
a CollectionSource for membership and an ItemFactory to create a node for a key.
The factory can capture a typed model; there is no requirement to erase the model
into void pointers or a universal data map.

Repeat owns all the resulting children and arranges them as a stack, grid or
flow. Refresh membership explicitly after the source changes. Surviving keys
preserve instances; an optional update callback refreshes the retained view's
data. Use refreshItem for a single changed item.

Stable keys represent identity, not just current position. Reordering should not
turn one selected object into another. Reusing a key for an incompatible node
kind needs an explicit replacement decision.

### C++ addition

**Add a small keyed model and reusable item factory.** These swatches are a separate, isolated population rather than another ownership transfer of the panel. The shared model is retained by the factory. Changes still require an explicit collection refresh; modifying `choices` alone sends no notification.

```cpp
#include <stdexcept>
#include <ui/collections/Repeat.hpp>

struct Choice {
  ui::ItemKey key;
  math::ColorRGBA8 color;
};

struct ChoiceSource : ui::CollectionSource {
  std::vector<Choice> choices{
      {"blue", {70, 120, 170, 255}},
      {"orange", {170, 100, 60, 255}},
      {"green", {60, 150, 100, 255}}};
  ui::Revision version{};

  std::size_t size() const override { return choices.size(); }
  ui::ItemKey keyAt(std::size_t i) const override { return choices.at(i).key; }
  ui::Revision revision() const override { return version; }
  math::ColorRGBA8 color(const ui::ItemKey &key) const {
    for (const auto &choice : choices)
      if (choice.key == key)
        return choice.color;
    throw std::out_of_range("Unknown choice");
  }
};

// Source must provide color(key) in addition to the CollectionSource contract.
template <typename Source>
ui::ItemFactory makeChoiceFactory(std::shared_ptr<Source> source) {
  return {
      .create = [source](const ui::ItemKey &key) -> std::unique_ptr<ui::Node> {
        return std::make_unique<ui::Rectangle>(
            ui::RectangleProps{.fill = source->color(key)},
            layout::BoxProps{.height = layout::SizeRule::fixed(32)});
      },
      .update = [source](ui::Node &node, const ui::ItemKey &key) {
        // Safe here because this factory always creates Rectangle.
        setPreviewColor(static_cast<ui::Rectangle &>(node), source->color(key));
      }};
}

std::unique_ptr<ui::Repeat> makeChoices(std::shared_ptr<ChoiceSource> source) {
  return std::make_unique<ui::Repeat>(
      source, makeChoiceFactory(source),
      ui::RepeatProps{.stack = {.gap = 8,
                               .childrenAlignment = layout::CrossAlignment::Stretch}});
}

void recolorFirstChoice(ChoiceSource &source, ui::Repeat &view) {
  source.choices.at(0).color = {200, 80, 120, 255};
  view.refreshItem(source.choices.at(0).key);
}
```

## 20. Bound live nodes: VirtualList and VirtualGrid

VirtualList realizes the visible range plus overscan. Use fixed extents when
every row is predictable, estimated extents when live measurements must refine
the model's initial guesses. Refinement can adjust offsets to preserve a visible
anchor instead of visibly jumping the content.

VirtualGrid provides uniform cells and a column count. It is not the content-track
Grid made automatically virtual: it requires predictable cell extents and does
not support spans. Both virtual containers require finite viewport dimensions;
do not put one inside a same-axis ScrollView that offers unbounded space.

Move persistent selection/edit data into the keyed model. An offscreen node can
be destroyed and recreated. Focus/capture can temporarily pin a node, but deletion
from the source removes that identity. Overscan trades extra nodes for smoother
range changes, not guaranteed zero allocations during scrolling.

Virtualization bounds live view objects, not total storage. Model keys and indexes
still occupy memory proportional to model size.

### C++ addition

**Reuse step 19's source/factory; replace its Repeat factory with one of these.** Both have finite viewport requests and own their scrolling. Do not wrap them in step 18's same-axis ScrollView. A source can contain many items without requiring many live nodes.

```cpp
#include <ui/collections/VirtualList.hpp>
#include <ui/collections/VirtualGrid.hpp>

std::unique_ptr<ui::VirtualList>
makeVirtualChoices(std::shared_ptr<ChoiceSource> source) {
  return std::make_unique<ui::VirtualList>(
      source, makeChoiceFactory(source),
      ui::VirtualListProps{.itemExtent = 32, .gap = 8, .overscan = 64},
      layout::BoxProps{.width = layout::SizeRule::fixed(480),
                       .height = layout::SizeRule::fixed(240)});
}

std::unique_ptr<ui::VirtualGrid>
makeUniformChoices(std::shared_ptr<ChoiceSource> source) {
  return std::make_unique<ui::VirtualGrid>(
      source, makeChoiceFactory(source),
      ui::VirtualGridProps{.columns = 3, .cellExtent = {120, 40}, .gap = {8, 8}},
      layout::BoxProps{.width = layout::SizeRule::fixed(480),
                       .height = layout::SizeRule::fixed(240)});
}
```

## 21. Add variable tracks, spans and frozen panes: VirtualTrackGrid

Use VirtualTrackGrid when the population needs explicit row/column placements,
spans or frozen leading/trailing tracks. GridSource supplies placement as well as
keys. Track extents may be fixed or grow from estimates, capped by their limits.

Frozen panes share ownership of neither duplicate nodes nor duplicate model
items; clipping partitions their visible regions. A span crossing a frozen
boundary is invalid. Frozen extents must fit even when the model is empty.
Estimate refinement can leave another layout flush necessary to update visibility.

Revisioned change sets make model changes explicit for this and the other keyed
collections. Mutate the source first, then describe the sequential inserts,
erasures, moves and updates with expected/resulting revisions. The collection
validates that the batch describes the source. This is not an automatic observer
or a promise that index maintenance costs only the number of changes.

An Update followed by Erase does not refresh the removed key. Callbacks may commit
side effects before throwing; revisions and safe ownership do not imply arbitrary
rollback. Treat retry and recovery as explicit application decisions.

### C++ addition

**Add placement to that same model using an adapter.** The first item spans two columns in the frozen top row. The remaining two occupy the next row; a third track is available for insertion. `appendTrackChoice` demonstrates the source-first delta protocol, outside tree traversal. Call it only once for the key shown, and refresh every view sharing that source if you keep several attached.

```cpp
#include <ui/collections/VirtualTrackGrid.hpp>

struct TrackChoices : ui::GridSource {
  std::shared_ptr<ChoiceSource> data = std::make_shared<ChoiceSource>();
  std::vector<ui::GridItemPlacement> places{{0, 0, 1, 2}, {1, 0}, {1, 1}};

  std::size_t size() const override { return data->size(); }
  ui::ItemKey keyAt(std::size_t i) const override { return data->keyAt(i); }
  ui::Revision revision() const override { return data->revision(); }
  ui::GridItemPlacement placementAt(std::size_t i) const override {
    return places.at(i);
  }
  math::ColorRGBA8 color(const ui::ItemKey &key) const { return data->color(key); }
};

std::unique_ptr<ui::VirtualTrackGrid>
makeTrackChoices(std::shared_ptr<TrackChoices> source) {
  return std::make_unique<ui::VirtualTrackGrid>(
      source, makeChoiceFactory(source),
      ui::VirtualTrackGridProps{
          .columns = {{120}, {120}},
          .rows = {{40}, {40, true}, {40, true}},
          .gap = {8, 8}, .frozen = {.rowsStart = 1}},
      layout::BoxProps{.width = layout::SizeRule::fixed(300),
                       .height = layout::SizeRule::fixed(180)});
}

void appendTrackChoice(TrackChoices &source, ui::VirtualTrackGrid &view) {
  // Stage allocation before changing the live model.
  auto choices = source.data->choices;
  auto places = source.places;
  choices.push_back({"purple", {150, 90, 190, 255}});
  places.push_back({2, 0, 1, 2});
  const auto previous = source.revision();
  const auto next = previous + 1;
  ui::CollectionChangeSet change{
      previous, next,
      {{ui::CollectionOperation::Insert, "purple", choices.size() - 1}}};
  source.data->choices = std::move(choices);
  source.places = std::move(places);
  source.data->version = next;
  view.applyChanges(change);
}
```

<a id="management"></a>
## 22. Package the composition: Component and CustomView

The panel now has enough internal relationships to deserve a name. Component is
a Box with explicit subtree replacement, not a React-like render callback. Let
the component own its internal state and connection tokens, and expose meaningful
operations rather than requiring callers to know its child indexes.

Build with props first, then add helpers where repeated usage reveals a coherent
decision. Keep the base font/resource service outside when shared, and keep local
interaction state inside when private. Do not store every service in every node
just because some descendant might need it.

CustomView supplies measure, paint and event callbacks for a genuinely custom
leaf. It is useful when no existing content atom expresses the behavior. Set a
self hit policy for an interactive leaf; the inherited ChildrenOnly policy will
not target an empty leaf. For reusable typed behavior, subclass and use the
protected hooks rather than replacing the public traversal methods.

### C++ addition

**Package behavior in a new component, independently of the mounted panel.** This small alternative uses CustomView to show how a component owns state read by its child. Calling `setSelected` invalidates the leaf whose paint callback reads that state. No raw pointer into an externally replaceable subtree is retained.

```cpp
class PreviewComponent : public ui::Component {
  PanelState _state;

public:
  PreviewComponent() {
    auto content = std::make_unique<ui::CustomView>(
        ui::CustomViewCallbacks{
            .measure = [](ui::MeasureContext &, const layout::SizeConstraints &) {
              return layout::MeasureResult{{160, 90}};
            },
            .paint = [this](rendering::PaintContext &paint) {
              const auto &child = *children().front();
              paint.fill({{}, child.bounds().size}, _state.previewColor);
            }});
    replaceContent(std::move(content));
  }

  void setSelected(bool selected) {
    _state.selected = selected;
    _state.previewColor = selected ? math::ColorRGBA8{170, 100, 60, 255}
                                   : math::ColorRGBA8{70, 120, 170, 255};
    if (!children().empty())
      children().front()->invalidatePaint();
  }
};
```

## 23. Respect event and mutation boundaries

Input travels through capture, target and bubble routing before default actions.
Handled records consumption; stopPropagation stops routing; preventDefault
suppresses default behavior. They are not synonyms. A custom gesture should not
accidentally leave a Button's default activation enabled underneath it.

Props can change through their APIs, but ownership changes during routing or
layout are restricted. To remove the panel or replace a subtree in response to
an action, enqueue work with root.defer and let the safe boundary apply it.
Collections have a controlled realization phase; that is not permission for
arbitrary measure callbacks to edit the tree.

Use semantic names and roles alongside focus/hit policies. A painted label is
not automatically a semantic label, and semantic metadata is not a native
accessibility bridge. Disabling a subtree excludes it from input; use a control's
own disable API to cancel its associated interaction state too.

### C++ addition

**Add an explicit event veto and deferred removal.** `vetoSecondaryPress` is suitable for a CustomView event callback or a subclass event hook; it does not install itself globally. `connectDismiss` assumes the button belongs to the supplied root. Retain its Connection in the owner alongside `MountedPanel::activation`.

```cpp
void vetoSecondaryPress(ui::UIEvent &event) {
  if (event.type == ui::EventType::PointerDown && event.button == 3) {
    event.handled = true;
    event.preventDefault();
    event.stopPropagation();
  }
}

ui::Connection connectDismiss(ui::UIRoot &root, ui::Button &button) {
  return button.onActivate([&root] {
    root.defer([](ui::UIRoot &owner) {
      owner.setContent({}); // Runs after routing, not inside the button callback.
    });
  });
}
```

## 24. Schedule work and accept background results safely

UIServices exposes runtime services without copying AppContext into the tree.
Use the scheduler for delayed/repeating UI work and retain its TimerHandle.
Elapsed time is supplied by update; repeating timers fire at most once per
advance, rather than replaying an unbounded backlog after a slow frame.

Workers must not dereference NodeHandle to edit nodes. Capture a completion sink,
handle and relevant source revision, compute independent data, then post the
result. The UI thread applies only results whose root, identity and revision
still match. Additional request-specific freshness belongs in your own job data.
The sink is a delivery boundary, not a worker pool. Its bounded queue rejects posts
when full or closed; check the returned bool when delivery matters. UIRoot's optional
CompletionQueueProps selects capacity and per-update work budget. The example below
deliberately permits dropping a nonessential status update, rather than blocking
the worker or mutating UI state from it.

Completion callbacks run outside the queue lock. A throwing callback is not
automatically retried; unattempted callbacks remain queued. Its own partial side
effects remain its responsibility. A caller may recover, but the host's uncaught
exception policy can still exit the application.

### C++ addition

**Add a timer and one background computation.** Retain the TimerHandle or it cancels; retain the jthread in the app/component so its destruction joins the worker at a chosen boundary. Request the handle and revision on the UI thread. The worker captures no node pointer, registry or AppContext. Its result can be dropped if a later edit supersedes it.

```cpp
#include <thread>

ui::TimerHandle schedulePreview(ui::UIRoot &root,
                                ui::NodeHandle<ui::Rectangle> preview) {
  return root.services().scheduler->schedule(0.5, [preview] {
    if (auto *node = preview.get())
      setPreviewColor(*node, {100, 180, 100, 255});
  });
}

std::jthread calculateStatus(ui::UIRoot &root, ui::NodeHandle<ui::Text> target) {
  auto *node = target.get(); // UI-thread lookup, before launching work.
  if (!node)
    return {};
  const auto revision = node->sourceRevision();
  const auto sink = root.completionSink();
  return std::jthread([sink, target, revision](std::stop_token stop) {
    unsigned total{};
    for (unsigned i = 0; i < 1000; ++i) {
      if (stop.stop_requested())
        return;
      total += i;
    }
    auto message = std::string{"Computed: "} + std::to_string(total);
    (void)sink.post(target, revision,
                    [message = std::move(message)](ui::Text &text) {
                      text.setValue(message); // Delivered by root.update().
                    });
  });
}
```

## 25. Diagnose before adding another abstraction

When placement is wrong, inspect the offer, the assigned border box, its content
inset and the parent placement in that order. When text seems misaligned, identify
which of node placement, content alignment and paragraph alignment you changed.
When an image seems incorrectly scaled, separate box size, fit result, source
density and raster resolution.

When updates appear stale, ask which derived result should have been invalidated.
Use inspectTree, diagnostics and stats instead of duplicating state to force a
refresh. Measured, arranged, prepared and painted are different counters; realized
counts attachments, not current live population.

There are three cache layers to reason about. Node measurement caches save layout
work. AssetRegistry shares equal resource requests. Layer caches save subtree
pixels. Their keys and lifetimes differ. Shared asset handles can keep entries
alive beyond a trim request; a pixel budget is not a measurement of all RAM use.

Only then choose a stronger mechanism: virtualization for too many live nodes,
Layer for reusable pixels, or a different layout relationship for awkward manual
positioning. The [reference cost model](REFERENCE.md#cost-model) explains where repeated
work remains. Focused [module tests](TESTING.md) should establish the contract
being changed, without expanding into an entire sample application.

### C++ addition

**Add an opt-in inspection function.** Run it on the UI thread after synchronization/layout. It prints observed geometry, flags and work counters rather than modifying state to force a refresh. Include formatter support explicitly.

```cpp
#include <iostream>
#include <format>
#include <math/GeometryFormatters.hpp>
#include <ui/UIFormatters.hpp>

void describeGuide(const ui::UIRoot &root) {
  for (const auto &node : root.inspectTree())
    std::cout << std::format("{}: bounds={}, dirty={}\n",
                             node.name.value_or("unnamed"),
                             node.bounds, node.dirty);
  const auto &stats = root.stats();
  std::cout << std::format("measured={}, cacheHits={}, prepared={}, painted={}\n",
                           stats.measured, stats.measureCacheHits,
                           stats.prepared, stats.painted);
  for (const auto &diagnostic : root.diagnostics().entries())
    std::cout << diagnostic.message << '\n';
}
```

## Rendering is a separate contract

The composition above needs a compatible PaintContext, not a specific window
surface or GPU. Image preparation may keep source pixels or realize them for a
device. Both paths preserve layout dimensions and source identity rules.

Read [2D.md](../render/2D.md) for painting and host-adapter usage,
[GPU.md](../render/GPU.md) for resource ownership and queued work, and
[3D.md](../render/3D.md) for a separate scene service whose output can become an
image. Those services do not add camera/depth responsibilities to UI nodes.

## Add a scene viewport without changing UI layout

A scene is an application model, not a special layout tree. Keep its owner across
frames; a viewport node holds shared read access. Mutate the scene from update or
routed callbacks, then let the normal layout/preparation pass rebuild changed
output. A stable SceneView caches its image until its scene, camera, size or renderer
changes. Merely moving its box does not upload the mesh again.

The following additions build a 3D preview. The surrounding IApp should declare
rendererRequirements.scene3D=true and render its UISession with render(frame),
not only render(frame.paint2D()), so preparation receives the scene service.

```cpp
#include <scene/Scene3D.hpp>
#include <ui/content/SceneView.hpp>
#include <rendering/RenderBackend.hpp>

struct GuideScene {
  std::shared_ptr<scene::Scene3D> world = std::make_shared<scene::Scene3D>();
  scene::ObjectId triangle;

  GuideScene() {
    auto mesh = scene::makeMesh({
        {{{-1, -1, 0}}, {{1, -1, 0}}, {{0, 1, 0}}}, {0, 1, 2}});
    triangle = world->create({.mesh = mesh,
                              .material = {.baseColor = {80, 160, 255, 255}}});
  }

  std::unique_ptr<ui::SceneView> makeView() const {
    return std::make_unique<ui::SceneView>(
        ui::SceneViewProps{.scene = world, .preferredSize = {320, 240}},
        layout::BoxProps{.width = layout::SizeRule::fill(),
                         .height = layout::SizeRule::fill()});
  }

  void rotate(float radians) {
    auto props = world->props(triangle);
    props.transform.orientation = math::axisAngle({0, 1, 0}, radians);
    world->setProps(triangle, props);
  }
};

void renderGuideScene(GuideUI &guide, rendering::RenderFrame &frame) {
  auto &painter = frame.paint2D();
  guide.root.prepare({.pixelScale = painter.pixelScale(),
                       .images = painter.imagePreparer(),
                       .scenes = frame.scene3D(),
                       .text = painter.textPreparer()});
  guide.root.render(painter);
}
```

For an affine 2D world, use Scene2D and Scene2DView instead. The camera maps world
coordinates into the content box; item transforms remain world-authored. Neither
scene class adds game rules, polling or another main loop.

```cpp
#include <scene/Scene2D.hpp>
#include <ui/content/Scene2DView.hpp>

std::unique_ptr<ui::Scene2DView> makeGuide2DScene(
    std::shared_ptr<scene::Scene2D> world) {
  world->create({.bounds = math::rect(20, 20, 80, 40),
                  .paint = {.tint = {255, 160, 60, 255}}});
  return std::make_unique<ui::Scene2DView>(
      ui::Scene2DViewProps{.scene = std::move(world)});
}
```

## Add authored vector geometry

Use Path when you own the geometry and want solid fills or strokes without an
intermediate SVG bitmap. The path's coordinates belong to its viewBox; layout
assigns the box, and fit/alignment map that viewBox into it. A larger layout box
does not change the authored points. Curves flatten to bounded segments at the
current density, and either painter computes coverage from those segments.

```cpp
#include <ui/content/Path.hpp>

std::unique_ptr<ui::Path> makeGuideCurve() {
  math::Path2D outline;
  outline.moveTo({4, 28}).cubicTo({4, 0}, {60, 0}, {60, 28})
      .lineTo({60, 44}).lineTo({4, 44}).close();
  return std::make_unique<ui::Path>(ui::PathProps{
      .path = std::move(outline), .viewBox = math::rect(0, 0, 64, 48),
      .paint = {.fill = math::ColorRGBA8{30, 100, 200, 255},
                .stroke = math::ColorRGBA8{255, 255, 255, 255},
                .strokeWidth = 2}});
}
```

NonZero and EvenOdd select the fill rule; fill implicitly closes open contours.
Strokes follow actual contour closure and use round joins/caps. Empty fill or
stroke means no such operation. ViewBox clipping is deliberate, so leave room
for strokes extending beyond their centerline. This is not an SVG/CSS importer:
use Vector for documents requiring styles, filters, gradients or SVG parsing.

## Further reading

### Content-fitting screens and numeric controls

When a window should fit its content, let the root measure the composition and
let AppHost cap the result to the display work area. A ScrollView with
`ScrollSizing::Content` reports natural size instead of expanding to every offered
pixel. It retains the full content extent when the real viewport is smaller.
Do not introduce an arbitrary board-size threshold for scrolling; window bounds
and content bounds already determine whether it is needed.

Stepper composes two ordinary Buttons around readout content you provide. Its
props own the current integer, inclusive limits, positive step, enabled state and
semantic name. Connect onValueChanged to your app-owned draft and readout; keep
the Connection alive. Programmatic setProps is silent, so refreshing controls
from the draft does not create a feedback loop. Set a compatible value together
with changed bounds; clamping a game-specific draft is the application's choice.

Keep screen navigation as pending app intent. Replace the tree after routing or
update, preserve the old model until its view is destroyed, then request content
fitting. Assign initial focus after layout: focus eligibility requires arranged
nodes. See [Minesweeper's screen contract](../platform/APPLICATIONS.md).

### Choosing an isolation boundary

Use a Box for content-dependent sizing. Use LayoutBoundary when the surrounding
layout must reserve an explicit extent regardless of the descendant's content:
construct it with a logical Size2 and one owned child. It does not expose a child
baseline or clip overflow. Put a Clip around it when clipping is desired. Changing
the boundary extent still informs its parent; changing text inside it schedules
local layout without resizing neighboring content. This is not a promise that all
root bookkeeping becomes proportional only to changed nodes.

For updates, `settings()` means the common Node groups. `contentProps()` means
Box alignment, `buttonProps()` means Button appearance/enablement, and
`gridProps()` means Grid tracks. These do not hide one another through inheritance.
Content nodes retain their own `props()` and `applyPatch()`. Patch itself is the
lower-level `playground::Patch<T>`; it is also usable by non-UI values.

To observe work, inspect UIRoot::stats() or workSample(). UIWorkStats accumulates
work counters; optional timings describe overlapping root operations. Pass the
host's PerformanceMonitor to UISession::synchronize to collect per-root frame
deltas. Compare counts under identical workloads before attributing a cost to
layout, text preparation or painting. The tracked Stack scratch reservations are
only part of memory use, not an allocation profiler for the whole program.

Use [the node index](REFERENCE.md#nodes) for available atoms and their exact properties,
[runtime contracts](REFERENCE.md#runtime) for lifecycle and failure behavior, and
[scope boundaries](REFERENCE.md#scope) for capabilities that are not provided. Build up
only the relationships your composition needs; nesting every available node is
not a goal. The useful outcome is knowing who owns each decision, which state it
changes, and which work follows from that change.

## Add accessible controls without a second state tree

Use `UISession::synchronize(ctx)` in AppHost applications. It attaches the current
root to window-owned clipboard, IME and accessibility services. Continue calling
update even when not painting: native actions, timers, focus and snapshots are
runtime work. The metrics-only overload remains useful for offscreen consumers;
it intentionally does not invent a native window or AppContext.

Give each control a useful name, or wrap it in Field with a visible label/help
node. Set values through the control's props/model; do not maintain a parallel
semantic checked/value state. Keep user callbacks scoped with Connection. This
independent composition reuses the guide's AssetRegistry/FontHandle:

```cpp
#include <ui/controls/Composite.hpp>
#include <ui/controls/Slider.hpp>
#include <ui/controls/TextField.hpp>

std::unique_ptr<ui::Node> makeAccessibleFields(AssetRegistry &assets,
                                             FontHandle font) {
  auto fields = std::make_unique<ui::FieldGroup>(
      "Preferences", layout::StackProps{
          .gap = 12, .childrenAlignment = layout::CrossAlignment::Stretch});
  auto name = std::make_unique<ui::TextField>(
      ui::TextFieldProps{.font = font, .required = true}, "Player");
  auto caption = std::make_unique<ui::Text>(
      assets, ui::TextProps{.value = "Player name", .font = font});
  fields->append(std::make_unique<ui::Field>(
      std::move(name), std::move(caption), nullptr,
      ui::FieldProps{.label = "Player name"}));
  fields->append(std::make_unique<ui::Slider>(ui::SliderProps{
      .range = {50, 0, 100, 1}, .name = "Volume"}));
  fields->append(std::make_unique<ui::TextArea>(
      ui::TextFieldProps{.font = std::move(font), .name = "Notes"},
      "Plain multiline notes"));
  return fields;
}
```

For modal content, retain a Dialog with its logical owner and change its open prop.
It is presented centered at root level, not inside its parent's scroll clip.
Do not manually disable all sibling controls: UIRoot restricts routing and the
semantic snapshot to the active modal scope, then restores a valid opener.
Put application Escape navigation in an AfterUI input context so a field can
cancel composition, a Select can close, or a Dialog can dismiss first.

TextField accepts committed UTF-8, not characters reconstructed from keycodes.
It owns transient selection/composition and bounded history. Use onValueChanged
for edits and onCommit for a submitted value; setters are silent. NumberField's
committed number is separate from its potentially invalid draft. A password field
does not expose plaintext through the semantic snapshot or clipboard copy.

Demo2D's scrollable gallery exercises every current control family. Try Tab and
Shift+Tab, D-pad/arrows, mouse drag/release, disabled controls, nested selection,
IME composition and Escape. Native reader testing is a separate pass; inspect
names/values rather than assuming a visually useful control is already announced
correctly. See [ACCESSIBILITY.md](ACCESSIBILITY.md) for exact boundaries.

### Let controls size themselves and inherit appearance

Use Flow for a row of independent controls that can move to another line. Give
labels AvailableInlineSize wrapping and controls padding, but leave their height
content-driven. A fixed height is a deliberate restriction, not a request to
automatically fit more lines. For images use ContentFit::Contain with centered
alignment when the complete image must remain visible. A Fill ScrollView occupies
the offered viewport instead of imposing a fixed preferred page width.

AppViewPolicy's colorScheme defaults to System. UISession resolves the current
system/user appearance into a palette; ordinary Text and controls inherit it.
Opt your page root into `setPaintStyle({.themeBackground=true})`. Transparent UI
over a scene should not opt in. Use `useTheme=false` for deliberately colored
artwork; do not hard-code label colors just to obtain readable default controls.
Local `setTheme` overrides are useful for a themed panel, but system high contrast
takes priority. Color scheme and contrast are independent settings.

Select's trigger keeps its place in layout while its list appears in an overlay.
Arrows change the highlighted candidate, not the committed selection; Enter or
click commits, Escape cancels. The existing selection callback is still where you
update the visible trigger label. Tooltip also uses root presentation: setAnchor
to its owner's attached NodeId, then control setOpen from your own timing policy.
Resolve IDs after attachment, for example during the owner's arrangement, rather
than storing constructor-time empty IDs. Popup content stays owned by its component;
never detach and reparent it into a second live tree to display it above a clip.
