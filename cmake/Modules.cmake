# Compiled modules expose the requirements of their public headers. External
# libraries used only by implementation files stay PRIVATE.
function(playground_add_module name)
    add_library(${name} STATIC ${ARGN})
    playground_configure_cpp_target(${name})
    target_compile_features(${name} PUBLIC cxx_std_23)
    target_include_directories(${name} PUBLIC ${PROJECT_SOURCE_DIR}/include)
endfunction()

playground_add_module(playground_math src/math/Geometry3D.cpp)
playground_add_module(playground_scene src/scene/SceneRenderer.cpp)
target_link_libraries(playground_scene PUBLIC playground_math)

playground_add_module(playground_layout src/layout/LayoutAlgorithms.cpp)
target_link_libraries(playground_layout PUBLIC playground_math)

playground_add_module(playground_ui_core
    src/ui/Node.cpp
    src/ui/UIRoot.cpp
    src/ui/RuntimeServices.cpp
    src/ui/content/ContentTypes.cpp
    src/ui/content/Image.cpp
    src/ui/containers/AnchorLayout.cpp
    src/ui/containers/Boundaries.cpp
    src/ui/containers/Box.cpp
    src/ui/containers/Flow.cpp
    src/ui/containers/Grid.cpp
    src/ui/containers/Stack.cpp
    src/ui/containers/ZStack.cpp
    src/ui/collections/AdaptiveStack.cpp
    src/ui/collections/Collection.cpp
    src/ui/collections/Repeat.cpp
    src/ui/collections/ScrollView.cpp
    src/ui/collections/VirtualGrid.cpp
    src/ui/collections/VirtualList.cpp
    src/ui/collections/VirtualTrackGrid.cpp
    src/ui/controls/Button.cpp)
target_link_libraries(playground_ui_core PUBLIC playground_layout)
find_package(Threads REQUIRED)
target_link_libraries(playground_ui_core PUBLIC Threads::Threads)

playground_add_module(playground_constraints
    src/ui/containers/ConstraintLayout.cpp)
target_link_libraries(playground_constraints PUBLIC playground_ui_core)
target_include_directories(playground_constraints SYSTEM PRIVATE ${kiwi_SOURCE_DIR})

playground_add_module(playground_ui_resources
    src/support/AssetRegistry.cpp
    src/support/Font.cpp
    src/support/SVGDocument.cpp
    src/support/TextFlow.cpp)
target_link_libraries(playground_ui_resources
    PUBLIC SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf
    PRIVATE utf8proc pugixml::pugixml)

playground_add_module(playground_sdl
    src/platform/Presentation.cpp
    src/platform/FileStore.cpp
    src/platform/Settings.cpp
    src/platform/Window.cpp
    src/platform/sdl/SDLInput.cpp
    src/platform/sdl/SurfacePainter.cpp
    src/platform/sdl/SurfaceRenderBackend.cpp
    src/platform/sdl/GPUResources.cpp
    src/platform/sdl/GPUText.cpp
    src/platform/sdl/TextColumns.cpp
    src/platform/sdl/UISession.cpp
    src/support/PerformanceMonitor.cpp
    src/ui/content/Text.cpp
    src/ui/content/Vector.cpp)
target_link_libraries(playground_sdl PUBLIC playground_ui_core playground_ui_resources playground_scene
    PRIVATE utf8proc tomlplusplus::tomlplusplus)
