// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "geo/geo3d.hpp"
#include "geo/intersect3d.hpp"

#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

using geo3d::Aabb;
namespace intersect = geo3d::intersect;
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

        CHECK_FALSE(intersect::testSS(triangle, box));
        CHECK_FALSE(intersect::testNoBB(triangle, box));
        CHECK_FALSE(intersect::test(triangle, box));
        CHECK_FALSE(intersect::testAM(triangle, box));
    }

    SECTION("Degenerate (single-point) triangle inside the box: intersects")
    {
        Triangle degenerate(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
        Aabb box(glm::vec3(0.0f), glm::vec3(1.0f));

        CHECK(intersect::testSS(degenerate, box));
        CHECK(intersect::testNoBB(degenerate, box));
        CHECK(intersect::test(degenerate, box));
        CHECK(intersect::testAM(degenerate, box));
    }

    SECTION("Degenerate (collinear) triangle crossing the box: intersects")
    {
        Triangle collinear(glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        Aabb box(glm::vec3(0.0f), glm::vec3(1.0f));

        CHECK(intersect::testSS(collinear, box));
        CHECK(intersect::testNoBB(collinear, box));
        CHECK(intersect::test(collinear, box));
        CHECK(intersect::testAM(collinear, box));
    }

    SECTION("Degenerate (collinear) triangle away from the box: does not intersect")
    {
        Triangle collinear(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(6.0f, 0.0f, 0.0f), glm::vec3(5.5f, 0.0f, 0.0f));
        Aabb box(glm::vec3(0.0f), glm::vec3(1.0f));

        CHECK_FALSE(intersect::testSS(collinear, box));
        CHECK_FALSE(intersect::testNoBB(collinear, box));
        CHECK_FALSE(intersect::test(collinear, box));
        CHECK_FALSE(intersect::testAM(collinear, box));
    }
}

// A tiny (~0.001 scale) triangle has edge vectors of ~1e-3 and an unnormalised plane normal of
// ~1e-6, so the SAT projections are small enough to expose any absolute epsilon or degeneracy
// check that wrongly treats a small-but-valid triangle as zero-area. The box spans [-1, 1].
TEST_CASE("Triangle-Aabb intersection handles tiny triangles", "[geo]")
{
    Aabb box(glm::vec3(0.0f), glm::vec3(1.0f));

    SECTION("Tiny triangle at the box centre: intersects")
    {
        Triangle tiny(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.001f, 0.0f, 0.0f), glm::vec3(0.0f, 0.001f, 0.0f));

        CHECK(intersect::testSS(tiny, box));
        CHECK(intersect::testNoBB(tiny, box));
        CHECK(intersect::test(tiny, box));
        CHECK(intersect::testAM(tiny, box));
    }

    SECTION("Tiny triangle straddling a box face: intersects")
    {
        Triangle tiny(glm::vec3(0.9995f, 0.0f, 0.0f), glm::vec3(1.0005f, 0.0f, 0.0f), glm::vec3(1.0f, 0.001f, 0.0005f));

        CHECK(intersect::testSS(tiny, box));
        CHECK(intersect::testNoBB(tiny, box));
        CHECK(intersect::test(tiny, box));
        CHECK(intersect::testAM(tiny, box));
    }

    SECTION("Tiny triangle just outside a box face: does not intersect")
    {
        Triangle tiny(glm::vec3(1.0005f, 0.0f, 0.0f), glm::vec3(1.0015f, 0.0f, 0.0f), glm::vec3(1.001f, 0.001f, 0.0005f));

        CHECK_FALSE(intersect::testSS(tiny, box));
        CHECK_FALSE(intersect::testNoBB(tiny, box));
        CHECK_FALSE(intersect::test(tiny, box));
        CHECK_FALSE(intersect::testAM(tiny, box));
    }

    SECTION("Tiny triangle parallel to a box face, just above it: does not intersect")
    {
        Triangle tiny(glm::vec3(0.0f, 0.0f, 1.0001f), glm::vec3(0.001f, 0.0f, 1.0001f), glm::vec3(0.0f, 0.001f, 1.0001f));

        CHECK_FALSE(intersect::testSS(tiny, box));
        CHECK_FALSE(intersect::testNoBB(tiny, box));
        CHECK_FALSE(intersect::test(tiny, box));
        CHECK_FALSE(intersect::testAM(tiny, box));
    }

    // The triangle's bounds overlap the box and its plane cuts the box, so the only separating
    // axis is an edge cross product: the edge running diagonally past the box's (+x, +y) edge.
    SECTION("Tiny triangle just outside a box edge, separated only by an edge-cross axis: does not intersect")
    {
        Triangle tiny(
            glm::vec3(1.0015f, 0.9995f, 0.0f),
            glm::vec3(0.9995f, 1.0015f, 0.0f),
            glm::vec3(1.0015f, 1.0015f, 0.001f));

        CHECK_FALSE(intersect::testSS(tiny, box));
        CHECK_FALSE(intersect::testNoBB(tiny, box));
        CHECK_FALSE(intersect::test(tiny, box));
        CHECK_FALSE(intersect::testAM(tiny, box));
    }

    // Even smaller (~1e-5 scale, so the unnormalised normal is ~1e-10): a triangle in the plane
    // x + y + z = 3 + d that surrounds the (+x, +y, +z) corner when viewed along (1, 1, 1). Its
    // bounds overlap the box and no edge-cross axis separates it, so only its own normal does.
    SECTION("Tiny triangle just outside a box corner, separated only by its normal: does not intersect")
    {
        float const s = 1e-5f;
        float const d = 3e-6f;
        Triangle tiny(
            glm::vec3(1.0f + d + s, 1.0f - s, 1.0f),
            glm::vec3(1.0f, 1.0f + d + s, 1.0f - s),
            glm::vec3(1.0f - s, 1.0f, 1.0f + d + s));

        CHECK_FALSE(intersect::testSS(tiny, box));
        CHECK_FALSE(intersect::testNoBB(tiny, box));
        CHECK_FALSE(intersect::test(tiny, box));
        CHECK_FALSE(intersect::testAM(tiny, box));
    }

    // A small box poking through the interior of a tiny triangle, touching none of its edges, so
    // testNoBB must fall through to its box-diagonal ray tests. With the triangle and the box
    // diagonals both short, any absolute epsilon on the ray-triangle determinant rejects the hit.
    SECTION("Small box inside the interior of a tiny triangle, touching no edge: intersects")
    {
        Triangle tiny(glm::vec3(0.0f), glm::vec3(0.001f, 0.0f, 0.0f), glm::vec3(0.0f, 0.001f, 0.0f));
        Aabb smallBox(glm::vec3(0.00025f, 0.00025f, 0.0f), glm::vec3(0.00005f));

        CHECK(intersect::testSS(tiny, smallBox));
        CHECK(intersect::testNoBB(tiny, smallBox));
        CHECK(intersect::test(tiny, smallBox));
        CHECK(intersect::testAM(tiny, smallBox));
    }

    // As above, but sized to isolate testNoBB's degenerate-triangle check from the ray-triangle
    // test. With legs of 0.015 the unnormalised normal is 2.25e-4 (|n|^2 ~5e-8, below float
    // epsilon), so an absolute threshold on |n|^2 wrongly treats the triangle as degenerate. The
    // box diagonals are long enough (0.003 along z) that the ray-triangle determinant (~7e-7)
    // stays above float epsilon, so this fails only if the degenerate check regresses.
    SECTION("Small box inside a 0.015-scale triangle, touching no edge: intersects")
    {
        Triangle small(glm::vec3(0.0f), glm::vec3(0.015f, 0.0f, 0.0f), glm::vec3(0.0f, 0.015f, 0.0f));
        Aabb smallBox(glm::vec3(0.004f, 0.004f, 0.0f), glm::vec3(0.0015f));

        CHECK(intersect::testSS(small, smallBox));
        CHECK(intersect::testNoBB(small, smallBox));
        CHECK(intersect::test(small, smallBox));
        CHECK(intersect::testAM(small, smallBox));
    }

    // The mirror of the box-corner case above: the same tiny triangle shape moved just inside the
    // corner, so the corner pokes through its interior without touching any of its edges.
    SECTION("Box corner poking through the interior of a tiny triangle, touching no edge: intersects")
    {
        float const s = 1e-5f;
        float const d = 3e-6f;
        Triangle tiny(
            glm::vec3(1.0f - d + s, 1.0f - s, 1.0f),
            glm::vec3(1.0f, 1.0f - d + s, 1.0f - s),
            glm::vec3(1.0f - s, 1.0f, 1.0f - d + s));

        CHECK(intersect::testSS(tiny, box));
        CHECK(intersect::testNoBB(tiny, box));
        CHECK(intersect::test(tiny, box));
        CHECK(intersect::testAM(tiny, box));
    }

    SECTION("Tiny triangle straddling a box edge: intersects")
    {
        Triangle tiny(
            glm::vec3(1.0005f, 0.9985f, 0.0f),
            glm::vec3(0.9985f, 1.0005f, 0.0f),
            glm::vec3(1.0005f, 1.0005f, 0.001f));

        CHECK(intersect::testSS(tiny, box));
        CHECK(intersect::testNoBB(tiny, box));
        CHECK(intersect::test(tiny, box));
        CHECK(intersect::testAM(tiny, box));
    }
}

