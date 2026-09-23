#include <ui/collections/Collection.hpp>

namespace playground::ui::detail {

KeyedChildren::KeyedChildren(std::shared_ptr<const CollectionSource> source,
                             ItemFactory factory, layout::BoxProps box)
    : Node{box}, _source{std::move(source)}, _factory{std::move(factory)} {
  if (!_source || !_factory.create)
    throw std::invalid_argument(
        "Collection source and item factory are required");
  _keys = collectionKeys(*_source);
  _sourceRevision = _source->revision();
  _availableKeys.insert(_keys.begin(), _keys.end());
}

void KeyedChildren::replaceKeys(std::vector<ItemKey> keys) {
  std::unordered_set<ItemKey> available(keys.begin(), keys.end());
  _keys = std::move(keys);
  _availableKeys = std::move(available);
}

void KeyedChildren::validateChanges(const CollectionChangeSet &batch) const {
  if (batch.expectedRevision != _sourceRevision ||
      batch.resultingRevision <= batch.expectedRevision ||
      _source->revision() != batch.resultingRevision)
    throw std::invalid_argument("Collection change revision mismatch");
  auto keys = _keys;
  for (const auto &change : batch.changes) {
    auto it = std::find(keys.begin(), keys.end(), change.key);
    switch (change.operation) {
    case CollectionOperation::Insert:
      if (it != keys.end() || change.index > keys.size())
        throw std::invalid_argument("Invalid collection insertion");
      keys.insert(keys.begin() + change.index, change.key);
      break;
    case CollectionOperation::Erase:
      if (it == keys.end())
        throw std::invalid_argument("Missing erased collection key");
      keys.erase(it);
      break;
    case CollectionOperation::Move:
      if (it == keys.end() || change.index >= keys.size())
        throw std::invalid_argument("Invalid collection move");
      keys.erase(it);
      keys.insert(keys.begin() + change.index, change.key);
      break;
    case CollectionOperation::Update:
      if (it == keys.end())
        throw std::invalid_argument("Missing updated collection key");
      break;
    default:
      throw std::invalid_argument("Invalid collection operation");
    }
  }
  if (keys != collectionKeys(*_source))
    throw std::invalid_argument("Change set does not describe current source");
}

void KeyedChildren::finishChanges(const CollectionChangeSet &batch) {
  _sourceRevision = batch.resultingRevision;
  for (const auto &change : batch.changes)
    if (change.operation == CollectionOperation::Update &&
        _availableKeys.contains(change.key))
      refreshItem(change.key);
}

void KeyedChildren::reconcile(std::vector<ItemKey> wanted) {
  checkStructuralMutation();
  std::unordered_set<ItemKey> wantedSet(wanted.begin(), wanted.end());
  if (wantedSet.size() != wanted.size())
    throw std::invalid_argument("Realized keys must be unique");
  std::vector<std::pair<ItemKey, std::unique_ptr<Node>>> additions;
  for (const auto &key : wanted) {
    if (!_realized.contains(key)) {
      auto node = _factory.create(key);
      if (!node)
        throw std::invalid_argument("Item factory returned a null node");
      additions.emplace_back(key, std::move(node));
    }
  }
  _realizedKeys.reserve(_realizedKeys.size() + additions.size());
  _realized.reserve(_realized.size() + additions.size());
  for (std::size_t i = _realizedKeys.size(); i-- > 0;) {
    if (!wantedSet.contains(_realizedKeys[i])) {
      _realized.erase(_realizedKeys[i]);
      takeChildAt(i);
      _realizedKeys.erase(_realizedKeys.begin() +
                          static_cast<std::ptrdiff_t>(i));
    }
  }
  for (auto &[key, node] : additions) {
    // Allocate bookkeeping before transferring ownership to the tree.
    auto [entry, inserted] = _realized.emplace(key, node.get());
    try {
      _realizedKeys.push_back(key);
    } catch (...) {
      _realized.erase(entry);
      throw;
    }
    try {
      appendChild(std::move(node));
    } catch (...) {
      _realizedKeys.pop_back();
      _realized.erase(entry);
      throw;
    }
  }
  for (std::size_t to = 0; to < wanted.size(); ++to) {
    auto it = std::find(_realizedKeys.begin() + static_cast<std::ptrdiff_t>(to),
                        _realizedKeys.end(), wanted[to]);
    const auto from = static_cast<std::size_t>(it - _realizedKeys.begin());
    if (from != to) {
      moveChildAt(from, to);
      std::rotate(_realizedKeys.begin() + static_cast<std::ptrdiff_t>(to), it,
                  it + 1);
    }
  }
}

void KeyedChildren::pinActiveItems(std::vector<ItemKey> &wanted) const {
  std::unordered_set<ItemKey> present(wanted.begin(), wanted.end());
  for (const auto &key : _realizedKeys) {
    if (!present.contains(key) && _availableKeys.contains(key) &&
        _realized.at(key)->hasActiveInputInSubtree())
      wanted.push_back(key);
  }
}

void KeyedChildren::refreshItem(const ItemKey &key) {
  invalidateLayout();
  if (auto *node = realized(key)) {
    node->invalidateLayout();
    if (_factory.update)
      _factory.update(*node, key);
  }
}

} // namespace playground::ui::detail
