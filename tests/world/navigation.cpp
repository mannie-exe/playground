#include <array>
#include <chrono>
#include <thread>

#include <support/Test.hpp>
#include <world/Navigation.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

int main() {
  return test::run([] {
    auto ledger = std::make_shared<runtime::ResourceLedger>();
    World world{{105}, ledger};
    const SpaceId space{{105}, 1}, destination{{105}, 2};
    const EntityId actor{{105}, 1};
    const CellId tile{space, 1};
    const std::array<WorldMutation, 3> seed{
        CreateSpace{{space, {}}}, CreateSpace{{destination, {}}},
        SpawnEntity{actor, {{{space, {0, 0, 0}}, {}}, {space}}}};
    world.apply(seed, world.snapshot().version(), 1);
    {
      auto empty = navigationMesh(tile, 1, {}, true, ledger);
      NavigationSnapshot emptyMesh{world.snapshot(), {&empty, 1}, ledger};
      NavigationRequest noRoute{
          world.handle(actor), 1, {space, {0, 0, 0}}, {space, {1, 0, 0}}};
      require(planNavigation(emptyMesh, noRoute, ledger).status ==
                  NavigationStatus::NoPath,
              "explicit complete empty tile differs from missing data");
      NavigationSnapshot absent{world.snapshot(), {}, ledger};
      require(planNavigation(absent, noRoute, ledger).status ==
                  NavigationStatus::NeedsData,
              "absent coverage is not an empty traversable world");
    }
    auto node = [&](unsigned id, double x, double z = 0) {
      NavigationNode result;
      result.id = {tile, id};
      result.position = {space, {x, 0, z}};
      return result;
    };
    auto edge = [&](unsigned id, unsigned from, unsigned to, double cost) {
      NavigationLink result;
      result.id = {tile, id};
      result.from = {tile, from};
      result.to = {tile, to};
      result.cost = cost;
      return result;
    };
    NavigationTile graph;
    graph.id = tile;
    graph.nodes = {node(1, 0), node(2, 1), node(3, 2), node(4, 1, 1)};
    graph.links = {edge(1, 1, 2, 10), edge(2, 2, 3, 1), edge(3, 1, 4, 1),
                   edge(4, 4, 3, 1)};
    NavigationSnapshot snapshot{world.snapshot(), {&graph, 1}, ledger};
    NavigationRequest request{
        world.handle(actor), 1, {space, {0, 0, 0}}, {space, {2, 0, 0}}};
    auto result = planNavigation(snapshot, request, ledger);
    require(result.status == NavigationStatus::Complete &&
                result.path->cost == 2,
            "weighted graph uses optimal authored cost instead of geometric "
            "shortcut");
    require(result.path->points[1].position.meters.z == 1,
            "stable route uses cheap detour");
    const auto repeated = planNavigation(snapshot, request, ledger);
    require(repeated.path->points[1].position ==
                    result.path->points[1].position &&
                repeated.expansions == result.expansions,
            "identical inputs preserve tie/expansion ordering");
    auto budgeted = request;
    budgeted.budget.expansions = 1;
    require(planNavigation(snapshot, budgeted, ledger).status ==
                NavigationStatus::BudgetExceeded,
            "search expansion exhaustion is not no-path");
    budgeted = request;
    budgeted.budget.points = 2;
    require(planNavigation(snapshot, budgeted, ledger).status ==
                NavigationStatus::BudgetExceeded,
            "waypoint output is independently bounded");
    budgeted = request;
    budgeted.budget.projectionVisits = 1;
    require(planNavigation(snapshot, budgeted, ledger).status ==
                NavigationStatus::BudgetExceeded,
            "endpoint projection is bounded too");
    auto wide = request;
    wide.profile.radius = .6;
    require(planNavigation(snapshot, wide, ledger).status ==
                NavigationStatus::NoPath,
            "agent clearance filters graph geometry");
    auto stale = request;
    ++stale.agent.epoch;
    require(planNavigation(snapshot, stale, ledger).status ==
                NavigationStatus::Failed,
            "stale incarnation rejected");
    std::stop_source cancelled;
    cancelled.request_stop();
    require(planNavigation(snapshot, request, ledger, cancelled.get_token())
                    .status == NavigationStatus::Cancelled,
            "cooperative cancellation produces a terminal outcome");

    graph.links.clear();
    graph.revision = 2;
    NavigationSnapshot disconnected{world.snapshot(), {&graph, 1}, ledger};
    require(planNavigation(disconnected, request, ledger).status ==
                NavigationStatus::NoPath,
            "complete disconnected graph proves no route");
    require(!result.path->valid(disconnected),
            "tile revision invalidates retained corridor");
    graph.complete = false;
    ++graph.revision;
    NavigationSnapshot missing{world.snapshot(), {&graph, 1}, ledger};
    require(planNavigation(missing, request, ledger).status ==
                NavigationStatus::NeedsData,
            "incomplete coverage never proves no path");
    auto partial = request;
    partial.allowPartial = true;
    auto prefix = planNavigation(missing, partial, ledger);
    require(prefix.status == NavigationStatus::Partial &&
                !prefix.path->complete && prefix.missing == tile,
            "available prefix is explicit and names missing coverage");
    PathFollower prefixFollower;
    prefixFollower.follow(prefix.path);
    require(prefixFollower
                    .advance(world.snapshot().sample(world.handle(actor)), .1,
                             missing)
                    .state == NavigationState::WaitingForData,
            "partial route cannot claim arrival");

    std::array<NavigationGridCell, 9> cells;
    cells[4].walkable = false;
    auto grid = navigationGrid(tile, 4, {space, {-2, 0, -2}}, 3, 3, 1, cells,
                               true, ledger);
    NavigationSnapshot gridSnapshot{world.snapshot(), {&grid, 1}, ledger};
    auto gridRequest = request;
    gridRequest.start = {space, {-1.5, 0, -.5}};
    gridRequest.goal = {space, {.5, 0, -.5}};
    auto gridPath = planNavigation(gridSnapshot, gridRequest, ledger);
    require(gridPath.status == NavigationStatus::Complete &&
                gridPath.path->cost == 4,
            "negative-coordinate grid routes around blocked occupancy");
    for (auto point : gridPath.path->points)
      require(point.position.meters != Vec3d{-.5, 0, -.5},
              "blocked cell never appears in route");
    auto narrow = gridRequest;
    narrow.profile.radius = .51;
    require(planNavigation(gridSnapshot, narrow, ledger).status ==
                NavigationStatus::NoPath,
            "grid clearance does not guess a wider neighborhood");

    auto point = [&](double x, double z) {
      return WorldPosition{space, {x, 0, z}};
    };
    const std::array triangles{NavigationTriangle{WorldTriangle{
                                   {point(0, 0), point(4, 0), point(0, 4)}}},
                               NavigationTriangle{WorldTriangle{
                                   {point(4, 0), point(4, 4), point(0, 4)}}}};
    auto mesh = navigationMesh(tile, 5, triangles, true, ledger);
    NavigationSnapshot meshSnapshot{world.snapshot(), {&mesh, 1}, ledger};
    auto meshRequest = request;
    meshRequest.start = point(1, 1);
    meshRequest.goal = point(3, 3);
    auto meshPath = planNavigation(meshSnapshot, meshRequest, ledger);
    require(meshPath.status == NavigationStatus::Complete &&
                meshPath.path->projectedStart == meshRequest.start &&
                meshPath.path->projectedGoal == meshRequest.goal,
            "ground mesh projects onto triangles and connects shared portals");
    require(meshPath.path->points.size() == 5 &&
                meshPath.path->points[2].position == point(2, 2),
            "mesh corridor travels through explicit shared edge midpoint");
    auto tall = meshRequest;
    tall.profile.height = 3;
    require(planNavigation(meshSnapshot, tall, ledger).status ==
                NavigationStatus::NoPath,
            "mesh headroom is explicit");
    const std::array duplicate{triangles[0], triangles[0], triangles[0]};
    test::rejects([&] { navigationMesh(tile, 6, duplicate, true, ledger); },
                  "non-manifold mesh rejected");

    PathFollower stalled{{.stallSeconds = .2}};
    stalled.follow(result.path);
    auto actual = world.snapshot().sample(world.handle(actor));
    require(stalled.advance(actual, .1, snapshot).desired.linear != Vec3d{},
            "follower requests motion without moving actual pose");
    stalled.advance(actual, .1, snapshot);
    require(stalled.advance(actual, .1, snapshot).state ==
                NavigationState::Blocked,
            "requested motion is not counted as actual progress");
    PathFollower follower;
    follower.follow(gridPath.path);
    actual.pose.position = gridRequest.start;
    bool arrived{};
    for (unsigned i = 0; i < 500; ++i) {
      const auto step = follower.advance(actual, .02, gridSnapshot);
      if (step.state == NavigationState::Arrived) {
        arrived = true;
        break;
      }
      actual.pose.position =
          translated(actual.pose.position, step.desired.linear * .02);
      actual.velocity = step.desired;
      ++actual.tick;
    }
    require(arrived &&
                length(relativeTo(actual.pose.position, gridRequest.goal)) <=
                    gridRequest.goalTolerance,
            "kinematic follower arrives from measured motion");
    follower.follow(gridPath.path);
    actual.pose.position = gridRequest.start;
    follower.advance(actual, .02, gridSnapshot);
    ++actual.discontinuity;
    require(follower.advance(actual, .02, gridSnapshot).state ==
                NavigationState::WaitingForData,
            "teleport invalidates prior progress");

    runtime::Executor executor;
    {
      NavigationTile departure, arrival;
      departure.id = tile;
      departure.nodes = {node(1, 0)};
      const CellId far{destination, 1};
      arrival.id = far;
      auto endpoint = node(1, 0);
      endpoint.id = {far, 1};
      endpoint.position = {destination, {8, 0, 0}};
      arrival.nodes = {endpoint};
      auto link = edge(1, 1, 1, 1);
      link.to = endpoint.id;
      link.traversal = TraversalKind::Transfer;
      departure.links = {link};
      std::array tiles{std::move(departure), std::move(arrival)};
      NavigationSnapshot transfers{world.snapshot(), tiles, ledger};
      auto crossing = request;
      crossing.goal = endpoint.position;
      crossing.profile.traversals |= traversalMask(TraversalKind::Transfer);
      auto route = planNavigation(transfers, crossing, ledger);
      require(route.status == NavigationStatus::Complete,
              "explicit cross-space link can be planned");
      PathFollower crossingFollower;
      crossingFollower.follow(route.path);
      auto subject = world.snapshot().sample(world.handle(actor));
      auto step = crossingFollower.advance(subject, .1, transfers);
      require(step.state == NavigationState::Traversing &&
                  step.traversal->link == link.id &&
                  step.desired.linear == Vec3d{},
              "connectivity requests transfer without moving the subject");
      crossingFollower.completeTraversal(link.id, false);
      require(crossingFollower.advance(subject, .1, transfers).state ==
                  NavigationState::Blocked,
              "failed destination preparation stops the follower");
      crossingFollower.follow(route.path);
      crossingFollower.advance(subject, .1, transfers);
      subject.pose.position = endpoint.position;
      subject.velocity.space = destination;
      ++subject.discontinuity;
      crossingFollower.completeTraversal(link.id, true);
      require(crossingFollower.advance(subject, .1, transfers).state ==
                  NavigationState::Arrived,
              "confirmed transfer uses actual destination pose before arrival");
    }
    {
      ReferenceFrames frames{world.snapshot(), ledger};
      const FrameId frame{space, world.snapshot().epoch(), 1};
      const std::array<FrameMutation, 1> create{
          CreateFrame{frame, {{}, {{10, 0, 0}, {}}}}};
      frames.apply(world.snapshot(), create, frames.snapshot().version());
      NavigationTile platform;
      platform.id = tile;
      platform.nodes = {node(1, 0), node(2, 1)};
      platform.nodes[0].frame = FramePosition{frame, {0, 0, 0}};
      platform.nodes[1].frame = FramePosition{frame, {1, 0, 0}};
      platform.links = {edge(1, 1, 2, 1)};
      const auto frameSnapshot = frames.snapshot();
      NavigationSnapshot moving{
          world.snapshot(), {&platform, 1}, ledger, {}, &frameSnapshot};
      auto onboard = request;
      onboard.start = {space, {10, 0, 0}};
      onboard.goal = {space, {11, 0, 0}};
      auto route = planNavigation(moving, onboard, ledger);
      require(route.status == NavigationStatus::Complete,
              "frame-local nodes sample their captured world boundary");
      const std::array<FrameMutation, 1> move{
          SetFrame{frame, {{}, {{20, 0, 0}, {}}}}};
      frames.apply(world.snapshot(), move, frames.snapshot().version());
      auto subject = world.snapshot().sample(world.handle(actor));
      subject.pose.position = {space, {20, 0, 0}};
      PathFollower aboard;
      aboard.follow(route.path);
      auto moved = frames.snapshot();
      auto step = aboard.advance(subject, .1, moving, &moved);
      require(step.state == NavigationState::Following &&
                  step.desired.linear.x > 0,
              "topology-preserving moving frame remaps the retained corridor");
      auto jump = std::get<SetFrame>(move[0]);
      jump.discontinuity = true;
      const std::array<FrameMutation, 1> teleport{jump};
      frames.apply(world.snapshot(), teleport, frames.snapshot().version());
      auto jumped = frames.snapshot();
      require(aboard.advance(subject, .1, moving, &jumped).state ==
                  NavigationState::WaitingForData,
              "frame discontinuity cannot blend or follow stale local history");
    }
    for (bool closed : {false, true}) {
      runtime::Executor unavailable{{.maxReservedBytes = 1}};
      if (closed)
        unavailable.close();
      NavigationService blocked{snapshot, unavailable, ledger};
      const auto id = blocked.request(request);
      blocked.advance(runtime::ActivityClock::now());
      require(blocked.poll(id).status == NavigationStatus::Failed &&
                  !blocked.demand().pending && !blocked.demand().wakeAt,
              "impossible or closed navigation executor terminates planning");
    }
    NavigationService service{snapshot, executor, ledger};
    auto first = service.request(request);
    auto superseding = request;
    ++superseding.goalRevision;
    auto second = service.request(superseding);
    require(service.poll(first).status == NavigationStatus::Cancelled,
            "superseded goal has retained cancellation");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (service.poll(second).status == NavigationStatus::Planning &&
           std::chrono::steady_clock::now() < deadline) {
      auto consumed = service.advance({}, {1, 4096, 1});
      require(consumed.operations <= 1 && consumed.messages <= 1 &&
                  consumed.bytes <= 4096,
              "navigation publication honors work grant");
      std::this_thread::yield();
    }
    require(service.poll(second).status == NavigationStatus::Complete,
            "asynchronous planning publishes retained immutable result");
    runtime::ServicePump pump;
    auto scope = pump.scope();
    auto handle = service.attach(scope);
    auto pending = service.request(superseding);
    scope.close();
    require(service.poll(pending).status == NavigationStatus::Cancelled &&
                !service.demand().pending && !service.demand().wakeAt,
            "scope shutdown prevents late navigation publication");
    LocalAvoidance avoidance;
    const AvoidanceRequest noSolver{
        world.snapshot().sample(world.handle(actor)),
        {space, {1, 0, 0}},
        .016,
        3};
    const auto unsupported = avoidance.evaluate(noSolver);
    require(unsupported.status == AvoidanceStatus::Unsupported &&
                unsupported.velocity == noSolver.preferred,
            "missing avoidance backend does not claim collision safety");
  });
}
