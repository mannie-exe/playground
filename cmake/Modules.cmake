# Compiled modules expose the requirements of their public headers. External
# libraries used only by implementation files stay PRIVATE.
function(playground_add_module name)
    add_library(${name} STATIC ${ARGN})
    playground_configure_cpp_target(${name})
    target_compile_features(${name} PUBLIC cxx_std_23)
    target_include_directories(${name} PUBLIC ${PROJECT_SOURCE_DIR}/include)
endfunction()

playground_add_module(playground_math src/math/Geometry3D.cpp src/math/Transform3D.cpp src/math/ColorSpace.cpp src/math/Path2D.cpp)
playground_add_module(playground_runtime src/runtime/CompletionQueue.cpp)
playground_add_module(playground_rendering src/rendering/RendererTypes.cpp src/rendering/RenderSettings.cpp src/rendering/Shader.cpp)
target_link_libraries(playground_rendering PUBLIC playground_math)
target_link_libraries(playground_rendering PRIVATE spirv-reflect-static)
playground_add_module(playground_scene src/scene/SceneRenderer.cpp src/scene/Scene3D.cpp src/scene/Scene2D.cpp src/scene/SceneViewport.cpp)
target_sources(playground_scene PRIVATE src/scene/ModelImport.cpp)
target_include_directories(playground_scene SYSTEM PRIVATE ${cgltf_SOURCE_DIR})
target_link_libraries(playground_scene PUBLIC playground_rendering)

playground_add_module(playground_layout src/layout/LayoutAlgorithms.cpp)
target_link_libraries(playground_layout PUBLIC playground_math)

playground_add_module(playground_ui_core
    src/ui/Node.cpp
    src/ui/UIRoot.cpp
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
    src/ui/controls/Button.cpp)
target_link_libraries(playground_ui_core PUBLIC playground_layout playground_rendering playground_scene)
find_package(Threads REQUIRED)
target_link_libraries(playground_ui_core PUBLIC Threads::Threads)
target_link_libraries(playground_runtime PUBLIC Threads::Threads)
target_link_libraries(playground_ui_core PUBLIC playground_runtime)

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
    src/platform/sdl/SoftwareSceneRenderer.cpp
    src/platform/sdl/RenderBackendFactory.cpp
    src/platform/sdl/GPUResources.cpp
    src/platform/sdl/GPUDevice.cpp
    src/platform/sdl/GPUTimestamps.cpp
    src/platform/sdl/GPUShaderPipeline.cpp
    src/platform/sdl/ModelImport.cpp
    src/platform/sdl/GPUInternal.cpp
    src/platform/sdl/GPURenderBackend.cpp
    src/platform/sdl/GPUPainter.cpp
    src/platform/sdl/GPUSceneRenderer.cpp
    src/platform/sdl/GPUText.cpp
    src/platform/sdl/TextColumns.cpp
    src/platform/sdl/UISession.cpp
    src/support/PerformanceMonitor.cpp
    src/ui/content/Text.cpp
    src/ui/content/Vector.cpp)
target_link_libraries(playground_sdl PUBLIC playground_ui_core playground_ui_resources playground_scene playground_rendering
    PRIVATE utf8proc tomlplusplus::tomlplusplus)
