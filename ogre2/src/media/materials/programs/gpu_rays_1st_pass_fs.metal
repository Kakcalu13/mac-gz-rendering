/*
 * Copyright (C) 2024 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <metal_stdlib>
using namespace metal;

struct PS_INPUT
{
    float4 gl_Position [[position]];
    float2 uv0;
    float3 cameraDir;
};

struct Params
{
    float2 projectionParams;
    float near;
    float far;
    float min;
    float max;
};

fragment float4 main_metal(
    PS_INPUT input [[stage_in]],
    depth2d<float, access::read> depthTexture [[texture(0)]],
    texture2d<float>             colorTexture [[texture(1)]],
    sampler colorSampler                      [[sampler(1)]],
    constant Params &p                        [[buffer(PARAMETER_SLOT)]])
{
    // Read depth at exact pixel coords; Metal uses reversed-Z (1=near, 0=far)
    uint2 texel = uint2(input.gl_Position.xy);
    float fDepth = depthTexture.read(texel, 0);
    // Convert reversed-Z to standard Z, then linearize
    fDepth = 1.0f - fDepth;
    float linearDepth = p.projectionParams.y / (fDepth - p.projectionParams.x);

    float3 viewSpacePos = input.cameraDir * linearDepth;

    // get retro value
    float retro = colorTexture.sample(colorSampler, input.uv0).x * 2000.0f;

    // get range (length of 3D point in view space)
    float l = length(viewSpacePos);

    if (l > p.far)
        l = p.max;
    else if (l < p.near)
        l = p.min;

    return float4(l, retro, 0.0f, 1.0f);
}
