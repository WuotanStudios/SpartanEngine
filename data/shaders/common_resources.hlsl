/*
Copyright(c) 2016-2024 Panos Karabelas

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

#ifndef SPARTAN_COMMON_RESOURCES
#define SPARTAN_COMMON_RESOURCES

// a constant buffer that updates once per frame
struct FrameBufferData
{
    matrix view;
    matrix view_inverted;
    matrix view_previous;
    matrix projection;
    matrix projection_inverted;
    matrix projection_previous;
    matrix view_projection;
    matrix view_projection_inverted;
    matrix view_projection_orthographic;
    matrix view_projection_unjittered;
    matrix view_projection_previous;
    matrix view_projection_previous_unjittered;

    float2 resolution_render;
    float2 resolution_output;

    float2 taa_jitter_current;
    float2 taa_jitter_previous;
    
    float directional_light_intensity;
    float delta_time;
    uint frame;
    uint options;
    
    float3 camera_position;
    float camera_near;
    
    float3 camera_direction;
    float camera_far;

    float camera_last_movement_time;
    float hdr_enabled;
    float hdr_max_nits;
    float hdr_white_point;

    float3 camera_position_previous;
    float resolution_scale;
    
    double time;
    float2 padding;
};

// 128 byte push constant buffer used by every pass
struct PassBufferData
{
    matrix transform;
    matrix values;
};

// struct which forms a bindless material parameters array
struct MaterialParameters
{
    float4 color;

    float2 tiling;
    float2 offset;

    float roughness;
    float metallness;
    float normal;
    float height;

    uint flags;
    float world_space_height;
    float ior;
    float subsurface_scattering;
    
    float  sheen;
    float3 sheen_tint;
    
    float anisotropic;
    float anisotropic_rotation;
    float clearcoat;
    float clearcoat_roughness;
};

// struct which forms a bindless light parameters array
struct LightParameters
{
    matrix transform[2];

    float4 color;

    float3 position;
    float intensity;

    float3 direction;
    float range;

    float angle;
    uint flags;
    float2 padding;
};

//= RESOURCES ======================================================================================
// g-buffer
Texture2D tex_albedo         : register(t0);
Texture2D tex_normal         : register(t1);
Texture2D tex_material       : register(t2);
Texture2D tex_velocity       : register(t3);
Texture2D tex_depth          : register(t4);
Texture2D tex_depth_backface : register(t5);
Texture2D tex_depth_opaque   : register(t6);
// lighting
Texture2D tex_light_diffuse     : register(t7);
Texture2D tex_light_diffuse_gi  : register(t8);
Texture2D tex_light_specular    : register(t9);
Texture2D tex_light_specular_gi : register(t10);
Texture2D tex_light_shadow      : register(t11);
Texture2D tex_light_volumetric  : register(t12);
// shadow maps
Texture2DArray tex_light_depth : register(t13);
Texture2DArray tex_light_color : register(t14);
// misc
Texture2D tex_lut_ibl     : register(t15);
Texture2D tex_environment : register(t16);
Texture2D tex_ssao        : register(t17);
Texture2D tex_ssr         : register(t18);
Texture2D tex_frame       : register(t19);
Texture2D tex             : register(t20);
Texture2D tex2            : register(t21);
Texture2D tex_font_atlas  : register(t22);
Texture2DArray tex_sss    : register(t23);

// bindless arrays
Texture2D material_textures[]                              : register(t24, space1);
SamplerComparisonState samplers_comparison[]               : register(s0,  space2);
SamplerState samplers[]                                    : register(s1,  space3);
RWStructuredBuffer<MaterialParameters> material_parameters : register(u0);
RWStructuredBuffer<LightParameters> light_parameters       : register(u1);

// storage textures/buffers
RWTexture2D<float4> tex_uav                                : register(u2);
RWTexture2D<float4> tex_uav2                               : register(u3);
RWTexture2D<float4> tex_uav3                               : register(u4);
RWTexture2D<float4> tex_uav4                               : register(u5);
RWTexture2DArray<float4> tex_uav_sss                       : register(u6);
globallycoherent RWStructuredBuffer<uint> g_atomic_counter : register(u7); // used by FidelityFX SPD
globallycoherent RWTexture2D<float4> tex_uav_mips[12]      : register(u8); // used by FidelityFX SPD

// buffers
[[vk::push_constant]]
PassBufferData buffer_pass;
cbuffer BufferFrame : register(b0) { FrameBufferData buffer_frame;  };
//==================================================================================================

// == EASY ACCESS FUNCTIONS/DEFINES =======================================================================================================================
// buffers
bool is_taa_enabled()                { return any(buffer_frame.taa_jitter_current); }
bool is_ssr_enabled()                { return buffer_frame.options & uint(1U << 0); }
bool is_ssao_enabled()               { return buffer_frame.options & uint(1U << 1); }
matrix pass_get_transform_previous() { return buffer_pass.values; }
float2 pass_get_f2_value()           { return float2(buffer_pass.values._m23, buffer_pass.values._m30); }
float3 pass_get_f3_value()           { return float3(buffer_pass.values._m00, buffer_pass.values._m01, buffer_pass.values._m02); }
float3 pass_get_f3_value2()          { return float3(buffer_pass.values._m20, buffer_pass.values._m21, buffer_pass.values._m31); }
float4 pass_get_f4_value()           { return float4(buffer_pass.values._m10, buffer_pass.values._m11, buffer_pass.values._m12, buffer_pass.values._m33); }
uint pass_get_material_index()       { return buffer_pass.values._m03; }
bool pass_is_transparent()           { return buffer_pass.values._m13 == 1.0f; }
bool pass_is_opaque()                { return !pass_is_transparent(); }
// _m32 is available for use

// binldess array indices
static const uint material_texture_slots_per_type  = 4;
static const uint material_texture_index_albedo    = 0 * material_texture_slots_per_type;
static const uint material_texture_index_roughness = 1 * material_texture_slots_per_type;
static const uint material_texture_index_metalness = 2 * material_texture_slots_per_type;
static const uint material_texture_index_normal    = 3 * material_texture_slots_per_type;
static const uint material_texture_index_occlusion = 4 * material_texture_slots_per_type;
static const uint material_texture_index_emission  = 5 * material_texture_slots_per_type;
static const uint material_texture_index_height    = 6 * material_texture_slots_per_type;
static const uint material_texture_index_mask      = 7 * material_texture_slots_per_type;
static const uint material_texture_index_packed    = 8 * material_texture_slots_per_type;

static const uint sampler_compare_depth         = 0;
static const uint sampler_point_clamp_edge      = 0;
static const uint sampler_point_clamp_border    = 1;
static const uint sampler_point_wrap            = 2;
static const uint sampler_bilinear_clamp        = 3;
static const uint sampler_bilinear_clamp_border = 4;
static const uint sampler_bilinear_wrap         = 5;
static const uint sampler_trilinear_clamp       = 6;
static const uint sampler_anisotropic_wrap      = 7;

// bindless array access
#define GET_TEXTURE(index_texture) material_textures[pass_get_material_index() + index_texture]
MaterialParameters GetMaterial() { return material_parameters[pass_get_material_index()]; }
#define GET_SAMPLER(index_sampler) samplers[index_sampler]
// ========================================================================================================================================================

#endif // SPARTAN_COMMON_RESOURCES
