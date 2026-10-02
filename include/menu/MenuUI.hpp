#pragma once

#include <functional>
#include <string>
#include <vector>

#include <app/AppTypes.hpp>
#include <support/AssetRegistry.hpp>
#include <ui/containers/Box.hpp>
#include <ui/content/Text.hpp>

namespace playground::menu {
class MenuUI final : public ui::Box {
  ui::Text *_status{};
  std::vector<ui::Connection> _connections;

public:
  MenuUI(AssetRegistry &, FontHandle font, FontHandle titleFont,
         std::function<void(AppId, AppLaunchProps)> launch);
  void setStatus(std::string value);
};
} // namespace playground::menu
