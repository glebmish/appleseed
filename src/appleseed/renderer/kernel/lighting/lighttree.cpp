
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2017 Petra Gospodnetic, The appleseedhq Organization
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Interface header.
#include "lighttree.h"

// appleseed.renderer headers.
#include "renderer/global/globallogger.h"
#include "renderer/global/globaltypes.h"
#include "renderer/modeling/edf/edf.h"
#include "renderer/modeling/input/source.h"
#include "renderer/modeling/light/light.h"
#include "renderer/modeling/material/material.h"

// appleseed.foundation headers.
#include "foundation/image/colorspace.h"
#include "foundation/math/distance.h"
#include "foundation/math/permutation.h"
#include "foundation/math/scalar.h"
#include "foundation/platform/timers.h"
#include "foundation/utility/vpythonfile.h"

// Standard headers.
#include <cassert>

using namespace foundation;
using namespace std;

namespace renderer
{

//
// LightTree class implementation.
//

LightTree::LightTree(
    const vector<NonPhysicalLightInfo>&      non_physical_lights,
    const vector<EmittingTriangle>&          emitting_triangles)
  : m_non_physical_lights(non_physical_lights)
  , m_emitting_triangles(emitting_triangles)
  , m_tree_depth(0)
  , m_is_built(false)
{
}

vector<size_t> LightTree::build()
{
    AABBVector light_bboxes;

    // Collect non-physical light sources.
    for (size_t i = 0, e = m_non_physical_lights.size(); i < e; ++i)
    {
        const Light* light = m_non_physical_lights[i].m_light;

        // Retrieve the exact position of the light.
        const Vector3d position = light->get_transform()
                                    .get_local_to_parent()
                                    .extract_translation();

        // Non physical light has no real size - hence some arbitrary small 
        // value is assigned.
        constexpr double BboxSize = 0.001f;
        const AABB3d bbox = AABB3d(Vector3d(position[0] - BboxSize,
                                            position[1] - BboxSize,
                                            position[2] - BboxSize),
                                   Vector3d(position[0] + BboxSize,
                                            position[1] + BboxSize,
                                            position[2] + BboxSize));
        light_bboxes.push_back(bbox);

        m_items.push_back(Item(bbox, i, NonPhysicalLightType));
    }

    // Collect emitting triangles.
    for (size_t i = 0, e = m_emitting_triangles.size(); i < e; ++i)
    {
        const EmittingTriangle& triangle = m_emitting_triangles[i];
        AABB3d bbox;
        bbox.invalidate();
        bbox.insert(triangle.m_v0);
        bbox.insert(triangle.m_v1);
        bbox.insert(triangle.m_v2);

        light_bboxes.push_back(bbox);

        m_items.push_back(Item(bbox, i, EmittingTriangleType));
    }

    // Create the partitioner.
    typedef bvh::MiddlePartitioner<AABBVector> Partitioner;
    Partitioner partitioner(light_bboxes);

    // Build the light tree.
    typedef bvh::Builder<LightTree, Partitioner> Builder;
    Builder builder;
    builder.build<DefaultWallclockTimer>(*this, partitioner, m_items.size(), 1);

    // Reorder m_items vector to match the ordering in the LightTree.
    if (!m_items.empty())
    {
        m_is_built = true;

        const vector<size_t>& ordering = partitioner.get_item_ordering();
        assert(m_items.size() == ordering.size());

        // Reorder items according to the tree ordering.
        ItemVector temp(ordering.size());
        small_item_reorder(
            &m_items[0],
            &temp[0],
            &ordering[0],
            ordering.size());

        // Set total node importance and level for each node of the LightTree.
        IndexLUT tri_index_to_node_index;
        tri_index_to_node_index.resize(m_emitting_triangles.size());
        recursive_node_update(0, 0, 0, tri_index_to_node_index);

        // Print light tree statistics.
        Statistics statistics;
        statistics.insert("nodes", m_nodes.size());
        statistics.insert("max tree depth", m_tree_depth);
        statistics.insert_time("total build time", builder.get_build_time());
        RENDERER_LOG_INFO("%s",
            StatisticsVector::make(
                "light tree statistics",
                statistics).to_string().c_str());
        
        return tri_index_to_node_index;
    }

    RENDERER_LOG_INFO("light tree not built - no light tree compatible lights in the scene.");
    return {};
}

bool LightTree::is_built() const
{
    return m_is_built;
}

float LightTree::recursive_node_update(
    const size_t    parent_index,
    const size_t    node_index, 
    const size_t    node_level,
    IndexLUT&       tri_index_to_node_index)
{
    float importance = 0.0f;

    if (!m_nodes[node_index].is_leaf())
    {
        const auto& child1 = m_nodes[node_index].get_child_node_index();
        const auto& child2 = m_nodes[node_index].get_child_node_index() + 1;

        const float importance1 = recursive_node_update(node_index, child1, node_level + 1, tri_index_to_node_index);
        const float importance2 = recursive_node_update(node_index, child2, node_level + 1, tri_index_to_node_index);

        importance = importance1 + importance2;
    }
    else
    {
        // Retrieve the light source associated to this leaf.
        const size_t item_index = m_nodes[node_index].get_item_index();
        const size_t light_index = m_items[item_index].m_light_index;

        if (m_items[item_index].m_light_type == NonPhysicalLightType)
        {
            const Light* light = m_non_physical_lights[light_index].m_light;
            
            // Retrieve the non physical light importance.
            Spectrum spectrum;
            light->get_inputs().find("intensity").source()->evaluate_uniform(spectrum);
            importance = average_value(spectrum);
        }
        else
        {
            assert(m_items[item_index].m_light_type == EmittingTriangleType);

            const EmittingTriangle& triangle = m_emitting_triangles[light_index];

            // Retrieve the emitting triangle importance.
            const EDF* edf = triangle.m_material->get_uncached_edf();
            importance = edf->get_uncached_max_contribution() * edf->get_uncached_importance_multiplier();
    
            // Save the index of the light tree node containing the EMT in the look up table.
            tri_index_to_node_index[light_index] = node_index;
        }

        // Keep track of the tree depth.
        if (m_tree_depth < node_level)
            m_tree_depth = node_level;
    }

    if (node_index == 0)
        m_nodes[node_index].set_root();
    else m_nodes[node_index].set_parent(parent_index);

    m_nodes[node_index].set_importance(importance);
    m_nodes[node_index].set_level(node_level);

    return importance;
}

void LightTree::sample(
    const Vector3d&     surface_point,
    float               s,
    LightType&          light_type,
    size_t&             light_index,
    float&              light_probability) const
{
    assert(is_built());

    light_probability = 1.0f;
    size_t node_index = 0;

    while (!m_nodes[node_index].is_leaf())
    {
        const auto& node = m_nodes[node_index];

        float p1, p2;
        child_node_probabilites(node, surface_point, p1, p2);

        if (s < p1)
        {
            light_probability *= p1;
            s /= p1;
            node_index = node.get_child_node_index();
        }
        else
        {
            light_probability *= p2;
            s = (s - p1) / p2;
            node_index = node.get_child_node_index() + 1;
        }
    }

    const size_t item_index = m_nodes[node_index].get_item_index();
    const Item& item = m_items[item_index];
    light_type = item.m_light_type;
    light_index = item.m_light_index;
}

float LightTree::evaluate_node_pdf(
    const Vector3d&     surface_point,
    size_t              node_index) const
{
    size_t parent_index = m_nodes[node_index].get_parent();
    float pdf = 1.0f;

    do
	{
        const LightTreeNode<AABB3d>& node = m_nodes[parent_index];

        float p1, p2;
        child_node_probabilites(node, surface_point, p1, p2);

        pdf *= node.get_child_node_index() == node_index ? p1 : p2;

        // Save the child index to be sure which probability should be taken
        // into consideration.
        node_index = parent_index;
        parent_index = m_nodes[node_index].get_parent();
    } while (!m_nodes[node_index].is_root());

    return pdf;
}

float LightTree::compute_node_probability(
    const LightTreeNode<AABB3d>&    node,
    const AABB3d&                   bbox,
    const Vector3d&                 surface_point) const
{
    // Calculate probability of a single node based on its distance
    // to the surface point being illuminated.
    // For leaf nodes use the actual position of the light source, instead
    // of center of the bbox. It is more precise for shaped lights.
    Vector3d position;
    const Item& item = m_items[node.get_item_index()];
    if (node.is_leaf() && item.m_light_type == EmittingTriangleType)
    {
        const size_t light_index = item.m_light_index;
        const EmittingTriangle& triangle = m_emitting_triangles[light_index];
        // Use centroid as triangle position approximation.
        position = (triangle.m_v0 + triangle.m_v1 + triangle.m_v2) * (1.0 / 3.0);
    }
    else
        position = bbox.center();

    const float squared_distance =
        static_cast<float>(square_distance(surface_point, position));

    return node.get_importance() / squared_distance;
}

void LightTree::child_node_probabilites(
    const LightTreeNode<AABB3d>&    node,
    const Vector3d&                 surface_point,
    float&                          p1,
    float&                          p2) const
{
    const auto& child1 = m_nodes[node.get_child_node_index()];
    const auto& child2 = m_nodes[node.get_child_node_index() + 1];

    // Node has currently no info about its own bbox characteristics.
    // Hence we have to extract it before from its parent.
    // TODO: make LightTreeNode aware of its bbox!
    const auto& bbox_left = node.get_left_bbox();
    const auto& bbox_right = node.get_right_bbox();

    p1 = compute_node_probability(child1, bbox_left, surface_point);
    p2 = compute_node_probability(child2, bbox_right, surface_point);

    // Normalize probabilities.
    const float total = p1 + p2;
    if (total <= 0.0f)
    {
        p1 = 0.5f;
        p2 = 0.5f;
    }
    else
    {
        p1 /= total;
        p2 /= total;
    }

    assert(feq(p1 + p2, 1.0f));
}

void LightTree::draw_tree_structure(
    const string&       filename_base,
    const AABB3d&       root_bbox,
    const bool          separate_by_levels) const
{
    // TODO: Add a possibility to shift each level of bboxes along the z-axis.

    const double Width = 0.1;

    if (separate_by_levels)
    {
        const char* color = "color.green";

        // Find nodes on each level of the tree and draw their child bboxes.
        for (size_t parent_level = 0; parent_level < m_tree_depth; parent_level++)
        {
            const auto filename = format("{0}_{1}.py", filename_base, parent_level + 1);
            VPythonFile file(filename.c_str());
            file.draw_axes(Width);

            // Draw the initial bbox.
            file.draw_aabb(root_bbox, color, Width);

            // Find every node at the parent level and draw its child bboxes.
            for (size_t i = 0; i < m_nodes.size(); ++i)
            {
                if (m_nodes[i].is_leaf())
                    continue;

                if (m_nodes[i].get_level() == parent_level)
                {
                    const auto& bbox_left = m_nodes[i].get_left_bbox();
                    const auto& bbox_right = m_nodes[i].get_right_bbox();

                    file.draw_aabb(bbox_left, color, Width);
                    file.draw_aabb(bbox_right, color, Width);
                }
            }
        }
    }
    else
    {
        const auto filename = format("{0}.py", filename_base);
        VPythonFile file(filename.c_str());
        file.draw_axes(Width);

        // Draw the initial bbox.
        file.draw_aabb(root_bbox, "color.yellow", Width);

        // Find nodes on each level of the tree and draw their child bboxes.
        for (size_t i = 0; i < m_nodes.size(); ++i)
        {
            if (m_nodes[i].is_leaf())
                continue;

            // Make even levels red and odd green.
            const char* color =
                m_nodes[i].get_level() % 2 != 0
                    ? "color.red"
                    : "color.green";

            const auto& bbox_left = m_nodes[i].get_left_bbox();
            const auto& bbox_right = m_nodes[i].get_right_bbox();

            file.draw_aabb(bbox_left, color, Width);
            file.draw_aabb(bbox_right, color, Width);
        }
    }
}

}   // namespace renderer
