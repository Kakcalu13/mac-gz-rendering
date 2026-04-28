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

// PS_INPUT matches Ogre/Compositor/Quad_vs_Metal output
struct PS_INPUT
{
    float2 uv0;
    float4 gl_Position [[position]];
};

fragment float4 main_metal(
    PS_INPUT input [[stage_in]],
    texture2d<float> cubeUVTex [[texture(0)]],
    sampler sampler0            [[sampler(0)]],
    texture2d<float> tex0       [[texture(1)]],
    sampler sampler1            [[sampler(1)]],
    texture2d<float> tex1       [[texture(2)]],
    sampler sampler2            [[sampler(2)]],
    texture2d<float> tex2       [[texture(3)]],
    sampler sampler3            [[sampler(3)]],
    texture2d<float> tex3       [[texture(4)]],
    sampler sampler4            [[sampler(4)]],
    texture2d<float> tex4       [[texture(5)]],
    sampler sampler5            [[sampler(5)]],
    texture2d<float> tex5       [[texture(6)]],
    sampler sampler6            [[sampler(6)]])
{
    float3 data = cubeUVTex.sample(sampler0, input.uv0).xyz;

    // which cube face to sample, and uv on that face
    float faceIdx = data.z;
    float2 uv = data.xy;

    float2 d = float2(0.0f, 0.0f);
    if (faceIdx == 0)
        d = tex0.sample(sampler1, uv).xy;
    else if (faceIdx == 1)
        d = tex1.sample(sampler2, uv).xy;
    else if (faceIdx == 2)
        d = tex2.sample(sampler3, uv).xy;
    else if (faceIdx == 3)
        d = tex3.sample(sampler4, uv).xy;
    else if (faceIdx == 4)
        d = tex4.sample(sampler5, uv).xy;
    else if (faceIdx == 5)
        d = tex5.sample(sampler6, uv).xy;

    return float4(d.x, d.y, 0.0f, 1.0f);
}
