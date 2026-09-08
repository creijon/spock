#include "geo/geo3d.hpp"
#include "geo/intersect3d.hpp"

#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

using geo3d::Aabb;
using geo3d::Intersect;
using geo3d::Triangle;

namespace
{
    // A large triangle lying in the z=0 plane, whose edges stay far away from a unit
    // box sitting at the origin, so the box's footprint is entirely inside the triangle.
    Triangle const bigTriangle(
        glm::vec3(-100.0f, -100.0f, 0.0f),
        glm::vec3(100.0f, -100.0f, 0.0f),
        glm::vec3(0.0f, 200.0f, 0.0f));

    // A triangle with one edge that passes straight through the origin.
    Triangle const edgeThroughOriginTriangle(
        glm::vec3(-5.0f, 0.0f, 0.0f),
        glm::vec3(5.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 5.0f, 5.0f));

    Aabb const unitBoxAtOrigin(glm::vec3(0.0f), glm::vec3(1.0f));
    Aabb const unitBoxAlongNormal(glm::vec3(0.0f, 0.0f, 20.0f), glm::vec3(1.0f));
}

// Benchmarks for the two Triangle-Aabb intersection strategies (testSS vs. testNoBB),
// covering the scenarios called out when comparing them: the common case testNoBB is
// optimised for (a triangle edge crosses the box), testNoBB's costliest fallback path
// (the box sits inside the triangle's interior without touching an edge), and a typical
// disjoint case (separated along the triangle's own normal), where both are expected to
// perform similarly.
//
// Hidden by default (tag starts with '.'); run explicitly with e.g.
// spock_tests "[benchmark]"
TEST_CASE("Triangle-Aabb intersection benchmarks", "[.][geo][benchmark]")
{
    SECTION("Common case: a triangle edge passes through the box")
    {
        REQUIRE(Intersect::testSS(edgeThroughOriginTriangle, unitBoxAtOrigin));
        REQUIRE(Intersect::testNoBB(edgeThroughOriginTriangle, unitBoxAtOrigin));
        REQUIRE(Intersect::test(edgeThroughOriginTriangle, unitBoxAtOrigin));

        BENCHMARK("testSS") { return Intersect::testSS(edgeThroughOriginTriangle, unitBoxAtOrigin); };
        BENCHMARK("testNoBB") { return Intersect::testNoBB(edgeThroughOriginTriangle, unitBoxAtOrigin); };
        BENCHMARK("test") { return Intersect::test(edgeThroughOriginTriangle, unitBoxAtOrigin); };
    }

    SECTION("testNoBB's costliest path: box sits inside the triangle's interior, touching no edge")
    {
        REQUIRE(Intersect::testSS(bigTriangle, unitBoxAtOrigin));
        REQUIRE(Intersect::testNoBB(bigTriangle, unitBoxAtOrigin));
        REQUIRE(Intersect::test(bigTriangle, unitBoxAtOrigin));

        BENCHMARK("testSS") { return Intersect::testSS(bigTriangle, unitBoxAtOrigin); };
        BENCHMARK("testNoBB") { return Intersect::testNoBB(bigTriangle, unitBoxAtOrigin); };
        BENCHMARK("test") { return Intersect::test(bigTriangle, unitBoxAtOrigin); };
    }

    SECTION("Common case: disjoint along the triangle's own normal axis")
    {
        REQUIRE_FALSE(Intersect::testSS(bigTriangle, unitBoxAlongNormal));
        REQUIRE_FALSE(Intersect::testNoBB(bigTriangle, unitBoxAlongNormal));
        REQUIRE_FALSE(Intersect::test(bigTriangle, unitBoxAlongNormal));

        BENCHMARK("testSS") { return Intersect::testSS(bigTriangle, unitBoxAlongNormal); };
        BENCHMARK("testNoBB") { return Intersect::testNoBB(bigTriangle, unitBoxAlongNormal); };
        BENCHMARK("test") { return Intersect::test(bigTriangle, unitBoxAlongNormal); };
    }
}
