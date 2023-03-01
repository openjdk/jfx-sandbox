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

#import "MetalMeshView.h"
#import "MetalPipelineManager.h"

#ifdef MESH_VERBOSE
#define MESH_LOG NSLog
#else
#define MESH_LOG(...)
#endif

@implementation MetalMeshView

- (MetalMeshView*) createMeshView:(MetalContext*)ctx
                             mesh:(MetalMesh*)mtlMesh
{
    self = [super init];
    if (self) {
        MESH_LOG(@"MetalMeshView_createMeshView()");
        context = ctx;
        mesh = mtlMesh;
        material = NULL;
        ambientLightColor[0] = 0;
        ambientLightColor[1] = 0;
        ambientLightColor[2] = 0;
        numLights = 0;
        lightsDirty = TRUE;
        cullMode = MTLCullModeNone;
        wireframe = FALSE;
    }
    return self;
}

- (void) setMaterial:(MetalPhongMaterial*)pMaterial
{
    MESH_LOG(@"MetalMeshView_setMaterial()");
    material = pMaterial;
}

- (void) setCullingMode:(int)cMode
{
    MESH_LOG(@"MetalMeshView_setCullingMode()");
    cullMode = cMode;
}

- (void) setWireframe:(bool)isWireFrame
{
    MESH_LOG(@"MetalMeshView_setWireframe()");
    wireframe = isWireFrame;
}

- (void) setAmbientLight:(float)r
                       g:(float)g
                       b:(float)b
{
    MESH_LOG(@"MetalMeshView_setAmbientLight()");
    ambientLightColor[0] = r;
    ambientLightColor[1] = g;
    ambientLightColor[2] = b;
}

- (void) setLight:(int)index
        x:(float)x y:(float)y z:(float)z
        r:(float)r g:(float)g b:(float)b w:(float)w
        ca:(float)ca la:(float)la qa:(float)qa
        isA:(float)isAttenuated range:(float)range
        dirX:(float)dirX dirY:(float)dirY dirZ:(float)dirZ
        inA:(float)innerAngle outA:(float)outerAngle
        falloff:(float)falloff
{
    MESH_LOG(@"MetalMeshView_setLight()");
    // NOTE: We only support up to 3 point lights at the present
    if (index >= 0 && index <= MAX_NUM_LIGHTS - 1) {
        MetalLight* light = ([[MetalLight alloc] createLight:x y:y z:z
            r:r g:g b:b w:w
            ca:ca la:la qa:qa
            isA:isAttenuated range:range
            dirX:dirX dirY:dirY dirZ:dirZ
            inA:innerAngle outA:outerAngle
            falloff:falloff]);
        lights[index] = light;
        lightsDirty = TRUE;
    }
}

- (MetalMesh*) getMesh
{
    return mesh;
}

- (int) getCullingMode
{
    return cullMode;
}

