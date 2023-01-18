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
using namespace metal;

struct VS_PHONG_INPUT {
    packed_float3 position;
    packed_float2 texCoord;
    packed_float4 normal;
};

/*struct VS_PHONG_INPUT {
    float4 position [[attribute(0)]];
    float4 texCoord [[attribute(1)]];
    float4 normal [[attribute(2)]];
};*/

typedef struct VS_PHONG_INOUT {
    float4 position [[position]];
} VS_PHONG_INOUT;

vertex VS_PHONG_INOUT PhongVS(const uint v_id [[ vertex_id ]],
                      constant VS_PHONG_INPUT * v_in [[ buffer(0) ]],
                      constant float4x4 & mvp_matrix [[ buffer(1) ]],
                      constant float4x4 & world_matrix [[ buffer(2) ]])
{
    VS_PHONG_INOUT out;
    float4x4 mvpMatrix = mvp_matrix * world_matrix;
    out.position = mvpMatrix * float4(v_in[v_id].position, 1.0);
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
