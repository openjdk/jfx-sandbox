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
#include "PhongPSDecl.h"
#include "PhongVS2PS.h"
using namespace metal;

float computeSpotlightFactor3(float3 l, float3 lightDir, float cosOuter, float denom, float falloff) {
    float cosAngle = dot(normalize(-lightDir), l);
    float cutoff = cosAngle - cosOuter;
    if (falloff != 0) {
        return pow(saturate(cutoff / denom), falloff);
    }
    return cutoff >= 0 ? 1 : 0;
}

fragment float4 PhongPS(VS_PHONG_INOUT vert [[stage_in]],
                        constant PS_PHONG_UNIFORMS & psUniforms [[ buffer(0) ]],
                        constant float4 & lightsAttenuation [[ buffer(1) ]],
                        constant float4 & lightsColor [[ buffer(2) ]],
                        constant float4 & lightsRange [[ buffer(3) ]],
                        constant float4 & spotLightsFactors [[ buffer(4) ]])
{
    //return float4(1.0, 0.0, 0.0, 1.0);
    float3 normal = float3(0, 0, 1);
    float4 tSpec = float4(0, 0, 0, 0);
    float specPower = 0;

    // lighting
    float3 worldNormVecToEye = normalize(vert.worldVecToEye);
    float3 refl = reflect(worldNormVecToEye, normal);
    float3 diffLightColor = 0;
    float3 specLightColor = 0;

    // testing if w is 0 or 1 using <0.5 since equality check for floating points might not work well
    if ((lightsAttenuation + 0).w < 0.5) {
        diffLightColor += saturate(dot(normal, -vert.worldNormLightDirs1)) * (lightsColor + 0).rgb;
        specLightColor += pow(saturate(dot(-refl, -vert.worldNormLightDirs1)), specPower) * (lightsColor + 0).rgb;
    } else {
        float dist = length(vert.worldVecsToLights1);
        if (dist <= (lightsRange + 0).x) {
            float3 l = normalize(vert.worldVecsToLights1);

            float cosOuter = (spotLightsFactors + 0).x;
            float denom = (spotLightsFactors + 0).y;
            float falloff = (spotLightsFactors + 0).z;
            float spotlightFactor = computeSpotlightFactor3(l, vert.worldNormLightDirs1, cosOuter, denom, falloff);

            float ca = (lightsAttenuation + 0).x;
            float la = (lightsAttenuation + 0).y;
            float qa = (lightsAttenuation + 0).z;
            float invAttnFactor = ca + la * dist + qa * dist * dist;

            float3 attenuatedColor = (lightsColor + 0).rgb * spotlightFactor / invAttnFactor;
            diffLightColor += saturate(dot(normal, l)) * attenuatedColor;
            specLightColor += pow(saturate(dot(-refl, l)), specPower) * attenuatedColor;
        }
    }

    float3 ambLightColor = psUniforms.ambientLightColor.rgb;

    float3 rez = (ambLightColor + diffLightColor) *
        (psUniforms.diffuseColor.rgb) + specLightColor * tSpec.rgb;

    return float4(saturate(rez), 1.0);
}
