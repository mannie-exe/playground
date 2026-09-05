#pragma once

#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/surface/Text.hpp>
#include <study_sdl3/ui/DisplayObject.hpp>

class DisplayText : public DisplayObject, public IInteractable {
  Text _text;
};
