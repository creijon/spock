// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/creators.hpp"

#include <catch2/catch_test_macros.hpp>

namespace
{
    vk::ColorComponentFlags const allComponents =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
}

TEST_CASE("blendStraightAlpha weights the source color by its alpha", "[creators]")
{
    vk::PipelineColorBlendAttachmentState const state = spock::blendStraightAlpha();

    CHECK(state.blendEnable == VK_TRUE);
    CHECK(state.srcColorBlendFactor == vk::BlendFactor::eSrcAlpha);
    CHECK(state.dstColorBlendFactor == vk::BlendFactor::eOneMinusSrcAlpha);
    CHECK(state.colorBlendOp == vk::BlendOp::eAdd);
    // The source alpha replaces the destination alpha.
    CHECK(state.srcAlphaBlendFactor == vk::BlendFactor::eOne);
    CHECK(state.dstAlphaBlendFactor == vk::BlendFactor::eZero);
    CHECK(state.alphaBlendOp == vk::BlendOp::eAdd);
    CHECK(state.colorWriteMask == allComponents);
}

TEST_CASE("blendPremultipliedAlpha adds the source color as it is and keeps the result premultiplied", "[creators]")
{
    vk::PipelineColorBlendAttachmentState const state = spock::blendPremultipliedAlpha();

    CHECK(state.blendEnable == VK_TRUE);
    CHECK(state.srcColorBlendFactor == vk::BlendFactor::eOne);
    CHECK(state.dstColorBlendFactor == vk::BlendFactor::eOneMinusSrcAlpha);
    CHECK(state.colorBlendOp == vk::BlendOp::eAdd);
    // Alpha composites the same way as color, so the result can be blended again or presented as premultiplied.
    CHECK(state.srcAlphaBlendFactor == vk::BlendFactor::eOne);
    CHECK(state.dstAlphaBlendFactor == vk::BlendFactor::eOneMinusSrcAlpha);
    CHECK(state.alphaBlendOp == vk::BlendOp::eAdd);
    CHECK(state.colorWriteMask == allComponents);
}

TEST_CASE("blendDisabled writes every component without blending", "[creators]")
{
    vk::PipelineColorBlendAttachmentState const state = spock::blendDisabled();

    CHECK(state.blendEnable == VK_FALSE);
    CHECK(state.colorWriteMask == allComponents);
}
