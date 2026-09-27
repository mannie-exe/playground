#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <ui/Node.hpp>

namespace playground::ui {

class CollectionSource {
public:
  virtual ~CollectionSource() = default;
  virtual std::size_t size() const = 0;
  virtual ItemKey keyAt(std::size_t index) const = 0;

  virtual Revision revision() const { return 0; }
};

enum class CollectionOperation { Insert, Erase, Move, Update };

struct CollectionChange {
  CollectionOperation operation;
  ItemKey key;
  std::size_t index{};
};

struct CollectionChangeSet {
  Revision expectedRevision{};
  Revision resultingRevision{};
  std::vector<CollectionChange> changes;
};

struct ItemFactory {
  std::function<std::unique_ptr<Node>(const ItemKey &)> create;
  std::function<void(Node &, const ItemKey &)> update;
};

namespace detail {
inline std::vector<ItemKey> collectionKeys(const CollectionSource &source) {
  std::vector<ItemKey> keys;
  keys.reserve(source.size());
  std::unordered_set<ItemKey> unique;
  for (std::size_t i = 0; i < source.size(); ++i) {
    auto key = source.keyAt(i);
    if (!unique.insert(key).second)
      throw std::invalid_argument("Collection keys must be unique");
    keys.push_back(std::move(key));
  }
  return keys;
}

// Membership is always consistent with children, even if user callbacks throw.
// Retained keys are moved in-place; their NodeId and local state survive
// reorder.
class KeyedChildren : public Node {
  std::vector<ItemKey> _realizedKeys;
  std::unordered_map<ItemKey, Node *> _realized;

protected:
  std::shared_ptr<const CollectionSource> _source;
  ItemFactory _factory;
  std::vector<ItemKey> _keys;
  std::unordered_set<ItemKey> _availableKeys;
  Revision _sourceRevision{};

  KeyedChildren(std::shared_ptr<const CollectionSource> source,
                ItemFactory factory, layout::BoxProps box = {});

  void replaceKeys(std::vector<ItemKey> keys);

  void validateChanges(const CollectionChangeSet &batch) const;

  void finishChanges(const CollectionChangeSet &batch);

  void reconcile(std::vector<ItemKey> wanted);

  void pinActiveItems(std::vector<ItemKey> &wanted) const;

public:
  Node *realized(const ItemKey &key) const noexcept {
    const auto it = _realized.find(key);
    return it == _realized.end() ? nullptr : it->second;
  }

  std::span<const ItemKey> realizedKeys() const noexcept {
    return _realizedKeys;
  }

  std::span<const ItemKey> itemKeys() const noexcept { return _keys; }

  virtual void refreshItem(const ItemKey &key);
};
} // namespace detail
} // namespace playground::ui
