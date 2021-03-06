
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
// Copyright (c) 2014-2017 Francois Beaune, The appleseedhq Organization
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
#include "lightsample.h"

// appleseed.renderer headers.
#include "renderer/kernel/intersection/intersector.h"
#include "renderer/kernel/shading/shadingpoint.h"

// Standard headers.
#include <cassert>
#include <string>

using namespace foundation;
using namespace std;

namespace renderer
{

//
// LightSample class implementation.
//

void LightSample::make_shading_point(
    ShadingPoint&           shading_point,
    const Vector3d&         direction,
    const Intersector&      intersector) const
{
    assert(m_triangle && !m_light);

    intersector.manufacture_hit(
        shading_point,
        ShadingRay(
            m_point,
            direction,
            0.0,
            0.0,
            ShadingRay::Time(),
            VisibilityFlags::CameraRay, 0),
        ShadingPoint::PrimitiveTriangle,    // note: we assume light samples are always on triangles (and not on curves)
        m_bary,
        m_triangle->m_assembly_instance,
        m_triangle->m_assembly_instance->transform_sequence().get_earliest_transform(),
        m_triangle->m_object_instance_index,
        m_triangle->m_region_index,
        m_triangle->m_triangle_index,
        m_triangle->m_triangle_support_plane);
}

} // namespace renderer

