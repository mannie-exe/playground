# Navigation and autonomous movement

Navigation plans routes and follows them toward app-selected goals. Goal selection,
behavior trees and game rules remain app concerns. Path planning, local avoidance,
[locomotion](LOCOMOTION.md) and [physical movement](PHYSICS.md) are separate stages.
An autonomous actor uses the same movement-request/actual-result boundary as a
player; the camera is not its heading or movement authority.

## API and representations

| API | Contract |
|---|---|
| `NavigationProfile` | Agent clearance, height, slope/step limits, allowed areas/traversals and positive costs |
| `NavigationSnapshot` | Space, epoch/tick, tile/link revisions, coverage and profile compatibility |
| `NavigationRequest` | Agent/request identity, start/goal, profile, projection tolerance and work/output limits |
| `NavigationService::request / cancel / poll` | Bounded asynchronous planning with retained terminal results |
| `NavigationPath` | Owned corridor/waypoints, segment frames, required tiles/links and revisions |
| `NavigationLink` | Directed connection, cost, traversal kind and explicit readiness/permission requirements |
| `PathFollower::advance` | Read actual pose and current corridor; produce desired travel/facing or a stop/traversal request |
| `NavigationState` | Planning, Following, WaitingForData, Traversing, Arrived, Blocked, Failed or Cancelled |

Support explicit waypoint/weighted graphs, 2D/voxel-derived walkable grids and
tiled ground navigation meshes behind the same request boundary. Preparation uses
authored navigation geometry/area tags or voxel occupancy, not material visibility.
Providers report representation and query capabilities; no claim of arbitrary
flying/swimming volume navigation follows from ground routes. Those volume planners
are future extensions; authored aerial waypoint graphs remain ordinary graphs.

Grid search uses bounded A* with nonnegative edge costs and an admissible heuristic
when promising optimal cost. Stable tie-breaking and neighbor order support replay
fixtures; optimality is limited to the supplied representation and cost model.
Mesh providers own corridor generation and agent-clearance validation. Library
handles stay private. Recast/Detour is a candidate adapter, not a mandated new
dependency in this documentation pass.

## Planning and missing data

Result status is Complete, Partial, NoPath, NeedsData, Unsupported, BudgetExceeded,
Cancelled or Failed. Complete reaches the accepted goal tolerance; Partial never
means Arrived. NoPath is valid only for a searched, complete relevant domain under
the stated profile. Missing tiles yield NeedsData/Partial with missing coverage,
not a proof of impossibility. Budget exhaustion is separate from both.

Requests choose whether to return available prefixes or wait for admitted data.
Tile requests use explicit bounded streaming sources/leases; path queries do not
perform hidden disk/network reads. Planning reserves expansions, frontier bytes,
jobs and output points. Superseded requests retain resources until retired, and
world epoch, agent generation and goal revision reject late publication.

Start/goal projection is explicit and bounded; results report projected endpoints.
No silent snapping across walls, spaces or disconnected layers. Profiles validate
clearance and traversals independently of the render mesh. A route is a plan over
declared data, not proof that a moving physical body can execute it without contacts.

## Following, frames and execution

Path following outputs a desired travel direction/velocity and facing target on
the simulation clock. It adapts to the selected locomotion policy; Tank cannot be
made to strafe by overwriting its pose. Progress uses actual movement, not requested
velocity or interpolated render position. Arrival requires measured position within
goal tolerance and completed mandatory traversals. Stalls produce bounded retries,
Blocked or failure; do not teleport an actor to make a route succeed.

Paths retain only a bounded usable corridor; distant segments may require replanning.
Tile eviction/replacement invalidates dependent segments. Validate before use and
stop before unavailable/invalid segments. A topology-preserving moved frame can
transform a frame-local segment using a consistent FrameSample. Boarding/alighting
uses an explicit traversal handler with readiness, deadline, cancellation and
completion results; graph connectivity never performs the movement itself.
Cross-space links invoke WorldTransfer, with destination readiness and authority
checks, rather than interpolating coordinates between spaces.

Kinematic followers work without physics over declared navigable data. Local
avoidance is a separate optional capability returning a bounded preferred velocity;
unsupported avoidance is observable. Crowd avoidance, physical character motors
and vehicle dynamics are future implementations. Neither a path nor an avoidance
velocity guarantees collision safety.

## Changes and verification

Voxel edits, blocked areas, door/link state and profile changes invalidate affected
tiles/paths by revision. A temporary local obstacle does not automatically alter
the global route graph: the app declares whether to stop, avoid or rebuild topology.
Tile preparation publishes at a simulation boundary, with bounded dirty regions
and coalesced rebuild demand. Never rebuild every tile because the camera moved.

Measure planning latency/expansions, queue age, retained tile bytes, invalidations,
replans and stalled agents. Verify graph/grid/mesh routes, narrow clearances,
negative coordinates, disconnected layers, missing tiles, edits, cancellation,
moving frames and transfer failure in [world tests](WORLD_TESTING.md).

[Godot navigation](https://docs.godotengine.org/en/stable/tutorials/navigation/navigation_introduction_3d.html)
and [Recast/Detour](https://recastnav.com/) inform the distinction between planning,
tile data, traversal execution and avoidance. They do not define Playground's ABI.
