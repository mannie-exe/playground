#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

#include <world/Store.hpp>

namespace playground::world {
namespace {
constexpr std::uint64_t magic =
    0x31444c524f574750ULL; // PGWORLD1, little-endian.

struct Writer {
  std::size_t maximum, used{};
  bool retain{};
  std::vector<std::byte> bytes;

  void raw(std::span<const std::byte> value) {
    if (value.size() > maximum - used)
      throw std::length_error("World checkpoint exceeds encoded limit");
    used += value.size();
    if (retain)
      bytes.insert(bytes.end(), value.begin(), value.end());
  }

  void number(std::uint64_t value) {
    std::array<std::byte, 8> encoded;
    for (unsigned i = 0; i < 8; ++i)
      encoded[i] = std::byte((value >> (i * 8)) & 255);
    raw(encoded);
  }

  void scalar(double value) { number(std::bit_cast<std::uint64_t>(value)); }

  void vector(Vec3d value) {
    scalar(value.x);
    scalar(value.y);
    scalar(value.z);
  }

  void rotation(math::Quaternion value) {
    scalar(value.x);
    scalar(value.y);
    scalar(value.z);
    scalar(value.w);
  }

  void blob(std::span<const std::byte> value) {
    number(value.size());
    raw(value);
  }

  void text(std::string_view value) { blob(std::as_bytes(std::span{value})); }

  void space(SpaceId value) { number(value.value); }

  void local(LocalPose value) {
    vector(value.offset);
    rotation(value.orientation);
  }
};

struct Reader {
  std::span<const std::byte> bytes;
  WorldId world;
  std::size_t position{};

  std::span<const std::byte> raw(std::size_t count) {
    if (count > bytes.size() - position)
      throw std::invalid_argument("Truncated world checkpoint");
    auto result = bytes.subspan(position, count);
    position += count;
    return result;
  }

  std::uint64_t number() {
    std::uint64_t result{};
    const auto value = raw(8);
    for (unsigned i = 0; i < 8; ++i)
      result |= std::uint64_t(std::to_integer<unsigned>(value[i])) << (i * 8);
    return result;
  }

  std::size_t count(std::size_t maximum) {
    const auto value = number();
    if (value > maximum || value > bytes.size() - position)
      throw std::length_error("World checkpoint field exceeds bound");
    return static_cast<std::size_t>(value);
  }

  bool flag() {
    const auto value = number();
    if (value > 1)
      throw std::invalid_argument("Invalid checkpoint boolean");
    return value != 0;
  }

  double scalar() {
    const auto result = std::bit_cast<double>(number());
    if (!std::isfinite(result))
      throw std::invalid_argument("Nonfinite checkpoint scalar");
    return result;
  }

  Vec3d vector() { return {scalar(), scalar(), scalar()}; }

  math::Quaternion rotation() {
    const auto component = [&] {
      const auto value = scalar();
      if (std::abs(value) > std::numeric_limits<float>::max())
        throw std::invalid_argument("Checkpoint rotation exceeds float range");
      return static_cast<float>(value);
    };
    return math::normalizedRotation(
        {component(), component(), component(), component()});
  }

  std::vector<std::byte> blob(std::size_t maximum) {
    const auto value = raw(count(maximum));
    return {value.begin(), value.end()};
  }

  std::string text(std::size_t maximum) {
    const auto value = raw(count(maximum));
    return {reinterpret_cast<const char *>(value.data()), value.size()};
  }

  SpaceId space() { return {world, number()}; }

