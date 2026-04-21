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
    float3 backgroundColor;
};

float packFloat(float4 color)
{
    int rgba = (int(color.x * 255.0) << 24) +
               (int(color.y * 255.0) << 16) +
               (int(color.z * 255.0) << 8) +
               int(color.w * 255.0);
    return as_type<float>(rgba);
}

fragment float4 main_metal(
    PS_INPUT input [[stage_in]],
    depth2d<float, access::read> depthTexture [[texture(0)]],
    texture2d<float>             colorTexture [[texture(1)]],
    sampler colorSampler                      [[sampler(1)]],
    constant Params &p                        [[buffer(PARAMETER_SLOT)]])
{
    float tolerance = 1e-6f;

    // Read depth at exact pixel coords; Metal uses reversed-Z (1=near, 0=far)
    uint2 texel = uint2(input.gl_Position.xy);
    float fDepth = depthTexture.read(texel, 0);
    // Convert reversed-Z to standard Z, then linearize
    fDepth = 1.0f - fDepth;
    float linearDepth = p.projectionParams.y / (fDepth - p.projectionParams.x);

    float3 viewSpacePos = input.cameraDir * linearDepth;

    // convert to z-up convention
    float3 point = float3(-viewSpacePos.z, -viewSpacePos.x, viewSpacePos.y);

    float4 color = colorTexture.sample(colorSampler, input.uv0);

    if (point.x > p.far - tolerance)
    {
        if (isinf(p.max))
            point = float3(p.max, p.max, p.max);
        else
            point.x = p.max;
        color = float4(p.backgroundColor, 1.0f);
    }
    else if (point.x < p.near + tolerance)
    {
        if (isinf(p.min))
            point = float3(p.min, p.min, p.min);
        else
            point.x = p.min;
        color = float4(p.backgroundColor, 1.0f);
    }

    // gamma correct
    color = sqrt(color);

    float rgba = packFloat(color);
    return float4(point, rgba);
}
