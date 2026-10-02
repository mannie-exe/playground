#import <AppKit/AppKit.h>

#include <platform/sdl/SystemAppearance.hpp>

namespace playground::sdl {
void platformContrast(ui::SystemAppearance &value) {
  value.reducedMotion =
      [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
  value.highContrast = [[NSWorkspace sharedWorkspace]
      accessibilityDisplayShouldIncreaseContrast];
}
} // namespace playground::sdl
