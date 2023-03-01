/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <metal_stdlib>
#include <simd/simd.h>
#include "PhongVSDecl.h"
#include "PhongVS2PS.h"
using namespace metal;

void quatToMatrix(float4 q, float3 N[3]) {
    float3 t1 = q.xyz * q.yzx * 2;
    float3 t2 = q.zxy * q.www * 2;
    float3 t3 = q.xyz * q.xyz * 2;
    float3 t4 = 1 - (t3 + t3.yzx);

    float3 r1 = t1 + t2;
    float3 r2 = t1 - t2;

    N[0] = float3(t4.y, r1.x, r2.z);
    N[1] = float3(r2.x, t4.z, r1.y);
    N[2] = float3(r1.z, r2.y, t4.x);

    N[2] *= (q.w >= 0) ? 1 : -1;   // ATI normal map generator compatibility
}

float3 getLocalVector(float3 global, float3 N[3]) {
    return float3(dot(global, N[1]), dot(global, N[2]), dot(global, N[0]));
}

vertex VS_PHONG_INOUT PhongVS(const uint v_id [[ vertex_id ]],
                      constant VS_PHONG_INPUT * v_in [[ buffer(0) ]],
                      constant float4x4 & mvp_matrix [[ buffer(1) ]],
                      constant float4x4 & world_matrix [[ buffer(2) ]],
                      constant float4 & lightsPos [[ buffer(3) ]],
                      constant float4 & lightsDir [[ buffer(4) ]],
                      constant float4 & cameraPos [[ buffer(5) ]])
{
    VS_PHONG_INOUT out;
    out.texCoord = v_in[v_id].texCoord;
    float4 worldVertexPos = world_matrix * (float4(v_in[v_id].position, 1.0));

    out.position = mvp_matrix * worldVertexPos;

    float3 n[3];
    quatToMatrix(v_in[v_id].normal, n);
    float3x3 sWorldMatrix = float3x3(world_matrix[0].xyz,
                                     world_matrix[1].xyz,
                                     world_matrix[2].xyz);
    for (int i = 0; i != 3; ++i) {
        n[i] = (sWorldMatrix) * n[i];
    }

    float3 worldVecToEye = cameraPos.xyz - worldVertexPos.xyz;
    out.worldVecToEye = getLocalVector(worldVecToEye, n);

    float3 worldVecToLight = (lightsPos).xyz - worldVertexPos.xyz;
    out.worldVecsToLights1 = getLocalVector(worldVecToLight, n);
    float3 worldNormLightDir = (lightsDir).xyz;
    out.worldNormLightDirs1 = getLocalVector(worldNormLightDir, n);

    worldVecToLight = (lightsPos + 1).xyz - worldVertexPos.xyz;
    out.worldVecsToLights2 = getLocalVector(worldVecToLight, n);
    worldNormLightDir = (lightsDir + 1).xyz;
    out.worldNormLightDirs2 = getLocalVector(worldNormLightDir, n);

    worldVecToLight = (lightsPos + 2).xyz - worldVertexPos.xyz;
    out.worldVecsToLights3 = getLocalVector(worldVecToLight, n);
    worldNormLightDir = (lightsDir + 2).xyz;
    out.worldNormLightDirs3 = getLocalVector(worldNormLightDir, n);

    return out;
}

// TODO : Vertex shader with vertexdescriptor, we may need to use this in future,
// if not we can remove it
/*vertex VS_PHONG_INOUT PhongVS(VS_PHONG_INPUT in [[stage_in]],
                      constant float4x4 & mvp_matrix [[ buffer(1) ]],
                      constant float4x4 & world_matrix [[ buffer(2) ]])
{
    VS_PHONG_INOUT out;
    float4x4 mvpMatrix = mvp_matrix * world_matrix;
    out.position = mvpMatrix * in.position;
    return out;
}*/
