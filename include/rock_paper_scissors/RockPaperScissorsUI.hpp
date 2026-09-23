#pragma once

#include <math/Geometry2D.hpp>

#include <rock_paper_scissors/Config.hpp>

enum class GameState {
  Menu,
  Pick,
  Resolve,
  Won,
  Lost,
};

struct RockPaperScissorsUILayoutProps {
  int contentWidth;
  int contentPadding;
  int playerDisplayHeight;
  int opponentDisplayHeight;
  int contentHeight;
  playground::math::Vec2i playerDisplaySize;
  playground::math::Vec2i opponentDisplaySize;
  playground::math::Vec2i contentSize;
  playground::math::Vec2i windowSize;

  playground::math::Vec2i menuButtonSize;
  int menuBackgroundHeight;

  playground::math::Vec2i moveCardSize;
  int moveIconSize;
  int turnIconSize;

  float baseFontSize;
  float titleFontSize;
};

struct RockPaperScissorsUIProps {
  playground::math::Rect bounds;

  RockPaperScissorsUILayoutProps layout;
};
