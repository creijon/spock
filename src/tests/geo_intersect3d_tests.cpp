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

// Regression cases for testAM: a triangle vertex sitting almost exactly on a box corner (every
// one of the 13 SAT axes is within a percent or two of separating, which is thin enough margin to
// be sensitive to floating-point evaluation order), and degenerate (zero-area) triangles, which
// have no plane normal to test the box's overlap against.
TEST_CASE("Triangle-Aabb intersection handles near-corner and degenerate triangles", "[geo]")
{
    SECTION("Triangle vertex almost touching a box corner: does not intersect")
    {
        Triangle triangle(
            glm::vec3(0.07494f, 0.070754f, 0.028271f),
            glm::vec3(0.075958f, 0.071995f, 0.028739f),
            glm::vec3(0.075162f, 0.071562f, 0.029279f));
        Aabb box(
            glm::vec3(0.05576525f, 0.0715705f, 0.01354725f),
            glm::vec3(0.07522762f, 0.09086225f, 0.0286315f),
            true);

        CHECK_FALSE(Intersect::testSS(triangle, box));
        CHECK_FALSE(Intersect::testNoBB(triangle, box));
        CHECK_FALSE(Intersect::test(triangle, box));
        CHECK_FALSE(Intersect::testAM(triangle, box));
    }

    SECTION("Degenerate (single-point) triangle inside the box: intersects")
    {
        Triangle degenerate(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
        Aabb box(glm::vec3(0.0f), glm::vec3(1.0f));

        CHECK(Intersect::testSS(degenerate, box));
        CHECK(Intersect::testNoBB(degenerate, box));
        CHECK(Intersect::test(degenerate, box));
        CHECK(Intersect::testAM(degenerate, box));
    }

    SECTION("Degenerate (collinear) triangle crossing the box: intersects")
    {
        Triangle collinear(glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        Aabb box(glm::vec3(0.0f), glm::vec3(1.0f));

        CHECK(Intersect::testSS(collinear, box));
        CHECK(Intersect::testNoBB(collinear, box));
        CHECK(Intersect::test(collinear, box));
        CHECK(Intersect::testAM(collinear, box));
    }

    SECTION("Degenerate (collinear) triangle away from the box: does not intersect")
    {
        Triangle collinear(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(6.0f, 0.0f, 0.0f), glm::vec3(5.5f, 0.0f, 0.0f));
        Aabb box(glm::vec3(0.0f), glm::vec3(1.0f));

        CHECK_FALSE(Intersect::testSS(collinear, box));
        CHECK_FALSE(Intersect::testNoBB(collinear, box));
        CHECK_FALSE(Intersect::test(collinear, box));
        CHECK_FALSE(Intersect::testAM(collinear, box));
    }
}

// Benchmarks for the Triangle-Aabb intersection strategies (testSS, testNoBB, test, and the
// standard-SAT reference testAM), covering the scenarios called out when comparing them: the
// common case testNoBB is optimised for (a triangle edge crosses the box), testNoBB's costliest
// fallback path (the box sits inside the triangle's interior without touching an edge), and a
// typical disjoint case (separated along the triangle's own normal).
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
        REQUIRE(Intersect::testAM(edgeThroughOriginTriangle, unitBoxAtOrigin));

        BENCHMARK("testSS") { return Intersect::testSS(edgeThroughOriginTriangle, unitBoxAtOrigin); };
        BENCHMARK("testNoBB") { return Intersect::testNoBB(edgeThroughOriginTriangle, unitBoxAtOrigin); };
        BENCHMARK("test") { return Intersect::test(edgeThroughOriginTriangle, unitBoxAtOrigin); };
        BENCHMARK("testAM") { return Intersect::testAM(edgeThroughOriginTriangle, unitBoxAtOrigin); };
    }

    SECTION("testNoBB's costliest path: box sits inside the triangle's interior, touching no edge")
    {
        REQUIRE(Intersect::testSS(bigTriangle, unitBoxAtOrigin));
        REQUIRE(Intersect::testNoBB(bigTriangle, unitBoxAtOrigin));
        REQUIRE(Intersect::test(bigTriangle, unitBoxAtOrigin));
        REQUIRE(Intersect::testAM(bigTriangle, unitBoxAtOrigin));

        BENCHMARK("testSS") { return Intersect::testSS(bigTriangle, unitBoxAtOrigin); };
        BENCHMARK("testNoBB") { return Intersect::testNoBB(bigTriangle, unitBoxAtOrigin); };
        BENCHMARK("test") { return Intersect::test(bigTriangle, unitBoxAtOrigin); };
        BENCHMARK("testAM") { return Intersect::testAM(bigTriangle, unitBoxAtOrigin); };
    }

    SECTION("Common case: disjoint along the triangle's own normal axis")
    {
        REQUIRE_FALSE(Intersect::testSS(bigTriangle, unitBoxAlongNormal));
        REQUIRE_FALSE(Intersect::testNoBB(bigTriangle, unitBoxAlongNormal));
        REQUIRE_FALSE(Intersect::test(bigTriangle, unitBoxAlongNormal));
        REQUIRE_FALSE(Intersect::testAM(bigTriangle, unitBoxAlongNormal));

        BENCHMARK("testSS") { return Intersect::testSS(bigTriangle, unitBoxAlongNormal); };
        BENCHMARK("testNoBB") { return Intersect::testNoBB(bigTriangle, unitBoxAlongNormal); };
        BENCHMARK("test") { return Intersect::test(bigTriangle, unitBoxAlongNormal); };
        BENCHMARK("testAM") { return Intersect::testAM(bigTriangle, unitBoxAlongNormal); };
    }
}