// The edge-triangle test treats the edge as a ray from v0 and accepts hits with t in [0, length],
// so an edge that starts or ends exactly on the triangle counts as intersecting. t is the distance
// from v0, not a [0, 1] parameter.
TEST_CASE("Edge-Triangle intersection", "[geo]")
{
    using geo3d::Edge;

    Triangle const triangle(
        glm::vec3(-1.0f, -1.0f, 0.0f),
        glm::vec3(1.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    float t = -1.0f;

    SECTION("an edge crossing the interior hits, with t as the distance from v0")
    {
        CHECK(intersect::test(Edge(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, -1.0f)), triangle, t));
        CHECK(t == 3.0f);
    }

    SECTION("an edge starting on the triangle hits with t == 0")
    {
        CHECK(intersect::test(Edge(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 2.0f)), triangle, t));
        CHECK(t == 0.0f);
        CHECK(intersect::test(Edge(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -2.0f)), triangle, t));
        CHECK(t == 0.0f);
    }

    SECTION("an edge ending on the triangle hits with t == length")
    {
        CHECK(intersect::test(Edge(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f)), triangle, t));
        CHECK(t == 2.0f);
    }

    SECTION("an edge that stops short of the triangle misses")
    {
        CHECK_FALSE(intersect::test(Edge(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 1.0f)), triangle, t));
    }

    SECTION("an edge pointing away from the triangle misses")
    {
        CHECK_FALSE(intersect::test(Edge(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 3.0f)), triangle, t));
    }

    SECTION("an edge passing outside the triangle misses")
    {
        CHECK_FALSE(intersect::test(Edge(glm::vec3(2.0f, 0.0f, 1.0f), glm::vec3(2.0f, 0.0f, -1.0f)), triangle, t));
        CHECK_FALSE(intersect::test(Edge(glm::vec3(0.0f, -1.5f, 1.0f), glm::vec3(0.0f, -1.5f, -1.0f)), triangle, t));
    }

    SECTION("an edge through a triangle vertex or along its boundary hits")
    {
        CHECK(intersect::test(Edge(glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(-1.0f, -1.0f, -1.0f)), triangle, t));
        CHECK(t == 1.0f);
        CHECK(intersect::test(Edge(glm::vec3(0.0f, -1.0f, 1.0f), glm::vec3(0.0f, -1.0f, -1.0f)), triangle, t));
        CHECK(t == 1.0f);
    }

    SECTION("an edge parallel to the triangle misses")
    {
        CHECK_FALSE(intersect::test(Edge(glm::vec3(-2.0f, 0.0f, 1.0f), glm::vec3(2.0f, 0.0f, 1.0f)), triangle, t));
        CHECK_FALSE(intersect::test(Edge(glm::vec3(-2.0f, 0.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f)), triangle, t));
    }

    SECTION("a degenerate edge misses and sets t to 0")
    {
        CHECK_FALSE(intersect::test(Edge(glm::vec3(0.0f), glm::vec3(0.0f)), triangle, t));
        CHECK(t == 0.0f);
    }

    SECTION("a miss sets t to 0")
    {
        CHECK_FALSE(intersect::test(Edge(glm::vec3(2.0f, 0.0f, 1.0f), glm::vec3(2.0f, 0.0f, -1.0f)), triangle, t));
        CHECK(t == 0.0f);
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
        REQUIRE(intersect::testSS(edgeThroughOriginTriangle, unitBoxAtOrigin));
        REQUIRE(intersect::testNoBB(edgeThroughOriginTriangle, unitBoxAtOrigin));
        REQUIRE(intersect::test(edgeThroughOriginTriangle, unitBoxAtOrigin));
        REQUIRE(intersect::testAM(edgeThroughOriginTriangle, unitBoxAtOrigin));

        BENCHMARK("testSS") { return intersect::testSS(edgeThroughOriginTriangle, unitBoxAtOrigin); };
        BENCHMARK("testNoBB") { return intersect::testNoBB(edgeThroughOriginTriangle, unitBoxAtOrigin); };
        BENCHMARK("test") { return intersect::test(edgeThroughOriginTriangle, unitBoxAtOrigin); };
        BENCHMARK("testAM") { return intersect::testAM(edgeThroughOriginTriangle, unitBoxAtOrigin); };
    }

    SECTION("testNoBB's costliest path: box sits inside the triangle's interior, touching no edge")
    {
        REQUIRE(intersect::testSS(bigTriangle, unitBoxAtOrigin));
        REQUIRE(intersect::testNoBB(bigTriangle, unitBoxAtOrigin));
        REQUIRE(intersect::test(bigTriangle, unitBoxAtOrigin));
        REQUIRE(intersect::testAM(bigTriangle, unitBoxAtOrigin));

        BENCHMARK("testSS") { return intersect::testSS(bigTriangle, unitBoxAtOrigin); };
        BENCHMARK("testNoBB") { return intersect::testNoBB(bigTriangle, unitBoxAtOrigin); };
        BENCHMARK("test") { return intersect::test(bigTriangle, unitBoxAtOrigin); };
        BENCHMARK("testAM") { return intersect::testAM(bigTriangle, unitBoxAtOrigin); };
    }

    SECTION("Common case: disjoint along the triangle's own normal axis")
    {
        REQUIRE_FALSE(intersect::testSS(bigTriangle, unitBoxAlongNormal));
        REQUIRE_FALSE(intersect::testNoBB(bigTriangle, unitBoxAlongNormal));
        REQUIRE_FALSE(intersect::test(bigTriangle, unitBoxAlongNormal));
        REQUIRE_FALSE(intersect::testAM(bigTriangle, unitBoxAlongNormal));

        BENCHMARK("testSS") { return intersect::testSS(bigTriangle, unitBoxAlongNormal); };
        BENCHMARK("testNoBB") { return intersect::testNoBB(bigTriangle, unitBoxAlongNormal); };
        BENCHMARK("test") { return intersect::test(bigTriangle, unitBoxAlongNormal); };
        BENCHMARK("testAM") { return intersect::testAM(bigTriangle, unitBoxAlongNormal); };
    }
}
