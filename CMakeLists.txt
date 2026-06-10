cmake_minimum_required(VERSION 3.16)
project(MyGame VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SFML_STATIC_LIBRARIES TRUE)
set(SFML_DIR "D:/libs/SFML-3.1.0/lib/cmake/SFML")

add_executable(MyGame
    src/main.cpp
    src/Simulation.cpp
    src/Brain.cpp
    src/BrainView.cpp
    src/Charts.cpp
    src/Leaderboard.cpp
    src/SaveState.cpp
    src/SliderPanel.cpp
)

find_package(SFML 3 COMPONENTS System Window Graphics Audio Network REQUIRED)

target_link_libraries(MyGame
    PRIVATE
        SFML::System
        SFML::Window
        SFML::Graphics
        SFML::Audio
        SFML::Network
)

if (WIN32)
    # native save/open file dialogs (SaveState.cpp)
    target_link_libraries(MyGame PRIVATE comdlg32)
endif()
