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
)
target_link_libraries(nbody_core_tests PRIVATE nbody_core Catch2::Catch2WithMain)
catch_discover_tests(nbody_core_tests)

# Task 1 also registers the binary-level smoke check: nbody --smoke must exit 0.
add_test(NAME nbody_smoke COMMAND nbody --smoke)

# nbody_app_tests (linking nbody_app) is added in a later task, per plan.