  LocalPose local() { return {vector(), rotation()}; }
};

void validateMetadata(const CheckpointMetadata &metadata,
                      const WorldStoreProps &props) {
  if (metadata.contentLock.size() > 4096 ||
      metadata.records.size() > props.maxRecords)
    throw std::invalid_argument("Invalid checkpoint content metadata");
  std::set<std::string_view> keys;
  for (const auto &record : metadata.records)
    if (record.key.empty() || record.key.size() > 256 || !record.schema ||
        !keys.insert(record.key).second ||
        record.bytes.size() > props.maxCheckpointBytes)
      throw std::invalid_argument("Invalid checkpoint domain record");
}

void encode(Writer &out, const WorldSnapshot &world,
            const FrameSnapshot &frames, const CheckpointMetadata &metadata) {
  out.number(magic);
  out.number(WorldStore::schemaVersion);
  out.number(world.id().value);
  out.number(world.revision());
  out.number(world.tick());
  out.text(metadata.contentLock);
  out.number(world.spaces().size());
  for (const auto &space : world.spaces()) {
    out.space(space.id);
    out.scalar(space.limits.maximumCoordinate);
    out.scalar(space.limits.positionTolerance);
    out.scalar(space.limits.maximumLocalCoordinate);
    out.scalar(space.limits.localTolerance);
  }
  out.number(world.entities().size());
  for (const auto &entity : world.entities()) {
    out.number(entity.id.value);
    out.number(entity.revision);
    out.number(entity.discontinuity);
    out.number(entity.destroyed);
    const auto &p = entity.props;
    out.space(p.pose.position.space);
    out.vector(p.pose.position.meters);
    out.rotation(p.pose.orientation);
    out.space(p.velocity.space);
    out.vector(p.velocity.linear);
    out.vector(p.velocity.angular);
    out.number(unsigned(p.activation));
    out.blob(p.data);
    out.number(bool(p.attachment));
    if (p.attachment) {
      out.space(p.attachment->frame.space);
      out.number(p.attachment->frame.value);
      out.local(p.attachment->local);
      out.vector(p.attachment->linear);
      out.vector(p.attachment->angular);
    }
  }
  out.number(frames.records().size());
  for (const auto &frame : frames.records()) {
    out.space(frame.id.space);
    out.number(frame.id.value);
    out.number(frame.removed);
    const auto &p = frame.definition;
    out.number(bool(p.parent));
    if (p.parent) {
      out.space(p.parent->space);
      out.number(p.parent->value);
    }
    out.local(p.local);
    out.vector(p.linear);
    out.vector(p.angular);
  }
  out.number(metadata.records.size());
  for (const auto &record : metadata.records) {
    out.text(record.key);
    out.number(record.schema);
    out.blob(record.bytes);
  }
}
} // namespace

void WorldStoreProps::validate() const {
  world.validate();
  if (maxCheckpointBytes < 128 || maxCheckpointBytes > 1024ULL * 1024 * 1024 ||
      !maxRecords || maxRecords > 65536 || !maxFrames || maxFrames > 65536 ||
      world.maxEntities > 65536 || world.maxSpaces > 4096)
    throw std::invalid_argument("Invalid checkpoint limits");
}

WorldStore::WorldStore(std::shared_ptr<CheckpointStorage> storage,
                       std::shared_ptr<runtime::ResourceLedger> ledger,
                       WorldStoreProps props)
    : _storage{std::move(storage)}, _ledger{std::move(ledger)}, _props{props} {
  _props.validate();
  if (!_storage || !_ledger)
    throw std::invalid_argument("World store requires storage and accounting");
}

void WorldStore::addMigration(std::uint64_t schema,
                              std::function<std::vector<std::byte>(
                                  std::span<const std::byte>, std::size_t)>
                                  migrate) {
  if (schema == schemaVersion || !migrate || _migrations.size() >= 16 ||
      _migrations.contains(schema))
    throw std::invalid_argument("Invalid checkpoint migration registration");
  _migrations.emplace(schema, std::move(migrate));
}

WorldSaveResult WorldStore::save(const WorldSnapshot &world,
                                 const FrameSnapshot &frames,
                                 const CheckpointMetadata &metadata,
                                 std::uint64_t expected) {
  if (frames.world().id() != world.id() ||
      frames.world().version() != world.version() ||
      frames.records().size() > _props.maxFrames ||
      world.spaces().size() > _props.world.maxSpaces ||
      world.entities().size() > _props.world.maxEntities)
    throw std::invalid_argument(
        "Checkpoint frames and world must share one boundary");
  validateMetadata(metadata, _props);
  for (const auto &entity : world.entities()) {
    if (entity.props.data.size() > _props.world.maxEntityBytes)
      throw std::length_error("Checkpoint entity exceeds domain byte limit");
    if (!entity.destroyed && entity.props.attachment)
      frames.resolve(entity.props.attachment->frame);
  }
  Writer out{_props.maxCheckpointBytes};
  encode(out, world, frames, metadata);
  auto charge = _ledger->reserve(runtime::MemoryClass::CPU,
                                 runtime::ResourceKind::Preparation,
                                 _props.maxCheckpointBytes + out.used * 2 + 64,
                                 "World checkpoint encoding and storage",
                                 {world.id().value, world.epoch(), 4});
  out.bytes.reserve(out.used);
  out.used = 0;
  out.retain = true;
  encode(out, world, frames, metadata);
  const auto revision = _storage->publish(world.id(), expected, out.bytes,
                                          _props.maxCheckpointBytes);
  if (expected == std::numeric_limits<std::uint64_t>::max() ||
      revision != expected + 1)
    throw std::runtime_error("Checkpoint storage violated revision contract");
  return {revision, world.version(), world.tick()};
}

std::unique_ptr<RestoredWorld> WorldStore::load(WorldId id) {
  if (!id.value)
    throw std::invalid_argument("World checkpoint identity required");
  auto staging = _ledger->reserve(
      runtime::MemoryClass::CPU, runtime::ResourceKind::Preparation,
      _props.maxCheckpointBytes * 3, "World checkpoint decode and migration",
      {id.value, 0, 4});
  auto stored = _storage->read(id, _props.maxCheckpointBytes);
  if (!stored)
    return {};
  if (!stored->revision || stored->bytes.size() > _props.maxCheckpointBytes)
    throw std::invalid_argument("Invalid stored checkpoint envelope");
  Reader header{stored->bytes, id};
  if (header.number() != magic)
    throw std::invalid_argument("Unknown world checkpoint format");
  const auto schema = header.number();
  if (schema != schemaVersion) {
    const auto migrate = _migrations.find(schema);
    if (migrate == _migrations.end())
      throw std::invalid_argument("Unsupported world checkpoint schema");
    auto candidate = migrate->second(stored->bytes, _props.maxCheckpointBytes);
    if (candidate.size() > _props.maxCheckpointBytes)
      throw std::length_error("Checkpoint migration exceeds limit");
    stored->bytes = std::move(candidate);
  }
  Reader in{stored->bytes, id};
  if (in.number() != magic || in.number() != schemaVersion ||
      in.number() != id.value)
    throw std::invalid_argument("Checkpoint migration/identity mismatch");
  WorldArchive archive;
  archive.id = id;
  archive.revision = in.number();
  archive.tick = in.number();
  auto result = std::make_unique<RestoredWorld>();
  result->metadata.contentLock = in.text(4096);
  auto count = in.count(_props.world.maxSpaces);
  archive.spaces.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
    archive.spaces.push_back(
        {in.space(), {in.scalar(), in.scalar(), in.scalar(), in.scalar()}});
  count = in.count(_props.world.maxEntities);
  archive.entities.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    EntityRecord entity;
    entity.id = {id, in.number()};
    entity.revision = in.number();
    entity.discontinuity = in.number();
    entity.destroyed = in.flag();
    auto &p = entity.props;
    p.pose = {{in.space(), in.vector()}, in.rotation()};
    p.velocity = {in.space(), in.vector(), in.vector()};
    const auto mode = in.number();
    if (mode > unsigned(Activation::Full))
      throw std::invalid_argument("Unknown saved activation mode");
    p.activation = Activation(mode);
    p.data = in.blob(_props.world.maxEntityBytes);
    if (in.flag()) {
      const auto space = in.space();
      const auto frame = in.number();
      p.attachment = FrameAttachment{
          {space, 0, frame}, in.local(), in.vector(), in.vector()};
    }
    archive.entities.push_back(std::move(entity));
  }
  result->world = std::make_unique<World>(id, _ledger, _props.world);
  result->world->restore(archive, result->world->snapshot().version());
  const auto snapshot = result->world->snapshot();
  count = in.count(_props.maxFrames);
  std::vector<FrameMutation> frames;
  frames.reserve(count * 2);
  for (std::size_t i = 0; i < count; ++i) {
    FrameId frame{in.space(), snapshot.epoch(), in.number()};
    const bool removed = in.flag();
    ReferenceFrame definition;
    if (in.flag())
      definition.parent = FrameId{in.space(), snapshot.epoch(), in.number()};
    definition.local = in.local();
    definition.linear = in.vector();
    definition.angular = in.vector();
    frames.emplace_back(CreateFrame{frame, definition});
    if (removed)
      frames.emplace_back(RemoveFrame{frame});
  }
  result->frames = std::make_unique<ReferenceFrames>(
      snapshot, _ledger,
      ReferenceFramesProps{_props.maxFrames, _props.maxFrames * 2, 64});
  result->frames->apply(snapshot, frames, result->frames->snapshot().version());
  count = in.count(_props.maxRecords);
  result->metadata.records.reserve(count);
  std::size_t owned =
      sizeof(RestoredWorld) + result->metadata.contentLock.size();
  for (std::size_t i = 0; i < count; ++i) {
    PersistentRecord record;
    record.key = in.text(256);
    record.schema = in.number();
    record.bytes = in.blob(_props.maxCheckpointBytes);
    owned += sizeof(PersistentRecord) + record.key.size() + record.bytes.size();
    result->metadata.records.push_back(std::move(record));
  }
  validateMetadata(result->metadata, _props);
  if (in.position != in.bytes.size())
    throw std::invalid_argument("Trailing world checkpoint data");
  result->charge =
      _ledger->splitReservation(staging, owned, runtime::ResourceKind::World);
  result->charge->setState(runtime::AllocationState::Owned);
  result->storeRevision = stored->revision;
  return result;
}
} // namespace playground::world
