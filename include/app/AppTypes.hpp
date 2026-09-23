#pragma once

#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <app/AppConfig.hpp>
#include <math/Color.hpp>
#include <math/GeometryFormatters.hpp>
#include <platform/Settings.hpp>

enum class AppId {
  Menu,
  Demo,
  Minesweeper,
  RockPaperScissors,
  Snake,
};

enum class AppCommandType {
  None,
  SwitchTo,
  ReturnToMenu,
  Quit,
  SetWindowProps,
  SetViewPolicy,
  FitContent,
  SetPresentation,
  ReloadSettings,
  SetUserSettings,
};

struct AppWindowProps {
  std::string title{playground::config::defaultWindowTitle};
  bool alwaysOnTop{};
  bool focusable{true};
  bool hidden{};
  bool mouseGrabbed{};
  playground::math::ColorRGBA8 clearColor{
      playground::config::defaultClearColor};
};

struct AppInfo {
  AppId id{AppId::Menu};
  std::string_view name{"Menu"};
  AppWindowProps window;
  playground::platform::AppViewPolicy view;
  playground::platform::PresentationProps presentation;
};

struct PendingAppCommand {
  AppCommandType type{AppCommandType::None};
  AppId target{AppId::Menu};
  std::optional<AppWindowProps> window;
  std::optional<playground::platform::AppViewPolicy> view;
  std::optional<playground::platform::PresentationProps> presentation;
  std::optional<playground::platform::SettingsDocument> settings;
  bool persist{};
};

constexpr std::string_view toString(AppId appId) {
  switch (appId) {
  case AppId::Menu:
    return "Menu";
  case AppId::Demo:
    return "Demo";
  case AppId::Minesweeper:
    return "Minesweeper";
  case AppId::RockPaperScissors:
    return "Rock Paper Scissors";
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
  case AppCommandType::SetWindowProps:
    return "SetWindowProps";
  case AppCommandType::SetViewPolicy:
    return "SetViewPolicy";
  case AppCommandType::FitContent:
    return "FitContent";
  case AppCommandType::SetPresentation:
    return "SetPresentation";
  case AppCommandType::ReloadSettings:
    return "ReloadSettings";
  case AppCommandType::SetUserSettings:
    return "SetUserSettings";
  default:
    return "Unknown";
  }
}

template <> struct std::formatter<AppId> : std::formatter<std::string_view> {
  auto format(AppId appId, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(appId), ctx);
  }
};

constexpr std::string_view appKey(AppId id) {
  switch (id) {
  case AppId::Menu:
    return "menu";
  case AppId::Demo:
    return "demo";
  case AppId::Minesweeper:
    return "minesweeper";
  case AppId::RockPaperScissors:
    return "rock-paper-scissors";
  case AppId::Snake:
    return "snake";
  }
  throw std::invalid_argument("Unknown app identity");
}

template <>
struct std::formatter<AppCommandType> : std::formatter<std::string_view> {
  auto format(AppCommandType type, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(type), ctx);
  }
};

template <>
struct std::formatter<AppWindowProps> : std::formatter<std::string_view> {
  auto format(const AppWindowProps &props, format_context &ctx) const {
    return std::format_to(ctx.out(),
                          "AppWindowProps{{.title = \"{}\", .clearColor = {}}}",
                          props.title, props.clearColor);
  }
};

template <> struct std::formatter<AppInfo> : std::formatter<std::string_view> {
  auto format(const AppInfo &info, format_context &ctx) const {
    return std::format_to(ctx.out(),
                          "AppInfo{{.id = {}, .name = \"{}\", .window = {}}}",
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
