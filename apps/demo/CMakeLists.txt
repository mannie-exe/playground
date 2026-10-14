# Legacy host/demo support remains here until the injected-host checkpoint.
add_library(playground_demo_support STATIC
    ${PROJECT_SOURCE_DIR}/src/app/AppHost.cpp
    ${PROJECT_SOURCE_DIR}/src/app/AppContext.cpp
    ${PROJECT_SOURCE_DIR}/src/app/UISession.cpp
    ${PROJECT_SOURCE_DIR}/src/demo2d/Demo2DUI.cpp
    ${PROJECT_SOURCE_DIR}/src/demo3d/Demo3DApp.cpp
    ${PROJECT_SOURCE_DIR}/src/menu/MenuApp.cpp
    ${PROJECT_SOURCE_DIR}/src/menu/MenuUI.cpp
    ${PROJECT_SOURCE_DIR}/src/minesweeper/MinesweeperCell.cpp
    ${PROJECT_SOURCE_DIR}/src/minesweeper/MinesweeperApp.cpp
    ${PROJECT_SOURCE_DIR}/src/minesweeper/DifficultyUI.cpp
    ${PROJECT_SOURCE_DIR}/src/minesweeper/MinesweeperModel.cpp
    ${PROJECT_SOURCE_DIR}/src/minesweeper/MinesweeperGrid.cpp
    ${PROJECT_SOURCE_DIR}/src/minesweeper/MinesweeperUI.cpp)
playground_configure_cpp_target(playground_demo_support)
target_link_libraries(playground_demo_support PUBLIC playground_lib playground_sdl)
playground_add_sdl_target(playground_demo SOURCES main.cpp)
target_link_libraries(playground_demo PRIVATE playground_demo_support)
