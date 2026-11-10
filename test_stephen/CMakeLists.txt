include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
  GIT_SHALLOW ON
  SYSTEM
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(nbody_core_tests_stephen
  core_tests.cpp
)
target_link_libraries(nbody_core_tests_stephen
  PRIVATE
    nbody_core
    GTest::gtest_main
)
target_compile_features(nbody_core_tests_stephen PRIVATE cxx_std_17)

include(GoogleTest)
gtest_discover_tests(nbody_core_tests_stephen)
