# Compiled modules expose the requirements of their public headers. External
# libraries used only by implementation files stay PRIVATE.
function(playground_add_module name)
    add_library(${name} STATIC ${ARGN})
    playground_configure_cpp_target(${name})
    target_compile_features(${name} PUBLIC cxx_std_23)
    target_include_directories(${name} PUBLIC ${PROJECT_SOURCE_DIR}/include)
endfunction()

playground_add_module(playground_math
    src/math/Geometry3D.cpp
    src/math/Transform3D.cpp
    src/math/ColorSpace.cpp
    src/math/Path2D.cpp)
playground_add_module(playground_runtime
    src/runtime/CompletionQueue.cpp
    src/runtime/Executor.cpp
    src/runtime/SimulationClock.cpp
    src/input/InputMap.cpp)
playground_add_module(playground_rendering
    src/rendering/RendererTypes.cpp
    src/rendering/RenderSettings.cpp
    src/rendering/GraphicsSettings.cpp
    src/rendering/Shader.cpp
    src/rendering/Texture.cpp)
target_sources(playground_rendering PRIVATE src/rendering/TextureStorage.cpp)
target_link_libraries(playground_rendering PUBLIC playground_math)
target_link_libraries(playground_rendering PRIVATE spirv-reflect-static)
playground_add_module(playground_scene
    src/scene/SceneRenderer.cpp
    src/scene/Scene3D.cpp
    src/scene/Scene2D.cpp
    src/scene/SceneViewport.cpp
    src/scene/Controllers.cpp
    src/scene/ModelImport.cpp
    src/scene/MeshPreparation.cpp
    src/scene/Animation.cpp
    src/scene/Environment.cpp)
target_include_directories(playground_scene SYSTEM PRIVATE ${cgltf_SOURCE_DIR})
target_link_libraries(playground_scene PUBLIC playground_rendering)
target_link_libraries(playground_scene PRIVATE playground_mikktspace meshoptimizer)

playground_add_module(playground_assets src/assets/AssetCatalog.cpp)
target_link_libraries(playground_assets PUBLIC playground_scene)

playground_add_module(playground_layout src/layout/LayoutAlgorithms.cpp)
target_link_libraries(playground_layout PUBLIC playground_math)

playground_add_module(playground_text
    src/support/Unicode.cpp
    src/support/TextFlow.cpp)
target_link_libraries(playground_text PRIVATE ICU::uc ICU::i18n ICU::data)

playground_add_module(playground_ui_core
    src/ui/Theme.cpp
    src/ui/Node.cpp
    src/ui/UIRoot.cpp
    src/ui/Overlays.cpp
    src/ui/containers/Popup.cpp
    src/ui/Semantics.cpp
    src/ui/TextEdit.cpp
    src/ui/RuntimeServices.cpp
    src/ui/content/ContentTypes.cpp
    src/ui/content/Image.cpp
    src/ui/content/Path.cpp
    src/ui/content/SceneView.cpp
    src/ui/content/Scene2DView.cpp
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
    src/ui/controls/Button.cpp
    src/ui/controls/Choice.cpp
    src/ui/controls/ChoiceStepper.cpp
    src/ui/controls/Composite.cpp
    src/ui/controls/Editing.cpp
    src/ui/controls/Form.cpp
    src/ui/controls/Groups.cpp
    src/ui/controls/Meter.cpp
    src/ui/controls/Navigation.cpp
    src/ui/controls/NumberStepper.cpp
    src/ui/controls/Slider.cpp
    src/ui/controls/Surfaces.cpp
    src/ui/controls/ToastHost.cpp)
target_link_libraries(playground_ui_core PUBLIC playground_layout playground_rendering playground_scene)
find_package(Threads REQUIRED)
target_link_libraries(playground_rendering PUBLIC Threads::Threads)
target_link_libraries(playground_ui_core PUBLIC Threads::Threads)
target_link_libraries(playground_runtime PUBLIC Threads::Threads playground_math)
target_link_libraries(playground_ui_core PUBLIC playground_runtime playground_text)
target_link_libraries(playground_ui_core PRIVATE ICU::uc ICU::i18n ICU::data)

