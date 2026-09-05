#pragma once

#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/surface/Image.hpp>
#include <study_sdl3/ui/DisplayObject.hpp>

class DisplayImage : public DisplayObject, public IInteractable {
  Image _image;
};
