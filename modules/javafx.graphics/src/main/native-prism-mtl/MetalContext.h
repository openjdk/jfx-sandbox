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

@interface MetalContext : NSObject
{
    float byteToFloatTable[256];
    simd_float4x4 mvpMatrix;
    simd_float4x4 worldMatrix;
    VS_INPUT vertices[85];//TODO: MTL: this should not exceed 4KB if we need to use setVertexBytes
    NSUInteger numTriangles;
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    id<MTLCommandBuffer> currentCommandBuffer;
    id<MTLRenderCommandEncoder> currentRenderEncoder;
    id<MTLRenderPipelineState> currentPipeState;
    id<MTLBuffer> currentFragArgBuffer;
    MetalShader* currentShader;
    // TODO: MTL: Currently this argBufArray is used to keep a track of all the MTLBuffers that are used
    // as argument buffers for each drawIndexedQuads call that get accumulated in a single MTLCommandBuffer.
    // All these buffers are released once the MTLCommandBuffer completes.
    // This should be improved to reduce un-necessary allocations and release. Ideally by using a
    // MTLHeap of size 1 to 2 MB. [Refer MetalShader.getArgumentBuffer()]
    NSMutableArray* argBufArray;

    MetalResourceFactory* resourceFactory;

    MTLScissorRect scissorRect;
    bool isScissorEnabled;
    MetalRTTexture* rtt;
    bool rttCleared;
    bool clearDepthTexture;
    float clearColor[4];
    MTLRenderPassDescriptor* rttPassDesc;
    MTLLoadAction rttLoadAction;
    //MTLRenderPipelineDescriptor* passThroughPipeDesc;
    //id<MTLRenderPipelineState> passThroughPipeState;

    MetalPipelineManager* pipelineManager;
    MetalPhongShader *phongShader;
    MTLRenderPassDescriptor* phongRPD;
    vector_float4 cPos;
    bool depthEnabled;
}

- (MetalPipelineManager*) getPipelineManager;
- (MetalShader*) getCurrentShader;
- (void) setCurrentShader:(MetalShader*) shader;
- (void) setCurrentPipeState:(id<MTLRenderPipelineState>) pipeState;
- (void) setCurrentArgumentBuffer:(id<MTLBuffer>) argBuffer;

- (void) commitCurrentCommandBuffer;
- (id<MTLDevice>) getDevice;
- (id<MTLCommandBuffer>) getCurrentCommandBuffer;
- (id<MTLRenderCommandEncoder>) getCurrentRenderEncoder;
- (void) endCurrentRenderEncoder;
- (void) resetRenderPass;

- (void) setRTT:(MetalRTTexture*)rttPtr;
- (MetalRTTexture*) getRTT;
- (void) clearRTT:(int)color red:(float)red green:(float)green blue:(float)blue alpha:(float)alpha
                        clearDepth:(bool)clearDepth ignoreScissor:(bool)ignoreScissor;
- (void) setClipRect:(int)x y:(int)y width:(int)width height:(int)height;
- (void) resetClip;

- (void) fillVB:(struct PrismSourceVertex const *)pSrcFloats colors:(char const *)pSrcColors
                  numVertices:(int)numVerts;
- (NSInteger) drawIndexedQuads:(struct PrismSourceVertex const *)pSrcXYZUVs
                      ofColors:(char const *)pSrcColors
                   vertexCount:(NSUInteger)numVerts;
- (void) drawClearRect:(struct PrismSourceVertex const *)pSrcXYZUVs
              ofColors:(char const *)pSrcColors
           vertexCount:(NSUInteger)numVerts;

- (void) resetProjViewMatrix;
- (void) setProjViewMatrix:(bool)isOrtho
        m00:(float)m00 m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33;

- (void) setProjViewMatrix:(float)m00
        m01:(float)m01 m02:(float)m02 m03:(float)m03
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
- (void) updatePhongLoadAction;
- (MTLRenderPassDescriptor*) getPhongRPD;
- (simd_float4x4) getMVPMatrix;
- (simd_float4x4) getWorldMatrix;
- (void) setCameraPosition:(float)x
        y:(float)y z:(float)z;
- (vector_float4) getCameraPosition;
- (MTLScissorRect) getScissorRect;
- (bool) isDepthEnabled;
- (bool) isScissorEnabled;
- (void) dealloc;

@end

#endif