playground_add_module(playground_constraints
    src/ui/containers/ConstraintLayout.cpp)
target_link_libraries(playground_constraints PUBLIC playground_ui_core)
target_include_directories(playground_constraints SYSTEM PRIVATE ${kiwi_SOURCE_DIR})

playground_add_module(playground_ui_resources
    src/support/AssetRegistry.cpp
    src/support/Font.cpp
    src/support/SVGDocument.cpp)
target_link_libraries(playground_ui_resources
    PUBLIC SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf playground_text
    PRIVATE pugixml::pugixml)
target_link_libraries(playground_ui_resources PUBLIC playground_rendering)

playground_add_module(playground_sdl
    src/app/PresentationSession.cpp
    # Host platform and session integration.
    src/platform/Presentation.cpp
    src/platform/FileStore.cpp
    src/platform/Settings.cpp
    src/platform/Window.cpp
    src/platform/sdl/SDLInput.cpp
    src/platform/sdl/SDLActionInput.cpp
    src/platform/sdl/SDLGamepads.cpp
    src/platform/sdl/UISession.cpp
    src/platform/sdl/WindowServices.cpp
    src/platform/sdl/EventWake.cpp
    src/platform/sdl/SystemAppearance.cpp
    src/support/PerformanceMonitor.cpp)

target_sources(playground_sdl PRIVATE
    # Software presentation and rasterization.
    src/platform/sdl/SurfacePainter.cpp
    src/platform/sdl/SurfaceRenderBackend.cpp
    src/platform/sdl/SoftwareSceneRenderer.cpp
    src/platform/sdl/RenderBackendFactory.cpp)

target_sources(playground_sdl PRIVATE
    # GPU lifetime, recording and presentation.
    src/platform/sdl/GPUResources.cpp
    src/platform/sdl/GPUDevice.cpp
    src/platform/sdl/GPUTimestamps.cpp
    src/platform/sdl/GPUShaderPipeline.cpp
    src/platform/sdl/GPUInternal.cpp
    src/platform/sdl/GPURenderBackend.cpp
    src/platform/sdl/GPUPainter.cpp
    src/platform/sdl/GPUSceneRenderer.cpp)

target_sources(playground_sdl PRIVATE
    # Resource realization bridges shared by content and renderers.
    src/platform/sdl/ModelImport.cpp
    src/platform/sdl/TextureDecode.cpp
    src/platform/sdl/KTXTexture.cpp
    src/platform/sdl/AssetResources.cpp
    src/platform/sdl/ModelPreparation.cpp
    src/platform/sdl/GPUText.cpp
    src/platform/sdl/TextColumns.cpp
    src/ui/views/SettingsView.cpp
    src/ui/content/Text.cpp
    src/ui/controls/TextField.cpp
    src/ui/content/Vector.cpp)
target_link_libraries(playground_sdl PUBLIC playground_ui_core playground_ui_resources playground_scene playground_rendering playground_assets
    PRIVATE utf8proc tomlplusplus::tomlplusplus ktx_read accesskit-static)
target_include_directories(playground_sdl SYSTEM PRIVATE ${stb_SOURCE_DIR})
if(APPLE)
    set_target_properties(playground_sdl PROPERTIES OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED ON)
    target_sources(playground_sdl PRIVATE src/platform/sdl/SystemAppearanceMac.mm)
    target_link_libraries(playground_sdl PRIVATE "-framework AppKit")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1)
    target_sources(playground_sdl PRIVATE src/platform/sdl/SystemAppearanceLinux.cpp)
    target_link_libraries(playground_sdl PRIVATE PkgConfig::DBUS)
endif()
