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
};

struct Params
{
    float near;
    float far;
    float min;
    float max;
};

fragment float4 main_metal(
    PS_INPUT input [[stage_in]],
    texture2d<float> inputTexture [[texture(0)]],
    constant Params &p            [[buffer(PARAMETER_SLOT)]])
{
    float tolerance = 1e-6f;

    // Use gl_Position.xy (window-space pixel coords) for exact integer texel
    // access — equivalent to GLSL texelFetch. Avoids needing texResolution.
    uint2 texel = uint2(input.gl_Position.xy);
    float4 px = inputTexture.read(texel, 0);

    float3 point = px.xyz;

    if (point.x > p.far - tolerance)
    {
        if (isinf(p.max))
            point = float3(p.max, p.max, p.max);
        else
            point.x = p.max;
    }
    else if (point.x < p.near + tolerance)
    {
        if (isinf(p.min))
            point = float3(p.min, p.min, p.min);
        else
            point.x = p.min;
    }

    return float4(point, px.a);
}
