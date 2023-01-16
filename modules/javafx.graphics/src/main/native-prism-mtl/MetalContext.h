/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
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

#ifndef METAL_CONTEXT_H
#define METAL_CONTEXT_H

#import "MetalCommon.h"
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

@class MetalTexture;
@class MetalRTTexture;
@class MetalResourceFactory;
@class MetalPipelineManager;
@class MetalShader;
@class MetalPhongShader;
@class MetalPhongMaterial;
@class MetalMeshView;

struct PrismSourceVertex {
    float x, y, z;
    float tu1, tv1;
    float tu2, tv2;
};

typedef struct VS_INPUT {
    vector_float2 position;
    vector_float4 color;
    vector_float2 texCoord0;
    vector_float2 texCoord1;
} VS_INPUT;

typedef enum VertexInputIndex {
    VertexInputIndexVertices = 0,
    VertexInputMatrixMVP = 1,
} VertexInputIndex;

typedef struct VS_PHONG_INPUT {
    vector_float4 position;
    vector_float4 texCoord;
    vector_float4 normal;
} VS_PHONG_INPUT;

@interface MetalContext : NSObject
{
    simd_float4x4 mvpMatrix;
    simd_float4x4 worldMatrix;
    VS_INPUT vertices[85];//TODO: MTL: this should not exceed 4KB if we need to use setVertexBytes
    NSUInteger numTriangles;
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    id<MTLCommandBuffer> currentCommandBuffer;
    id<MTLRenderPipelineState> currentPipeState;
    id<MTLBuffer> currentFragArgBuffer;
    MetalShader* currentShader;

    MetalResourceFactory* resourceFactory;

    MetalRTTexture* rtt;
    MetalTexture* tex0;
    MTLRenderPassDescriptor* rttPassDesc;
    MTLLoadAction rttLoadAction;
    //MTLRenderPipelineDescriptor* passThroughPipeDesc;
    //id<MTLRenderPipelineState> passThroughPipeState;

    MetalPipelineManager* pipelineManager;
    MetalPhongShader *phongShader;
}

- (MetalPipelineManager*) getPipelineManager;
- (MetalShader*) getCurrentShader;
- (void) setCurrentShader:(MetalShader*) shader;
- (void) setCurrentPipeState:(id<MTLRenderPipelineState>) pipeState;
- (void) setCurrentArgumentBuffer:(id<MTLBuffer>) argBuffer;

- (id<MTLDevice>) getDevice;
- (id<MTLCommandBuffer>) newCommandBuffer;
- (id<MTLCommandBuffer>) newCommandBuffer:(NSString*)label;
- (id<MTLCommandBuffer>) getCurrentCommandBuffer;
- (void) setRTTLoadActionToClear;
- (void) resetRenderPass;

- (void) setTex0:(MetalTexture*)texPtr;
- (MetalTexture*) getTex0;
- (void) setRTT:(MetalRTTexture*)rttPtr;
- (MetalRTTexture*) getRTT;
- (void) fillVB:(struct PrismSourceVertex const *)pSrcFloats colors:(char const *)pSrcColors
                  numVertices:(int)numVerts;
- (NSInteger) drawIndexedQuads:(struct PrismSourceVertex const *)pSrcFloats
                                   ofColors:(char const *)pSrcColors vertexCount:(NSUInteger)numVerts;
- (void) resetProjViewMatrix;
- (void) setProjViewMatrix:(bool)isOrtho
        m00:(float)m00 m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33;

- (void) setWorldTransformMatrix:(float)m00
        m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33;

- (void) setWorldTransformIdentityMatrix:(float)m00
        m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33;

- (NSInteger) setDeviceParametersFor3D;

- (void) renderMeshView:(MetalMeshView*)meshView;
@end

#endif
