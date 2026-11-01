cmake_minimum_required(VERSION 3.25)
project(nbody LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wpedantic)

include(FetchContent)
find_package(SFML 3.0 COMPONENTS Graphics Window System QUIET)
if(NOT SFML_FOUND)
  set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_DOC      OFF CACHE BOOL "" FORCE)
  # Audio is not in our COMPONENTS list; skip it so its Ogg/Vorbis/FLAC/Opus
  # dev-package requirements don't gate the build.
  set(SFML_BUILD_AUDIO    OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2 GIT_SHALLOW ON
    SYSTEM)
  FetchContent_MakeAvailable(SFML)
endif()

# nbody_core MUST NOT link anything (incl. SFML). Enforced: no link line here.
add_library(nbody_core STATIC src/core/GravityField.cpp src/core/EulerIntegrator.cpp
  src/core/Rk4Integrator.cpp src/core/PhysicsWorld.cpp src/core/Presets.cpp)
target_include_directories(nbody_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nbody_core PUBLIC cxx_std_17)

# nbody_app: the SFML-facing application layer as a STATIC library. SFML is
# linked PRIVATE: app consumers get nbody_core transitively but no
# SFML link/include leak (the nbody exe names its own SFML links below).
add_library(nbody_app STATIC src/app/AppArgs.cpp src/app/HeadlessRunner.cpp
  src/app/Camera.cpp src/app/StepPlanning.cpp src/app/MenuBar.cpp
  src/app/MenuModel.cpp src/app/SimulationApp.cpp src/app/Starfield.cpp
  src/app/TextField.cpp)
target_include_directories(nbody_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nbody_app PUBLIC cxx_std_17)
target_link_libraries(nbody_app PUBLIC nbody_core
  PRIVATE SFML::Graphics SFML::Window SFML::System)

add_executable(nbody src/main.cpp)
target_link_libraries(nbody PRIVATE nbody_app nbody_core
  SFML::Graphics SFML::Window SFML::System ${CMAKE_DL_LIBS})

enable_testing()
add_subdirectory(tests)
