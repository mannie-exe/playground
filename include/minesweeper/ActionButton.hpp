#pragma once

#include <functional>
#include <string>
#include <utility>

#include <minesweeper/ViewResources.hpp>
#include <ui/controls/Button.hpp>

struct ActionButtonProps {
  playground::ui::ButtonProps button;
  playground::math::ColorRGBA8 labelColor{255, 255, 255, 255};
};

// Application composition; activation behavior belongs to the shared Button.
class ActionButton : public playground::ui::Button {
  playground::ui::Connection _activation;

public:
  ActionButton(const playground::minesweeper::ViewResources &resources,
               playground::math::Size2 size, const ActionButtonProps &props,
               std::string label, std::function<void()> action)
      : Button{playground::minesweeper::makeLabel(resources, label,
                                                  props.labelColor, size),
               props.button,
               {.width = playground::layout::SizeRule::fixed(size.width),
                .height = playground::layout::SizeRule::fixed(size.height)}} {
    static_cast<playground::ui::Text *>(children().front().get())
        ->applyPatch(
            {.colorTreatment =
                 playground::Patch<playground::ui::ColorTreatment>::set(
                     playground::ui::ColorTreatment::Adaptive)});
    setContentAlignment(playground::layout::Alignment::stretch());
    auto semantics = semanticProps();
    semantics.name = std::move(label);
    setSemanticProps(std::move(semantics));
    _activation = onActivate(std::move(action));
  }
};
