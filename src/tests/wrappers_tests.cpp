// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/wrappers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

TEST_CASE("VertexFormat default-constructs empty", "[wrappers]")
{
    spock::VertexFormat format;
    auto info = format.createInfo();

    CHECK(info.vertexBindingDescriptionCount == 0);
    CHECK(info.vertexAttributeDescriptionCount == 0);
}

TEST_CASE("VertexFormat can be constructed with attributes and stride", "[wrappers]")
{
    spock::VertexFormat::Attributes attrs{
        {vk::Format::eR32G32B32Sfloat, 0},
        {vk::Format::eR32G32Sfloat, 12}
    };
    spock::VertexFormat format(attrs, 20);
    auto info = format.createInfo();

    CHECK(info.vertexBindingDescriptionCount == 1);
    CHECK(info.vertexAttributeDescriptionCount == 2);
    CHECK(info.pVertexBindingDescriptions[0].binding == 0);
    CHECK(info.pVertexBindingDescriptions[0].stride == 20);
    CHECK(info.pVertexBindingDescriptions[0].inputRate == vk::VertexInputRate::eVertex);
}

TEST_CASE("VertexFormat addAttributes adds new binding with correct stride and input rate", "[wrappers]")
{
    spock::VertexFormat format;
    
    spock::VertexFormat::Attributes attrs{
        {vk::Format::eR32G32B32Sfloat, 0}
    };
    
    format.addAttributes(attrs, 12, 1, vk::VertexInputRate::eInstance);
    auto info = format.createInfo();

    CHECK(info.vertexBindingDescriptionCount == 1);
    CHECK(info.pVertexBindingDescriptions[0].binding == 1);
    CHECK(info.pVertexBindingDescriptions[0].stride == 12);
    CHECK(info.pVertexBindingDescriptions[0].inputRate == vk::VertexInputRate::eInstance);
}

TEST_CASE("VertexFormat supports multiple bindings", "[wrappers]")
{
    spock::VertexFormat format;
    
    spock::VertexFormat::Attributes vertexAttrs{
        {vk::Format::eR32G32B32Sfloat, 0},
        {vk::Format::eR32G32Sfloat, 12}
    };
    format.addAttributes(vertexAttrs, 20, 0, vk::VertexInputRate::eVertex);
    
    spock::VertexFormat::Attributes instanceAttrs{
        {vk::Format::eR32G32Sfloat, 0},
        {vk::Format::eR32G32B32A32Sfloat, 8}
    };
    format.addAttributes(instanceAttrs, 24, 1, vk::VertexInputRate::eInstance);
    
    auto info = format.createInfo();

    CHECK(info.vertexBindingDescriptionCount == 2);
    CHECK(info.vertexAttributeDescriptionCount == 4);
    
    // Check bindings
    auto bindings = std::vector<vk::VertexInputBindingDescription>(
        info.pVertexBindingDescriptions,
        info.pVertexBindingDescriptions + info.vertexBindingDescriptionCount);
    
    auto binding0 = std::find_if(bindings.begin(), bindings.end(),
        [](auto b) { return b.binding == 0; });
    auto binding1 = std::find_if(bindings.begin(), bindings.end(),
        [](auto b) { return b.binding == 1; });
    
    REQUIRE(binding0 != bindings.end());
    REQUIRE(binding1 != bindings.end());
    CHECK(binding0->stride == 20);
    CHECK(binding0->inputRate == vk::VertexInputRate::eVertex);
    CHECK(binding1->stride == 24);
    CHECK(binding1->inputRate == vk::VertexInputRate::eInstance);
}

