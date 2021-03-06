
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
#include "physicalsurfaceshader.h"

// appleseed.renderer headers.
#include "renderer/kernel/aov/aovaccumulator.h"
#include "renderer/kernel/lighting/ilightingengine.h"
#include "renderer/kernel/shading/shadingcomponents.h"
#include "renderer/kernel/shading/shadingcontext.h"
#include "renderer/kernel/shading/shadingpoint.h"
#include "renderer/modeling/input/inputarray.h"
#include "renderer/modeling/surfaceshader/surfaceshader.h"
#include "renderer/utility/paramarray.h"

// appleseed.foundation headers.
#include "foundation/utility/api/specializedapiarrays.h"
#include "foundation/utility/containers/dictionary.h"

// Standard headers.
#include <cstddef>

// Forward declarations.
namespace renderer  { class PixelContext; }
namespace renderer  { class Project; }

using namespace foundation;
using namespace std;

namespace renderer
{

namespace
{
    //
    // A surface shader that uses physically-based rendering to shade pixels.
    //

    const char* Model = "physical_surface_shader";

    class PhysicalSurfaceShader
      : public SurfaceShader
    {
      public:
        PhysicalSurfaceShader(
            const char*                 name,
            const ParamArray&           params)
          : SurfaceShader(name, params)
        {
            m_inputs.declare("color_multiplier", InputFormatFloat, "1.0");
            m_inputs.declare("alpha_multiplier", InputFormatFloat, "1.0");
        }

        virtual void release() override
        {
            delete this;
        }

        virtual const char* get_model() const override
        {
            return Model;
        }

        virtual bool on_frame_begin(
            const Project&              project,
            const BaseGroup*            parent,
            OnFrameBeginRecorder&       recorder,
            IAbortSwitch*               abort_switch) override
        {
            m_lighting_samples = m_params.get_optional<size_t>("lighting_samples", 1);
            return true;
        }

        virtual void evaluate(
            SamplingContext&            sampling_context,
            const PixelContext&         pixel_context,
            const ShadingContext&       shading_context,
            const ShadingPoint&         shading_point,
            AOVAccumulatorContainer&    aov_accumulators) const override
        {
            // Evaluate the shader inputs.
            InputValues values;
            m_inputs.evaluate(
                shading_context.get_texture_cache(),
                shading_point.get_uv(0),
                &values);

            // Compute lighting.
            ShadingComponents radiance;
            for (size_t i = 0, e = m_lighting_samples; i < e; ++i)
            {
                shading_context.get_lighting_engine()->compute_lighting(
                    sampling_context,
                    pixel_context,
                    shading_context,
                    shading_point,
                    radiance);
            }
            if (m_lighting_samples > 1)
                radiance /= static_cast<float>(m_lighting_samples);

            // Accumulate into AOVs.
            aov_accumulators.write(radiance, values.m_color_multiplier);

            // Apply alpha multiplier.
            aov_accumulators.alpha().apply_multiplier(Alpha(values.m_alpha_multiplier));
        }

      private:
        APPLESEED_DECLARE_INPUT_VALUES(InputValues)
        {
            float   m_color_multiplier;
            float   m_alpha_multiplier;
        };

        size_t      m_lighting_samples;
    };
}


//
// PhysicalSurfaceShaderFactory class implementation.
//

const char* PhysicalSurfaceShaderFactory::get_model() const
{
    return Model;
}

Dictionary PhysicalSurfaceShaderFactory::get_model_metadata() const
{
    return
        Dictionary()
            .insert("name", Model)
            .insert("label", "Physical")
            .insert("default_model", "true");
}

DictionaryArray PhysicalSurfaceShaderFactory::get_input_metadata() const
{
    DictionaryArray metadata;

    metadata.push_back(
        Dictionary()
            .insert("name", "color_multiplier")
            .insert("label", "Color Multiplier")
            .insert("type", "colormap")
            .insert("entity_types",
                Dictionary().insert("texture_instance", "Textures"))
            .insert("default", "1.0")
            .insert("use", "optional"));

    metadata.push_back(
        Dictionary()
            .insert("name", "alpha_multiplier")
            .insert("label", "Alpha Multiplier")
            .insert("type", "colormap")
            .insert("entity_types",
                Dictionary().insert("texture_instance", "Textures"))
            .insert("default", "1.0")
            .insert("use", "optional"));

    metadata.push_back(
        Dictionary()
            .insert("name", "lighting_samples")
            .insert("label", "Lighting Samples")
            .insert("type", "integer")
            .insert("min",
                Dictionary()
                    .insert("value", "1")
                    .insert("type", "hard"))
            .insert("max",
                Dictionary()
                    .insert("value", "1000")
                    .insert("type", "soft"))
            .insert("default", "1")
            .insert("use", "optional"));

    return metadata;
}

auto_release_ptr<SurfaceShader> PhysicalSurfaceShaderFactory::create(
    const char*         name,
    const ParamArray&   params) const
{
    return auto_release_ptr<SurfaceShader>(new PhysicalSurfaceShader(name, params));
}

auto_release_ptr<SurfaceShader> PhysicalSurfaceShaderFactory::static_create(
    const char*         name,
    const ParamArray&   params)
{
    return auto_release_ptr<SurfaceShader>(new PhysicalSurfaceShader(name, params));
}

}   // namespace renderer
