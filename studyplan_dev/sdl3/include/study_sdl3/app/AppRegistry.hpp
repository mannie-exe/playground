#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <study_sdl3/app/AppTypes.hpp>
#include <study_sdl3/app/IApp.hpp>

using AppFactory = std::function<std::unique_ptr<IApp>()>;

class AppRegistry {
  struct Entry {
    AppInfo info;
    AppFactory factory;
  };

  std::vector<Entry> _entries;

public:
  void add(AppInfo info, AppFactory factory) {
    _entries.emplace_back(Entry{.info = std::move(info),
                                .factory = std::move(factory)});
  }

  std::unique_ptr<IApp> create(AppId appId) const {
    for (const Entry &entry : _entries) {
      if (entry.info.id == appId)
        return entry.factory();
    }

    throw std::string{"AppRegistry failed to create requested app"};
  }

  const AppInfo &info(AppId appId) const {
    for (const Entry &entry : _entries) {
      if (entry.info.id == appId)
        return entry.info;
    }

    throw std::string{"AppRegistry failed to find requested app info"};
  }

  std::vector<AppInfo> list() const {
    std::vector<AppInfo> apps;
    apps.reserve(_entries.size());

    for (const Entry &entry : _entries)
      apps.emplace_back(entry.info);

    return apps;
  }
};