TEST_CASE("VertexFormat assigns sequential locations to attributes", "[wrappers]")
{
    spock::VertexFormat format;
    
    spock::VertexFormat::Attributes firstAttrs{
        {vk::Format::eR32G32B32Sfloat, 0}
    };
    format.addAttributes(firstAttrs, 12, 0);
    
    spock::VertexFormat::Attributes secondAttrs{
        {vk::Format::eR32G32Sfloat, 0},
        {vk::Format::eR32Sfloat, 8}
    };
    format.addAttributes(secondAttrs, 12, 0);
    
    auto info = format.createInfo();

    CHECK(info.vertexAttributeDescriptionCount == 3);
    
    auto attrs = std::vector<vk::VertexInputAttributeDescription>(
        info.pVertexAttributeDescriptions,
        info.pVertexAttributeDescriptions + info.vertexAttributeDescriptionCount);
    
    // Locations should be sequential: 0, 1, 2
    CHECK(attrs[0].location == 0);
    CHECK(attrs[1].location == 1);
    CHECK(attrs[2].location == 2);
}

TEST_CASE("VertexFormat reuses existing binding when adding attributes to same binding", "[wrappers]")
{
    spock::VertexFormat format;
    
    spock::VertexFormat::Attributes attrs1{
        {vk::Format::eR32G32B32Sfloat, 0}
    };
    format.addAttributes(attrs1, 20, 0);
    
    // Add more attributes to the same binding - should not create a new binding
    spock::VertexFormat::Attributes attrs2{
        {vk::Format::eR32G32Sfloat, 12}
    };
    format.addAttributes(attrs2, 20, 0);  // Same binding and stride
    
    auto info = format.createInfo();

    CHECK(info.vertexBindingDescriptionCount == 1);
    CHECK(info.vertexAttributeDescriptionCount == 2);
}

TEST_CASE("VertexFormat attributes use correct binding indices", "[wrappers]")
{
    spock::VertexFormat format;
    
    spock::VertexFormat::Attributes firstBindingAttrs{
        {vk::Format::eR32G32B32Sfloat, 0}
    };
    format.addAttributes(firstBindingAttrs, 12, 0);
    
    spock::VertexFormat::Attributes secondBindingAttrs{
        {vk::Format::eR32G32Sfloat, 0}
    };
    format.addAttributes(secondBindingAttrs, 8, 1);
    
    auto info = format.createInfo();

    auto attrs = std::vector<vk::VertexInputAttributeDescription>(
        info.pVertexAttributeDescriptions,
        info.pVertexAttributeDescriptions + info.vertexAttributeDescriptionCount);
    
    CHECK(attrs[0].binding == 0);
    CHECK(attrs[1].binding == 1);
}

TEST_CASE("VertexFormat preserves attribute offsets", "[wrappers]")
{
    spock::VertexFormat::Attributes attrs{
        {vk::Format::eR32G32B32Sfloat, 0},
        {vk::Format::eR32G32Sfloat, 12},
        {vk::Format::eR32Sfloat, 20}
    };
    spock::VertexFormat format(attrs, 24);
    auto info = format.createInfo();

    auto attrDescs = std::vector<vk::VertexInputAttributeDescription>(
        info.pVertexAttributeDescriptions,
        info.pVertexAttributeDescriptions + info.vertexAttributeDescriptionCount);
    
    CHECK(attrDescs[0].offset == 0);
    CHECK(attrDescs[1].offset == 12);
    CHECK(attrDescs[2].offset == 20);
}

TEST_CASE("VertexFormatWrapper constructs from vertex type with attributes", "[wrappers]")
{
    struct TestVertex
    {
        static spock::VertexFormat::Attributes attributes()
        {
            return {
                {vk::Format::eR32G32B32Sfloat, offsetof(TestVertex, position)},
                {vk::Format::eR32G32B32Sfloat, offsetof(TestVertex, normal)}
            };
        }

        glm::vec3 position;
        glm::vec3 normal;
    };

    spock::VertexFormatWrapper<TestVertex> wrapper;
    auto info = wrapper.createInfo();

    CHECK(info.vertexAttributeDescriptionCount == 2);
    CHECK(info.vertexBindingDescriptionCount == 1);
    CHECK(info.pVertexBindingDescriptions[0].stride == sizeof(TestVertex));
    CHECK(info.pVertexBindingDescriptions[0].inputRate == vk::VertexInputRate::eVertex);
}
