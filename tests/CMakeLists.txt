include(FetchContent)
find_package(Threads REQUIRED)

FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1 GIT_SHALLOW ON
  SYSTEM)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(nbody_core_tests
  core/test_smoke.cpp
  core/test_vec2.cpp
  core/test_gravity.cpp
  core/test_euler.cpp
  core/test_rk4.cpp
  core/test_world.cpp
  core/test_presets.cpp
)
target_link_libraries(nbody_core_tests PRIVATE nbody_core Catch2::Catch2WithMain)
catch_discover_tests(nbody_core_tests)

# Task 1 also registers the binary-level smoke check: nbody --smoke must exit 0.
add_test(NAME nbody_smoke COMMAND nbody --smoke)

# nbody_app_tests (Task 7): CLI arg parsing; Task 8 adds Camera math. Links
# nbody_app but never instantiates an SFML window (pure code; this exe must
# run display-free).
add_executable(nbody_app_tests app/test_args.cpp app/test_camera.cpp
  app/test_step_planning.cpp app/test_menu_bar.cpp app/test_menu_model.cpp
  app/test_starfield.cpp)
target_link_libraries(nbody_app_tests PRIVATE nbody_app Catch2::Catch2WithMain)
catch_discover_tests(nbody_app_tests)
