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

float NTSC_Gray(float3 color) {
    return dot(color, float3(0.299, 0.587, 0.114));
}

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
                        texture2d<float> mapDiffuse [[ texture(0) ]],
                        texture2d<float> mapSpecular [[ texture(1) ]],
                        texture2d<float> mapBump [[ texture(2) ]])
{
    //return float4(1.0, 0.0, 0.0, 1.0);

    float2 texD = vert.texCoord;

    // TODO : MTL : This is default filter and addressmode
    // set in both OpenGL and D3D, currently i am setting it
    // directly here, in future we can optimise it to be passed
    // as sampler state.
    constexpr sampler mipmapSampler(filter::linear,
                                mip_filter::linear,
                                   address::repeat);
    float4 tDiff = mapDiffuse.sample(mipmapSampler, texD);
    if (tDiff.a == 0.0) discard_fragment();
    tDiff = tDiff * psUniforms.diffuseColor;

    float3 normal = float3(0, 0, 1);
    constexpr sampler nonMipmapSampler(filter::linear,
                                      address::repeat);
    //bump
    if (psUniforms.isBumpMap) {
        float4 BumpSpec = mapBump.sample(nonMipmapSampler, texD);
        normal = normalize(BumpSpec.xyz * 2 - 1);
    }
    // specular
    float4 tSpec = float4(0, 0, 0, 0);
    float specPower = 0;
    if (psUniforms.isSpecColor || psUniforms.isSpecMap) {
        specPower = psUniforms.specColor.a;
        if (psUniforms.isSpecColor) { // Color
            tSpec.rgb = psUniforms.specColor.rgb;
        }
        if (psUniforms.isSpecMap) { // Texture
            tSpec = mapSpecular.sample(nonMipmapSampler, texD);
            specPower *= NTSC_Gray(tSpec.rgb);
        }
        if (psUniforms.isSpecColor && psUniforms.isSpecMap) { // Mix
            tSpec = mapSpecular.sample(nonMipmapSampler, texD);
            specPower *= NTSC_Gray(tSpec.rgb);
            tSpec.rgb *= psUniforms.specColor.rgb;
        }
    }

    // lighting
    float3 worldNormVecToEye = normalize(vert.worldVecToEye);
    float3 refl = reflect(worldNormVecToEye, normal);
    float3 diffLightColor = 0;
    float3 specLightColor = 0;

    for (int i = 0; i < vert.numLights; i++) {
        // TODO: MTL: Implementation using array of scalars
        // which is not working
        /*float3 light = float3(vert.worldVecsToLights[(i * 3)],
                              vert.worldVecsToLights[(i * 3) + 1],
                              vert.worldVecsToLights[(i * 3) + 2]);
        float3 lightDir = float3(vert.worldNormLightDirs[(i * 3)],
                                 vert.worldNormLightDirs[(i * 3) + 1],
                                 vert.worldNormLightDirs[(i * 3) + 2]);*/
        float3 light;
        float3 lightDir;
        switch (i) {
            case 0 :
                light = vert.worldVecsToLights1;
                lightDir = vert.worldNormLightDirs1;
                break;
            case 1 :
                light = vert.worldVecsToLights2;
                lightDir = vert.worldNormLightDirs2;
                break;
            case 2 :
                light = vert.worldVecsToLights3;
                lightDir = vert.worldNormLightDirs3;
                break;
        }
        float4 lightColor = float4(psUniforms.lightsColor[(i * 4)],
                                   psUniforms.lightsColor[(i * 4) + 1],
                                   psUniforms.lightsColor[(i * 4) + 2],
                                   1.0);
        float4 lightAttenuation = float4(psUniforms.lightsAttenuation[(i * 4)],
                                         psUniforms.lightsAttenuation[(i * 4) + 1],
                                         psUniforms.lightsAttenuation[(i * 4) + 2],
                                         psUniforms.lightsAttenuation[(i * 4) + 3]);
        float4 lightRange = float4(psUniforms.lightsRange[(i * 4)],
                                   0.0,
                                   0.0,
                                   0.0);
        float4 spotLightsFactor = float4(psUniforms.spotLightsFactors[(i * 4)],
                                         psUniforms.spotLightsFactors[(i * 4) + 1],
                                         psUniforms.spotLightsFactors[(i * 4) + 2],
                                         psUniforms.spotLightsFactors[(i * 4) + 3]);
        // testing if w is 0 or 1 using <0.5 since equality check for floating points might not work well
        if ((lightAttenuation).w < 0.5) {
            diffLightColor += saturate(dot(normal, -lightDir)) * (lightColor).rgb;
            specLightColor += pow(saturate(dot(-refl, -lightDir)), specPower) * (lightColor).rgb;
        } else {
            float dist = length(light);
            if (dist <= (lightRange).x) {
                float3 l = normalize(light);

                float cosOuter = (spotLightsFactor).x;
                float denom = (spotLightsFactor).y;
                float falloff = (spotLightsFactor).z;
                float spotlightFactor = computeSpotlightFactor3(l, lightDir, cosOuter, denom, falloff);

                float ca = (lightAttenuation).x;
                float la = (lightAttenuation).y;
                float qa = (lightAttenuation).z;
                float invAttnFactor = ca + la * dist + qa * dist * dist;

                float3 attenuatedColor = (lightColor).rgb * spotlightFactor / invAttnFactor;
                diffLightColor += saturate(dot(normal, l)) * attenuatedColor;
                specLightColor += pow(saturate(dot(-refl, l)), specPower) * attenuatedColor;
            }
        }
    }

    float3 ambLightColor = psUniforms.ambientLightColor.rgb;

    float3 rez = (ambLightColor + diffLightColor) *
        (tDiff.rgb) + specLightColor * tSpec.rgb;

    return float4(saturate(rez), tDiff.a);
}