- (void) render
{
    // Prepare lights data
    float lightsPosition[MAX_NUM_LIGHTS * 4];      // 3 coords + 1 padding
    float lightsNormDirection[MAX_NUM_LIGHTS * 4]; // 3 coords + 1 padding
    float lightsColor[MAX_NUM_LIGHTS * 4];         // 3 color + 1 padding
    float lightsAttenuation[MAX_NUM_LIGHTS * 4];   // 3 attenuation factors + 1 isAttenuated
    float lightsRange[MAX_NUM_LIGHTS * 4];         // 1 maxRange + 3 padding
    float spotLightsFactors[MAX_NUM_LIGHTS * 4];   // 2 angles + 1 falloff + 1 padding
    for (int i = 0, d = 0, p = 0, c = 0, a = 0, r = 0, s = 0; i < MAX_NUM_LIGHTS; i++) {
        MetalLight* light = lights[i];

        lightsPosition[p++] = light->position[0];
        lightsPosition[p++] = light->position[1];
        lightsPosition[p++] = light->position[2];
        lightsPosition[p++] = 0;

        lightsNormDirection[d++] = light->direction[0];
        lightsNormDirection[d++] = light->direction[1];
        lightsNormDirection[d++] = light->direction[2];
        lightsNormDirection[d++] = 0;

        lightsColor[c++] = light->color[0];
        lightsColor[c++] = light->color[1];
        lightsColor[c++] = light->color[2];
        lightsColor[c++] = 1;

        lightsAttenuation[a++] = light->attenuation[0];
        lightsAttenuation[a++] = light->attenuation[1];
        lightsAttenuation[a++] = light->attenuation[2];
        lightsAttenuation[a++] = light->attenuation[3];

        lightsRange[r++] = light->maxRange;
        lightsRange[r++] = 0;
        lightsRange[r++] = 0;
        lightsRange[r++] = 0;

        if ([light isPointLight] || [light isDirectionalLight]) {
            spotLightsFactors[s++] = -1; // cos(180)
            spotLightsFactors[s++] = 2;  // cos(0) - cos(180)
            spotLightsFactors[s++] = 0;
            spotLightsFactors[s++] = 0;
        } else {
            // preparing for: I = pow((cosAngle - cosOuter) / (cosInner - cosOuter), falloff)
            float cosInner = cos(light->inAngle * M_PI / 180);
            float cosOuter = cos(light->outAngle * M_PI / 180);
            spotLightsFactors[s++] = cosOuter;
            spotLightsFactors[s++] = cosInner - cosOuter;
            spotLightsFactors[s++] = light->foff;
            spotLightsFactors[s++] = 0;
        }
    }

    id<MTLCommandBuffer> commandBuffer = [context getCurrentCommandBuffer];
    id<MTLRenderCommandEncoder> phongEncoder = [commandBuffer renderCommandEncoderWithDescriptor:[context getPhongRPD]];
    id<MTLRenderPipelineState> phongPipelineState =
        [[context getPipelineManager] getPhongPipeStateWithFragFuncName:@"PhongPS"];
    [phongEncoder setRenderPipelineState:phongPipelineState];
    // In Metal default winding order is Clockwise but the vertex data that
    // we are getting is in CounterClockWise order, so we need to set
    // MTLWindingCounterClockwise explicitly
    [phongEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [phongEncoder setCullMode:cullMode];
    simd_float4x4 mvpMatrix = [context getMVPMatrix];
    vector_float4 camPos = [context getCameraPosition];
    [phongEncoder setVertexBytes:&mvpMatrix
                               length:sizeof(mvpMatrix)
                              atIndex:1];
    simd_float4x4 worldMatrix = [context getWorldMatrix];
    [phongEncoder setVertexBytes:&worldMatrix
                               length:sizeof(worldMatrix)
                              atIndex:2];
    [phongEncoder setVertexBytes:&lightsPosition
                               length:sizeof(lightsPosition)
                              atIndex:3];
    [phongEncoder setVertexBytes:&lightsNormDirection
                               length:sizeof(lightsNormDirection)
                              atIndex:4];
    [phongEncoder setVertexBytes:&camPos
                               length:sizeof(camPos)
                              atIndex:5];
    id<MTLBuffer> vBuffer = [mesh getVertexBuffer];
    [phongEncoder setVertexBuffer:vBuffer
                           offset:0
                            atIndex:0];
    [phongEncoder setFragmentBytes:&lightsAttenuation
                                length:sizeof(lightsAttenuation)
                                atIndex:0];
    [phongEncoder setFragmentBytes:&lightsColor
                                length:sizeof(lightsColor)
                                atIndex:1];
    [phongEncoder setFragmentBytes:&lightsRange
                                length:sizeof(lightsRange)
                                atIndex:2];
    [phongEncoder setFragmentBytes:&spotLightsFactors
                                length:sizeof(spotLightsFactors)
                                atIndex:3];
    [phongEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
        indexCount:[mesh getNumIndices]
        indexType:MTLIndexTypeUInt16
        indexBuffer:[mesh getIndexBuffer]
        indexBufferOffset:0];
    [phongEncoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    [context updatePhongLoadAction];
}

@end // MetalMeshView
