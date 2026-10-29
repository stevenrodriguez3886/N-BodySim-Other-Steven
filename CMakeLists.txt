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

# nbody_core: real STATIC library (first source landed in Task 3).
# nbody_core MUST NOT link anything (incl. SFML). Enforced: no link line here.
add_library(nbody_core STATIC src/core/GravityField.cpp)
target_include_directories(nbody_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nbody_core PUBLIC cxx_std_17)

# Task 1 scaffold: nbody_app is an empty placeholder (INTERFACE); its .cpp
# sources join as later tasks land, at which point SFML moves to PRIVATE per
# plan section A. INTERFACE targets can only carry INTERFACE link lines.
add_library(nbody_app INTERFACE)
target_include_directories(nbody_app INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nbody_app INTERFACE nbody_core
  SFML::Graphics SFML::Window SFML::System)

add_executable(nbody src/main.cpp)
target_link_libraries(nbody PRIVATE nbody_app nbody_core
  SFML::Graphics SFML::Window SFML::System ${CMAKE_DL_LIBS})

enable_testing()
add_subdirectory(tests)
