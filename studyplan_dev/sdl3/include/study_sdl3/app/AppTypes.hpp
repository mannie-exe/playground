#pragma once

#include <format>
#include <optional>
#include <string>
#include <string_view>

#include <study_sdl3/platform/WindowTypes.hpp>
#include <study_sdl3/support/SDLPrimitives.hpp>

enum class AppId {
  Menu,
  Demo,
  Minesweeper,
  Snake,
};

enum class AppCommandType {
  None,
  SwitchTo,
  ReturnToMenu,
  Quit,
  ReconfigureWindow,
};

struct AppInfo {
  AppId id{AppId::Menu};
  std::string_view name{"Menu"};
  WindowConfig window{};
};

struct PendingAppCommand {
  AppCommandType type{AppCommandType::None};
  AppId target{AppId::Menu};
  std::optional<WindowConfig> window;
};

constexpr std::string_view toString(AppId appId) {
  switch (appId) {
  case AppId::Menu:
    return "Menu";
  case AppId::Demo:
    return "Demo";
  case AppId::Minesweeper:
    return "Minesweeper";
  case AppId::Snake:
    return "Snake";
  default:
    return "Unknown";
  }
}

constexpr std::string_view toString(AppCommandType type) {
  switch (type) {
  case AppCommandType::None:
    return "None";
  case AppCommandType::SwitchTo:
    return "SwitchTo";
  case AppCommandType::ReturnToMenu:
    return "ReturnToMenu";
  case AppCommandType::Quit:
    return "Quit";
  case AppCommandType::ReconfigureWindow:
    return "ReconfigureWindow";
  default:
    return "Unknown";
  }
}

template <>
struct std::formatter<AppId> : std::formatter<std::string_view> {
  auto format(AppId appId, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(appId), ctx);
  }
};

template <>
struct std::formatter<AppCommandType> : std::formatter<std::string_view> {
  auto format(AppCommandType type, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(type), ctx);
  }
};

template <>
struct std::formatter<AppInfo> : std::formatter<std::string_view> {
  auto format(const AppInfo &info, format_context &ctx) const {
    return std::format_to(
        ctx.out(), "AppInfo{{.id = {}, .name = \"{}\", .window = {}}}",
        info.id, info.name, info.window);
  }
};

template <>
struct std::formatter<PendingAppCommand> : std::formatter<std::string_view> {
  auto format(const PendingAppCommand &command, format_context &ctx) const {
    if (command.window) {
      return std::format_to(
          ctx.out(),
          "PendingAppCommand{{.type = {}, .target = {}, .window = {}}}",
          command.type, command.target, *command.window);
    }

    return std::format_to(
        ctx.out(),
        "PendingAppCommand{{.type = {}, .target = {}, .window = nullopt}}",
        command.type, command.target);
  }
};
