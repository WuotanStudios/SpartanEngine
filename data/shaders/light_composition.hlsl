/*
Copyright(c) 2016-2025 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

// = INCLUDES ========
#include "common.hlsl"
#include "fog.hlsl"
//====================

struct translucency
{
    struct refraction
    {
        static float compute_fade_factor(float2 uv)
        {
            float edge_threshold = 0.05f; // how close to the edge to start fading
            float2 edge_distance = min(uv, 1.0f - uv);
            return saturate(min(edge_distance.x, edge_distance.y) / edge_threshold);
        }
        
        static float3 refract_vector(float3 i, float3 n, float eta)
        {
            // snell's Law
            float cosi  = dot(-i, n);
            float cost2 = 1.0f - eta * eta * (1.0f - cosi * cosi);
            return eta * i + (eta * cosi - sqrt(abs(cost2))) * n;
        }
        
        static float3 get_color(Surface surface)
        {
            const float scale = 0.05f;
            
            // compute refraction vector
            float3 normal_vector        = world_to_view(surface.normal, false);
            float3 incident_vector      = world_to_view(surface.camera_to_pixel, false);
            float3 refraction_direction = refract_vector(incident_vector, normal_vector, 1.0f / surface.ior);
            
            // compute refracted uv
            float2 refraction_uv_offset = refraction_direction.xy * (scale / surface.camera_to_pixel_length);
            float2 refracted_uv         = saturate(surface.uv + refraction_uv_offset);

            // don't refract what's behind the surface
            float depth_surface    = get_linear_depth(surface.depth); // depth transparent
            float depth_refraction = get_linear_depth(get_depth_opaque(refracted_uv)); // depth opaque
            float is_behind        = depth_surface < depth_refraction;
            refracted_uv           = lerp(refracted_uv, refracted_uv, is_behind);
    
            // get base color
            float frame_mip_count   = pass_get_f3_value().x;
            float mip_level         = lerp(0, frame_mip_count, surface.roughness_alpha);
            float3 color            = tex_frame.SampleLevel(GET_SAMPLER(sampler_bilinear_clamp), surface.uv, mip_level).rgb;
            float3 color_refraction = tex_frame.SampleLevel(GET_SAMPLER(sampler_bilinear_clamp), refracted_uv, mip_level).rgb;
        
            // screen fade
            float fade_factor = compute_fade_factor(refracted_uv);
            color             = lerp(color, color_refraction, fade_factor);
    
            return color;
        }
    };
    
    struct water
    {
        static float3 get_color(Surface surface, inout float alpha)
        {
            const float MAX_DEPTH            = 100.0f;
            const float ALPHA_FACTOR         = 0.2f;
            const float FOAM_DEPTH_THRESHOLD = 2.0f;
            const float3 light_absorption    = float3(0.3f, 0.2f, 0.1f); // color spectrum light absorption

            // compute water depth
            float water_level       = get_position(surface.uv).y;
            float water_floor_level = get_position(get_depth_opaque(surface.uv), surface.uv).y;
            float water_depth       = clamp(water_level - water_floor_level, 0.0f, MAX_DEPTH);

            // compute color and alpha at that depth with slight adjustments
            float3 color = float3(exp(-light_absorption.x * water_depth), exp(-light_absorption.y * water_depth), exp(-light_absorption.z * water_depth));
            alpha        = 1.0f - exp(-water_depth * ALPHA_FACTOR);
            
            return color;
        }
    };
};

[numthreads(THREAD_GROUP_COUNT_X, THREAD_GROUP_COUNT_Y, 1)]
void main_cs(uint3 thread_id : SV_DispatchThreadID)
{
    // create surface
    float2 resolution_out;
    tex_uav.GetDimensions(resolution_out.x, resolution_out.y);
    Surface surface;
    surface.Build(thread_id.xy, resolution_out, true, false);

    // initialize
    float3 light_diffuse       = 0.0f;
    float3 light_specular      = 0.0f;
    float3 light_refraction    = 0.0f;
    float3 light_emissive      = 0.0f;
    float3 light_atmospheric   = 0.0f;
    float alpha                = 0.0f;
    float distance_from_camera = 0.0f;

    // during the compute pass, fill in the sky pixels
    if (surface.is_sky() && pass_is_opaque())
    {
        light_emissive       = tex_environment.SampleLevel(samplers[sampler_bilinear_clamp], direction_sphere_uv(surface.camera_to_pixel), 0).rgb;
        alpha                = 0.0f;
        distance_from_camera = FLT_MAX_16;
    }
    // for the opaque pass, fill in the opaque pixels, and for the transparent pass, fill in the transparent pixels
    else if ((pass_is_opaque() && surface.is_opaque()) || (pass_is_transparent() && surface.is_transparent()))
    {
        light_diffuse        = tex_light_diffuse[thread_id.xy].rgb;
        light_specular       = tex_light_specular[thread_id.xy].rgb;
        light_emissive       = surface.emissive * surface.albedo * 10.0f;
        alpha                = surface.alpha;
        distance_from_camera = surface.camera_to_pixel_length;
        
        // transparent
        if (surface.is_transparent())
        {
            // refraction
            light_refraction = translucency::refraction::get_color(surface);

            // water
            if (surface.is_water())
            {
                light_refraction *= translucency::water::get_color(surface, alpha);
                // override g-buffer albedo alpha, for the IBL pass, right after
                tex_uav2[thread_id.xy]  = float4(surface.albedo, alpha);
            }
        }
    }

    // fog
    {
        // atmospheric
        float max_mip     = pass_get_f3_value().x;
        float3 sky_color  = tex_environment.SampleLevel(samplers[sampler_trilinear_clamp], float2(0.5, 0.5f), max_mip).rgb;
        light_atmospheric = got_fog_atmospheric(distance_from_camera, surface.position.y, buffer_frame.directional_light_intensity) * sky_color;

        // volumetric
        light_atmospheric += tex_light_volumetric[thread_id.xy].rgb; // already uses sky color
    }
    
    tex_uav[thread_id.xy] = float4(light_diffuse * surface.albedo + light_specular + light_emissive + light_refraction + light_atmospheric, alpha);
}
