/// @file
/// Smoke test for the core test target: a single always-true assertion that
/// proves the test binary builds, links, and runs under the test harness.

#include <catch2/catch_test_macros.hpp>

/// @brief Trivially passes to verify that building and running tests works.
TEST_CASE("build works") {
    REQUIRE(1 == 1);
}
