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

#import <jni.h>
#import <dlfcn.h>
#import <stdlib.h>
#import <assert.h>
#import <stdio.h>
#import <string.h>
#import <math.h>

#import "MetalContext.h"
#import "MetalRTTexture.h"
#import "MetalPipelineManager.h"
#import "MetalShader.h"
#import "com_sun_prism_mtl_MTLContext.h"
#import "MetalMesh.h"
#import "MetalPhongShader.h"
#import "MetalMeshView.h"
#import "MetalPhongMaterial.h"

#ifdef CTX_VERBOSE
#define CTX_LOG NSLog
#else
#define CTX_LOG(...)
#endif

@implementation MetalContext

- (id) createContext:(dispatch_data_t)shaderLibData
{
    CTX_LOG(@"-> MetalContext.createContext()");
    self = [super init];
    if (self) {
        argsRingBuffer = [[MetalRingBuffer alloc] init:ARGS_BUFFER_SIZE];
        dataRingBuffer = [[MetalRingBuffer alloc] init:DATA_BUFFER_SIZE];
        transientBuffersForCB = [[NSMutableArray alloc] init];
        shadersUsedInCB = [[NSMutableSet alloc] init];
        isScissorEnabled = false;
        commitOnDraw = false;
        currentRenderEncoder = nil;
        meshIndexCount = 0;
        linearSamplerDict = [[NSMutableDictionary alloc] init];
        nonLinearSamplerDict = [[NSMutableDictionary alloc] init];
        compositeMode = com_sun_prism_mtl_MTLContext_MTL_COMPMODE_SRCOVER; //default

        device = MTLCreateSystemDefaultDevice();
        currentBufferIndex = 0;
        commandQueue = [device newCommandQueue];
        commandQueue.label = @"The only MTLCommandQueue";
        pipelineManager = [MetalPipelineManager alloc];
        [pipelineManager init:self libData:shaderLibData];

        rttPassDesc = [MTLRenderPassDescriptor new];
        rttPassDesc.colorAttachments[0].clearColor  = MTLClearColorMake(1, 1, 1, 1); // make this programmable
        rttPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
        rttPassDesc.colorAttachments[0].loadAction  = MTLLoadActionLoad;

        for (short i = 0; i < 256; i++) {
            byteToFloatTable[i] = ((float)i) / 255.0f;
        }

        pixelBuffer = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];

        // clearing rtt related initialization
        identityMatrixBuf = [device newBufferWithLength:sizeof(simd_float4x4)
                                                options:MTLResourceStorageModePrivate];
        id<MTLBuffer> tMatBuf = [self getTransientBufferWithLength:sizeof(simd_float4x4)];
        simd_float4x4* identityMatrix = (simd_float4x4*)tMatBuf.contents;

        *identityMatrix = matrix_identity_float4x4;

        clearEntireRttVerticesBuf = [device newBufferWithLength:sizeof(CLEAR_VS_INPUT) * 4
                                                        options:MTLResourceStorageModePrivate];
        id<MTLBuffer> tclearVertBuf = [self getTransientBufferWithLength:sizeof(CLEAR_VS_INPUT) * 4];
        CLEAR_VS_INPUT* clearEntireRttVertices = (CLEAR_VS_INPUT*)tclearVertBuf.contents;

        clearEntireRttVertices[0].position.x = -1; clearEntireRttVertices[0].position.y = -1;
        clearEntireRttVertices[1].position.x = -1; clearEntireRttVertices[1].position.y =  1;
        clearEntireRttVertices[2].position.x =  1; clearEntireRttVertices[2].position.y = -1;
        clearEntireRttVertices[3].position.x =  1; clearEntireRttVertices[3].position.y =  1;

        // Create Index Buffer
        indexBuffer = [device newBufferWithLength:(INDICES_PER_IB * sizeof(unsigned short))
                                          options:MTLResourceStorageModePrivate];
        id<MTLBuffer> tIndexBuffer = [self getTransientBufferWithLength:
                                               (INDICES_PER_IB * sizeof(unsigned short))];
        unsigned short* indices = (unsigned short*)tIndexBuffer.contents;
        for (unsigned short i = 0, j = 0; i < INDICES_PER_IB; j += 4, i += 6) {
            indices[i + 0] = 0 + j; // 0, 4,  8, 12
            indices[i + 1] = 1 + j; // 1, 5,  9, 13
            indices[i + 2] = 2 + j; // 2, 6, 10, 14

            indices[i + 3] = 1 + j; // 1, 5,  9, 13
            indices[i + 4] = 2 + j; // 2, 6, 10, 14
            indices[i + 5] = 3 + j; // 3, 7, 11, 15
        }

        id<MTLCommandBuffer> commandBuffer = [self getCurrentCommandBuffer];
        @autoreleasepool {
            id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
            [blitEncoder copyFromBuffer:tIndexBuffer
                           sourceOffset:(NSUInteger)0
                               toBuffer:indexBuffer
                      destinationOffset:(NSUInteger)0
                                   size:tIndexBuffer.length];

            [blitEncoder copyFromBuffer:tclearVertBuf
                           sourceOffset:(NSUInteger)0
                               toBuffer:clearEntireRttVerticesBuf
                      destinationOffset:(NSUInteger)0
                                   size:tclearVertBuf.length];

            [blitEncoder copyFromBuffer:tMatBuf
                           sourceOffset:(NSUInteger)0
                               toBuffer:identityMatrixBuf
                      destinationOffset:(NSUInteger)0
                                   size:tMatBuf.length];

            [blitEncoder endEncoding];
        }
        [self commitCurrentCommandBuffer:false];
    }
    return self;
}

- (int) setRTT:(MetalRTTexture*)rttPtr
{
    if (rtt != rttPtr) {
        CTX_LOG(@"-> Native: MetalContext.setRTT() endCurrentRenderEncoder");
        [self endCurrentRenderEncoder];
    }
    // TODO: MTL:
    // The method can possibly be optmized(with no significant gain in FPS)
    // to avoid updating RenderPassDescriptor if the render target
    // is not being changed.
    rtt = rttPtr;
    id<MTLTexture> mtlTex = [rtt getTexture];
    [self validatePixelBuffer:(mtlTex.width * mtlTex.height * 4)];
    CTX_LOG(@"-> Native: MetalContext.setRTT() %lu , %lu",
                    [rtt getTexture].width, [rtt getTexture].height);
    if ([rttPtr isMSAAEnabled]) {
        rttPassDesc.colorAttachments[0].storeAction = MTLStoreActionStoreAndMultisampleResolve;
        rttPassDesc.colorAttachments[0].texture = [rtt getMSAATexture];
        rttPassDesc.colorAttachments[0].resolveTexture = [rtt getTexture];
    } else {
        rttPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
        rttPassDesc.colorAttachments[0].texture = [rtt getTexture];
        rttPassDesc.colorAttachments[0].resolveTexture = nil;
    }
    [self resetClipRect];
    return 1;
}

- (MetalRTTexture*) getRTT
{
    CTX_LOG(@"-> Native: MetalContext.getRTT()");
    return rtt;
}

- (void) validatePixelBuffer:(NSUInteger)length
{
    if ([pixelBuffer length] < length) {
        [transientBuffersForCB addObject:pixelBuffer];
        pixelBuffer = [device newBufferWithLength:length options:MTLResourceStorageModeShared];
    }
}

- (id<MTLBuffer>) getPixelBuffer
{
    return pixelBuffer;
}

- (MetalRingBuffer*) getArgsRingBuffer
{
    return argsRingBuffer;
}

- (MetalRingBuffer*) getDataRingBuffer
{
    return dataRingBuffer;
}

- (id<MTLBuffer>) getTransientBufferWithBytes:(const void *)pointer length:(NSUInteger)length
{
    id<MTLBuffer> transientBuf = [device newBufferWithBytes:pointer
                                                     length:length
                                                    options:MTLResourceStorageModeShared];
    [transientBuffersForCB addObject:transientBuf];
    commitOnDraw = true;
    return transientBuf;
}

- (id<MTLBuffer>) getTransientBufferWithLength:(NSUInteger)length
{
    id<MTLBuffer> transientBuf = [device newBufferWithLength:length
                                                     options:MTLResourceStorageModeShared];
    [transientBuffersForCB addObject:transientBuf];
    commitOnDraw = true;
    return transientBuf;
}

- (id<MTLSamplerState>) getSampler:(bool)isLinear
                          wrapMode:(int)wrapMode
{
    NSMutableDictionary* samplerDict;
    if (isLinear) {
        samplerDict = linearSamplerDict;
    } else {
        samplerDict = nonLinearSamplerDict;
    }
    NSNumber *keyWrapMode = [NSNumber numberWithInt:wrapMode];
    id<MTLSamplerState> sampler = samplerDict[keyWrapMode];
    if (sampler == nil) {
        sampler = [self createSampler:isLinear wrapMode:wrapMode];
        [samplerDict setObject:sampler forKey:keyWrapMode];
    }
    return sampler;
}

- (id<MTLSamplerState>) createSampler:(bool)isLinear
                             wrapMode:(int)wrapMode
{
    MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];
    if (isLinear) {
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    }
    if (wrapMode != -1) {
        samplerDescriptor.sAddressMode = wrapMode;
        samplerDescriptor.tAddressMode = wrapMode;
    }
    id<MTLSamplerState> sampler = [[self getDevice] newSamplerStateWithDescriptor:samplerDescriptor];
    [samplerDescriptor release];
    return sampler;
}

- (id<MTLDevice>) getDevice
{
    // CTX_LOG(@"MetalContext.getDevice()");
    return device;
}

- (void) commitCurrentCommandBuffer
{
    [self commitCurrentCommandBuffer:false];
}

- (void) commitCurrentCommandBufferAndWait
{
    [self commitCurrentCommandBuffer:true];
}

- (void) commitCurrentCommandBuffer:(bool)waitUntilCompleted
{
    [self endCurrentRenderEncoder];

    NSMutableArray* bufsForCB = transientBuffersForCB;
    transientBuffersForCB = [[NSMutableArray alloc] init];

    for (MetalShader* shader in shadersUsedInCB) {
        [shader setArgsUpdated:true];
    }
    [shadersUsedInCB removeAllObjects];

    unsigned int rbid = [MetalRingBuffer getCurrentBufferIndex];
    [currentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
        [MetalRingBuffer resetBuffer:rbid];
        for (id buffer in bufsForCB) {
            [buffer release];
        }
        [bufsForCB removeAllObjects];
        [bufsForCB release];
    }];
    commitOnDraw = false;
    [currentCommandBuffer commit];

    if (waitUntilCompleted || ![MetalRingBuffer isBufferAvailable]) {
        [currentCommandBuffer waitUntilCompleted];
    }

    [currentCommandBuffer release];
    currentCommandBuffer = nil;
    [MetalRingBuffer updateBufferInUse];
    [argsRingBuffer resetOffsets];
    [dataRingBuffer resetOffsets];
}

- (id<MTLCommandBuffer>) getCurrentCommandBuffer
{
    CTX_LOG(@"MetalContext.getCurrentCommandBuffer() --- current value = %p", currentCommandBuffer);
    if (currentCommandBuffer == nil
                || currentCommandBuffer.status != MTLCommandBufferStatusNotEnqueued) {

        @autoreleasepool {
            // The commandBuffer creation using commandQueue returns
            // an autoreleased object. We need this object at a class level as it
            // gets used in other class methods.
            // Take up the ownership of this commandBuffer object using retain.
            currentCommandBuffer = [[commandQueue commandBuffer] retain];
        }
        currentCommandBuffer.label = @"JFX Command Buffer";
    }
    return currentCommandBuffer;
}

- (id<MTLRenderCommandEncoder>) getCurrentRenderEncoder
{
    if (currentRenderEncoder == nil) {
        CTX_LOG(@"MetalContext.getCurrentRenderEncoder() is nil");
        id<MTLCommandBuffer> cb = [self getCurrentCommandBuffer];

        @autoreleasepool {
            // The RenderEncoder creation using command buffer returns
            // an autoreleased object. We need this object at a class level as it
            // gets used in other class methods.
            // Take up the ownership of this RenderEncoder object using retain.
            currentRenderEncoder = [[cb renderCommandEncoderWithDescriptor:rttPassDesc] retain];
        }
    }
    return currentRenderEncoder;
}

- (void) endCurrentRenderEncoder
{
    if (currentRenderEncoder != nil) {
        CTX_LOG(@"MetalContext.endCurrentRenderEncoder()");
        meshIndexCount = 0;
        [currentRenderEncoder endEncoding];
        [currentRenderEncoder release];
        currentRenderEncoder = nil;
    }
}

- (NSUInteger) getCurrentBufferIndex
{
    return currentBufferIndex;
}

- (id<MTLRenderPipelineState>) getPhongPipelineState
{
    return [[self getPipelineManager] getPhongPipeStateWithFragFuncName:@"PhongPS"
                compositeMode:[self getCompositeMode]];
}

- (NSInteger) drawIndexedQuads:(struct PrismSourceVertex const *)pSrcXYZUVs
                      ofColors:(char const *)pSrcColors
                   vertexCount:(NSUInteger)numVertices
{

    CTX_LOG(@"MetalContext.drawIndexedQuads()");

    CTX_LOG(@"numVertices = %lu", numVertices);

    int vbLength = sizeof(VS_INPUT) * numVertices;
    int numQuads = numVertices / 4;
    int numIndices = numQuads * 6;

    id<MTLBuffer> vertexBuffer = [dataRingBuffer getBuffer];
    int offset = [dataRingBuffer reserveBytes:vbLength];

    if (offset < 0) {
        vertexBuffer = [self getTransientBufferWithLength:vbLength];
        offset = 0;
    }

    [self fillVB:pSrcXYZUVs
          colors:pSrcColors
     numVertices:numVertices
              vb:(vertexBuffer.contents + offset)];

    id<MTLRenderCommandEncoder> renderEncoder = [self getCurrentRenderEncoder];

    [renderEncoder setFrontFacingWinding:MTLWindingClockwise];
    [renderEncoder setCullMode:MTLCullModeNone];
    [renderEncoder setTriangleFillMode:MTLTriangleFillModeFill];

    [renderEncoder setVertexBytes:&mvpMatrix
                           length:sizeof(mvpMatrix)
                          atIndex:VertexInputMatrixMVP];

    MetalShader* shader = [self getCurrentShader];
    [shadersUsedInCB addObject:shader];

    [renderEncoder setRenderPipelineState:[shader getPipelineState:[rtt isMSAAEnabled]
                                                     compositeMode:compositeMode]];

    if ([shader getArgumentBufferLength] != 0) {
        [shader copyArgBufferToRingBuffer];
        [renderEncoder setFragmentBuffer:[shader getRingBuffer]
                                  offset:[shader getRingBufferOffset]
                                 atIndex:0];

        NSMutableDictionary* texturesDict = [shader getTexutresDict];
        if ([texturesDict count] > 0) {
            for (NSString *key in texturesDict) {
                id<MTLTexture> tex = texturesDict[key];
                CTX_LOG(@"    Value: %@ for key: %@", tex, key);
                [renderEncoder useResource:tex usage:MTLResourceUsageRead];
            }

            NSMutableDictionary* samplersDict = [shader getSamplersDict];
            for (NSNumber *key in samplersDict) {
                id<MTLSamplerState> sampler = samplersDict[key];
                CTX_LOG(@"    Value: %@ for key: %@", sampler, key);
                [renderEncoder setFragmentSamplerState:sampler atIndex:[key integerValue]];
            }
        }
    }

    [renderEncoder setScissorRect:[self getScissorRect]];

    for (int i = 0; numIndices > 0; i++) {
        [renderEncoder setVertexBuffer:vertexBuffer
                                offset:(offset + (i * VERTICES_PER_IB * sizeof(VS_INPUT)))
                               atIndex:VertexInputIndexVertices];

        [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                  indexCount:((numIndices > INDICES_PER_IB) ? INDICES_PER_IB : numIndices)
                                   indexType:MTLIndexTypeUInt16
                                 indexBuffer:indexBuffer
                           indexBufferOffset:0];
        numIndices -= INDICES_PER_IB;
    }

    if (commitOnDraw) {
        [self commitCurrentCommandBuffer];
    }
    return 1;
}

- (void) setProjViewMatrix:(bool)depthTest
        m00:(float)m00 m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33
{
    CTX_LOG(@"MetalContext.setProjViewMatrix() : depthTest %d", depthTest);
    mvpMatrix = simd_matrix(
        (simd_float4){ m00, m01, m02, m03 },
        (simd_float4){ m10, m11, m12, m13 },
        (simd_float4){ m20, m21, m22, m23 },
        (simd_float4){ m30, m31, m32, m33 }
    );
    if (depthTest &&
        ([rtt getDepthTexture] != nil)) {
        CTX_LOG(@"MetalContext.setProjViewMatrix() enable depth testing");
        depthEnabled = true;
    } else {
        CTX_LOG(@"MetalContext.setProjViewMatrix() disable depth testing");
        depthEnabled = false;
    }
    [self updateDepthDetails:depthTest];
}

- (void) setProjViewMatrix:(float)m00
        m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33
{
    CTX_LOG(@"MetalContext.setProjViewMatrix()");
    mvpMatrix = simd_matrix(
        (simd_float4){ m00, m01, m02, m03 },
        (simd_float4){ m10, m11, m12, m13 },
        (simd_float4){ m20, m21, m22, m23 },
        (simd_float4){ m30, m31, m32, m33 }
    );
}

- (void) setWorldTransformMatrix:(float)m00
        m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33
{
    CTX_LOG(@"MetalContext.setWorldTransformMatrix()");
    worldMatrix = simd_matrix(
        (simd_float4){ m00, m01, m02, m03 },
        (simd_float4){ m10, m11, m12, m13 },
        (simd_float4){ m20, m21, m22, m23 },
        (simd_float4){ m30, m31, m32, m33 }
    );
}

- (void) setWorldTransformIdentityMatrix
{
    CTX_LOG(@"MetalContext.setWorldTransformIdentityMatrix()");
    worldMatrix = matrix_identity_float4x4;
}

- (void) resetRenderPass
{
    CTX_LOG(@"MetalContext.resetRenderPass()");
    if (depthEnabled) {
        rttPassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
    }
}

- (void) clearRTT:(float)red
            green:(float)green
             blue:(float)blue
            alpha:(float)alpha
       clearDepth:(bool)clearDepth
{
    CTX_LOG(@">>>> MetalContext.clearRTT() %f, %f, %f, %f", red, green, blue, alpha);
    CTX_LOG(@">>>> MetalContext.clearRTT() %d", clearDepth);

    clearDepthTexture = clearDepth;
    clearColor[0] = red;
    clearColor[1] = green;
    clearColor[2] = blue;
    clearColor[3] = alpha;

    id<MTLRenderCommandEncoder> renderEncoder = [self getCurrentRenderEncoder];

    [renderEncoder setRenderPipelineState:[pipelineManager getClearRttPipeState]];
    [renderEncoder setFrontFacingWinding:MTLWindingClockwise];
    [renderEncoder setCullMode:MTLCullModeNone];
    [renderEncoder setTriangleFillMode:MTLTriangleFillModeFill];

    [renderEncoder setScissorRect:[self getScissorRect]];

    if (isScissorEnabled) {
        CTX_LOG(@"     MetalContext.clearRTT()     clearing scissor rect");

        [renderEncoder setVertexBytes:&mvpMatrix
                               length:sizeof(mvpMatrix)
                              atIndex:VertexInputMatrixMVP];

        [renderEncoder setVertexBytes:clearScissorRectVertices
                               length:sizeof(clearScissorRectVertices)
                              atIndex:VertexInputIndexVertices];
    } else {
        CTX_LOG(@"     MetalContext.clearRTT()     clearing whole rtt");

        [renderEncoder setVertexBuffer:identityMatrixBuf
                                offset:0
                               atIndex:VertexInputMatrixMVP];

        [renderEncoder setVertexBuffer:clearEntireRttVerticesBuf
                                offset:0
                               atIndex:VertexInputIndexVertices];
    }

    [renderEncoder setFragmentBytes:clearColor
                             length:sizeof(clearColor)
                            atIndex:VertexInputClearColor];

    [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                              indexCount:6
                               indexType:MTLIndexTypeUInt16
                             indexBuffer:indexBuffer
                       indexBufferOffset:0];

    if (clearDepthTexture && !depthEnabled) {
        [self endCurrentRenderEncoder];
    }

    CTX_LOG(@"<<<< MetalContext.clearRTT()");
}

- (void) renderMeshView:(MetalMeshView*)meshView
{
    MetalMesh* mesh = [meshView getMesh];
    NSUInteger indexCount = [mesh getNumIndices];
    CTX_LOG(@"MetalContext.renderMeshView() indexCount : %lu", indexCount);
    meshIndexCount += indexCount;
    CTX_LOG(@"MetalContext.renderMeshView() meshIndexCount : %lu", meshIndexCount);
    if (meshIndexCount >= MESH_INDEX_LIMIT) {
        CTX_LOG(@"MetalContext.renderMeshView() meshIndexCount is hitting limit");
        [self endCurrentRenderEncoder];
    }
    [meshView render];
}

- (void) setClipRect:(int)x y:(int)y width:(int)width height:(int)height
{
    CTX_LOG(@">>>> MetalContext.setClipRect()");
    CTX_LOG(@"     MetalContext.setClipRect() x = %d, y = %d, width = %d, height = %d", x, y, width, height);
    id<MTLTexture> currRtt = [rtt getTexture];
    int x1 = x + width;
    int y1 = y + height;
    if (x <= 0 && y <= 0 && x1 >= currRtt.width && y1 >= currRtt.height) {
        CTX_LOG(@"     MetalContext.setClipRect() 1 resetting clip, %lu, %lu", currRtt.width, currRtt.height);
        [self resetClipRect];
    } else {
        CTX_LOG(@"     MetalContext.setClipRect() 2");
        if (x < 0)                    x = 0;
        if (y < 0)                    y = 0;
        if (x1 > currRtt.width)  width  = currRtt.width - x;
        if (y1 > currRtt.height) height = currRtt.height - y;
        if (x > x1)              width  = x = 0;
        if (y > y1)              height = y = 0;
        scissorRect.x = x;
        scissorRect.y = y;
        scissorRect.width  = width;
        scissorRect.height = height;
        isScissorEnabled = true;

        clearScissorRectVertices[0].position.x = scissorRect.x;
        clearScissorRectVertices[0].position.y = scissorRect.y;
        clearScissorRectVertices[1].position.x = scissorRect.x;
        clearScissorRectVertices[1].position.y = scissorRect.y + scissorRect.height;
        clearScissorRectVertices[2].position.x = scissorRect.x + scissorRect.width;
        clearScissorRectVertices[2].position.y = scissorRect.y;
        clearScissorRectVertices[3].position.x = scissorRect.x + scissorRect.width;
        clearScissorRectVertices[3].position.y = scissorRect.y + scissorRect.height;
    }
    CTX_LOG(@"<<<< MetalContext.setClipRect()");
}

- (void) resetClipRect
{
    CTX_LOG(@">>>> MetalContext.resetClipRect()");
    isScissorEnabled = false;
    scissorRect.x = 0;
    scissorRect.y = 0;
    scissorRect.width  = 0;
    scissorRect.height = 0;
}

- (void) resetProjViewMatrix
{
    CTX_LOG(@"MetalContext.resetProjViewMatrix()");
    mvpMatrix = matrix_identity_float4x4;
}

- (void) fillVB:(struct PrismSourceVertex const *)pSrcXYZUVs
         colors:(char const *)pSrcColors
    numVertices:(int)numVertices
             vb:(void*)vb
{
    CTX_LOG(@"fillVB : numVertices = %d", numVertices);

    VS_INPUT* pVert = (VS_INPUT*)vb;
    unsigned char const* colors = (unsigned char*)(pSrcColors);
    struct PrismSourceVertex const * inVerts = pSrcXYZUVs;

    for (int i = 0; i < numVertices; i++) {
        pVert->position.x = inVerts->x;
        pVert->position.y = inVerts->y;

        pVert->color.r = byteToFloatTable[*(colors)];
        pVert->color.g = byteToFloatTable[*(colors + 1)];
        pVert->color.b = byteToFloatTable[*(colors + 2)];
        pVert->color.a = byteToFloatTable[*(colors + 3)];

        pVert->texCoord0.x = inVerts->tu1;
        pVert->texCoord0.y = inVerts->tv1;

        pVert->texCoord1.x = inVerts->tu2;
        pVert->texCoord1.y = inVerts->tv2;

        inVerts++;
        colors += 4;
        pVert++;
    }
}

- (MetalPipelineManager*) getPipelineManager
{
    return pipelineManager;
}

- (MetalShader*) getCurrentShader
{
    return currentShader;
}

- (void) setCurrentShader:(MetalShader*) shader
{
    currentShader = shader;
}

- (NSInteger) setDeviceParametersFor2D
{
    CTX_LOG(@"MetalContext_setDeviceParametersFor2D()");
    return 1;
}

- (NSInteger) setDeviceParametersFor3D
{
    CTX_LOG(@"MetalContext_setDeviceParametersFor3D()");
    if (clearDepthTexture) {
        CTX_LOG(@"MetalContext_setDeviceParametersFor3D clearDepthTexture is true");
        rttPassDesc.depthAttachment.clearDepth = 1.0;
        rttPassDesc.depthAttachment.loadAction = MTLLoadActionClear;
    } else {
        rttPassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
    }

    // TODO: MTL: Check whether we need to do shader initialization here
    /*if (!phongShader) {
        phongShader = ([[MetalPhongShader alloc] createPhongShader:self]);
    }*/
    return 1;
}

- (void) updateDepthDetails:(bool)depthTest
{
    CTX_LOG(@"MetalContext_updateDepthDetails");
    if (depthTest) {
        CTX_LOG(@"MetalContext_updateDepthDetails depthTest is true");
        if ([[self getRTT] isMSAAEnabled]) {
            CTX_LOG(@"MetalContext_updateDepthDetails MSAA");
            rttPassDesc.depthAttachment.storeAction = MTLStoreActionStoreAndMultisampleResolve;
            rttPassDesc.depthAttachment.texture = [rtt getDepthMSAATexture];
            rttPassDesc.depthAttachment.resolveTexture = [rtt getDepthTexture];
        } else {
            CTX_LOG(@"MetalContext_updateDepthDetails non-MSAA");
            rttPassDesc.depthAttachment.storeAction = MTLStoreActionStore;
            rttPassDesc.depthAttachment.texture = [[self getRTT] getDepthTexture];
            rttPassDesc.depthAttachment.resolveTexture = nil;
        }
    } else {
        CTX_LOG(@"MetalContext_updateDepthDetails depthTest is false");
        rttPassDesc.depthAttachment = nil;
    }
}

- (void) verifyDepthTexture
{
    CTX_LOG(@"MetalContext_verifyDepthTexture");
    id<MTLTexture> depthTexture = [rtt getDepthTexture];
    if (depthTexture == nil) {
        [rtt createDepthTexture];
        rttPassDesc.depthAttachment.clearDepth = 1.0;
        rttPassDesc.depthAttachment.loadAction = MTLLoadActionClear;
    }
}

- (void) setCompositeMode:(int) mode
{
    CTX_LOG(@"-> Native: MetalContext.setCompositeMode(): mode = %d", mode);
    compositeMode = mode;
}

- (int) getCompositeMode
{
    return compositeMode;
}

- (void) setCameraPosition:(float)x
            y:(float)y z:(float)z
{
    cPos.x = x;
    cPos.y = y;
    cPos.z = z;
    cPos.w = 0;
}

- (MTLRenderPassDescriptor*) getPhongRPD
{
    return phongRPD;
}

- (simd_float4x4) getMVPMatrix
{
    return mvpMatrix;
}

- (simd_float4x4) getWorldMatrix
{
    return worldMatrix;
}

- (vector_float4) getCameraPosition
{
    return cPos;
}

- (MTLScissorRect) getScissorRect
{
    if (!isScissorEnabled) {
        scissorRect.x = 0;
        scissorRect.y = 0;
        id<MTLTexture> currRtt = rttPassDesc.colorAttachments[0].texture;
        scissorRect.width  = currRtt.width;
        scissorRect.height = currRtt.height;
    }
    return scissorRect;
}

- (bool) isDepthEnabled
{
    return depthEnabled;
}

- (bool) isScissorEnabled
{
    return isScissorEnabled;
}

- (bool) isCurrentRTT:(MetalRTTexture*)rttPtr
{
    if (rttPtr == rtt) {
        return true;
    } else {
        return false;
    }
}

// TODO: MTL: This was copied from GlassHelper, and could be moved to a utility class.
+ (NSString*) nsStringWithJavaString:(jstring)javaString withEnv:(JNIEnv*)env
{
    NSString *string = @"";
    if (javaString != NULL) {
        const jchar* jstrChars = (*env)->GetStringChars(env, javaString, NULL);
        jsize size = (*env)->GetStringLength(env, javaString);
        if (size > 0) {
            string = [[[NSString alloc] initWithCharacters:jstrChars length:(NSUInteger)size] autorelease];
        }
        (*env)->ReleaseStringChars(env, javaString, jstrChars);
    }
    return string;
}

- (void)dealloc
{
    CTX_LOG(@">>>> MTLContext.dealloc -- releasing native MetalContext");

    if (commandQueue != nil) {
        [commandQueue release];
        commandQueue = nil;
    }

    if (pipelineManager != nil) {
        [pipelineManager release];
        pipelineManager = nil;
    }

    if (rttPassDesc != nil) {
        [rttPassDesc release];
        rttPassDesc = nil;
    }

    if (phongShader != nil) {
        [phongShader release];
        phongShader = nil;
    }

    if (phongRPD != nil) {
        [phongRPD release];
        phongRPD = nil;
    }

    if (argsRingBuffer != nil) {
        [argsRingBuffer dealloc];
        argsRingBuffer = nil;
    }

    if (dataRingBuffer != nil) {
        [dataRingBuffer dealloc];
        dataRingBuffer = nil;
    }

    for (NSNumber *keyWrapMode in linearSamplerDict) {
        [linearSamplerDict[keyWrapMode] release];
    }
    for (NSNumber *keyWrapMode in nonLinearSamplerDict) {
        [nonLinearSamplerDict[keyWrapMode] release];
    }
    [linearSamplerDict release];
    [nonLinearSamplerDict release];

    if (transientBuffersForCB != nil) {
        for (id buffer in transientBuffersForCB) {
            [buffer release];
        }
        [transientBuffersForCB removeAllObjects];
        [transientBuffersForCB release];
        transientBuffersForCB = nil;
    }

    if (shadersUsedInCB != nil) {
        [shadersUsedInCB removeAllObjects];
        [shadersUsedInCB release];
        shadersUsedInCB = nil;
    }
    if (identityMatrixBuf != nil) {
        [identityMatrixBuf release];
        identityMatrixBuf = nil;
    }

    if (clearEntireRttVerticesBuf != nil) {
        [clearEntireRttVerticesBuf release];
        clearEntireRttVerticesBuf = nil;
    }

    if (pixelBuffer != nil) {
        [pixelBuffer release];
        pixelBuffer = nil;
    }

    device = nil;

    [super dealloc];
}

- (id<MTLCommandQueue>) getCommandQueue
{
    return commandQueue;
}

@end // MetalContext


/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nInitialize
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nInitialize
  (JNIEnv *env, jclass jClass, jobject shaderLibBuffer)
{
    CTX_LOG(@">>>> MTLContext_nInitialize");
    jlong jContextPtr = 0L;

    // Create data object from direct byte buffer
    const void* dataPtr = (*env)->GetDirectBufferAddress(env, shaderLibBuffer);
    if (dataPtr == NULL) {
        NSLog(@"MTLContext_nInitialize: shaderLibBuffer addr = NULL");
        return 0L;
    }

    const jlong numBytes = (*env)->GetDirectBufferCapacity(env, shaderLibBuffer);
    if (numBytes <= 0) {
        NSLog(@"MTLContext_nInitialize: shaderLibBuffer invalid capacity");
        return 0L;
    }

    CTX_LOG(@"shaderLibBuffer :: addr: 0x%p, numBytes: %ld", dataPtr, numBytes);

    // We use a no-op destructor because the direct ByteBuffer is managed on the
    // Java side. We must not free it here.
    dispatch_data_t shaderLibData = dispatch_data_create(dataPtr, numBytes,
            DISPATCH_QUEUE_SERIAL,
            ^(void) {});

    if (shaderLibData == nil) {
        NSLog(@"MTLContext_nInitialize: Unable to create a dispatch_data object");
        return 0L;
    }

    jContextPtr = ptr_to_jlong([[MetalContext alloc] createContext:shaderLibData]);
    CTX_LOG(@"<<<< MTLContext_nInitialize");
    return jContextPtr;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nDisposeShader
  (JNIEnv *env, jclass jClass, jlong shaderRef)
{
    CTX_LOG(@">>>> MTLContext_nDisposeShader");

    MetalShader *shaderPtr = (MetalShader *)jlong_to_ptr(shaderRef);
    if (shaderPtr != nil) {
        [shaderPtr release];
        shaderPtr = nil;
    }
    CTX_LOG(@"<<<< MTLContext_nDisposeShader");
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nRelease
  (JNIEnv *env, jclass jClass, jlong context)
{
    CTX_LOG(@">>>> MTLContext_nRelease");

    MetalContext *contextPtr = (MetalContext *)jlong_to_ptr(context);

    if (contextPtr != NULL) {
        [contextPtr dealloc];
    }
    contextPtr = NULL;
    CTX_LOG(@"<<<< MTLContext_nRelease");
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nCommitCurrentCommandBuffer
  (JNIEnv *env, jclass jClass, jlong context)
{
    CTX_LOG(@">>>> MTLContext_nCommitCurrentCommandBuffer");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    [mtlContext commitCurrentCommandBuffer];
    CTX_LOG(@"<<<< MTLContext_nCommitCurrentCommandBuffer");
}

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nDrawIndexedQuads
  (JNIEnv *env, jclass jClass, jlong context, jfloatArray vertices, jbyteArray colors, jint numVertices)
{
    CTX_LOG(@"MTLContext_nDrawIndexedQuads");

    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);

    struct PrismSourceVertex *pVertices =
                    (struct PrismSourceVertex *) (*env)->GetPrimitiveArrayCritical(env, vertices, 0);
    char *pColors = (char *) (*env)->GetPrimitiveArrayCritical(env, colors, 0);

    [mtlContext drawIndexedQuads:pVertices ofColors:pColors vertexCount:numVertices];

    if (pColors) (*env)->ReleasePrimitiveArrayCritical(env, colors, pColors, 0);
    if (pVertices) (*env)->ReleasePrimitiveArrayCritical(env, vertices, pVertices, 0);

    return 1;
}

JNIEXPORT int JNICALL Java_com_sun_prism_mtl_MTLContext_nUpdateRenderTarget
  (JNIEnv *env, jclass jClass, jlong context, jlong texPtr, jboolean depthTest)
{
    CTX_LOG(@"MTLContext_nUpdateRenderTarget");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    MetalRTTexture *rtt = (MetalRTTexture *)jlong_to_ptr(texPtr);
    int ret = [mtlContext setRTT:rtt];
    // TODO: MTL: If we create depth texture while creating RTT
    // then also current implementation works fine. So in future
    // if we see any performance/state impact we should move
    // depthTexture creation along with RTT creation.
    if (depthTest) {
        [mtlContext verifyDepthTexture];
    }
    return ret;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetClipRect
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetClipRect
  (JNIEnv *env, jclass jClass, jlong ctx,
   jint x, jint y, jint width, jint height)
{
    CTX_LOG(@"MTLContext_nSetClipRect");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);
    [pCtx setClipRect:x y:y width:width height:height];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nResetClipRect
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nResetClipRect
  (JNIEnv *env, jclass jClass, jlong ctx)
{
    CTX_LOG(@"MTLContext_nResetClipRect");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);
    [pCtx resetClipRect];
}

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nResetTransform
  (JNIEnv *env, jclass jClass, jlong context)
{
    CTX_LOG(@"MTLContext_nResetTransform");

    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    //[mtlContext resetProjViewMatrix];
    return 1;
}

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nSetProjViewMatrix
  (JNIEnv *env, jclass jClass,
    jlong context, jboolean isOrtho,
    jdouble m00, jdouble m01, jdouble m02, jdouble m03,
    jdouble m10, jdouble m11, jdouble m12, jdouble m13,
    jdouble m20, jdouble m21, jdouble m22, jdouble m23,
    jdouble m30, jdouble m31, jdouble m32, jdouble m33)
{
    CTX_LOG(@"MTLContext_nSetProjViewMatrix");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);

    CTX_LOG(@"%f %f %f %f", m00, m01, m02, m03);
    CTX_LOG(@"%f %f %f %f", m10, m11, m12, m13);
    CTX_LOG(@"%f %f %f %f", m20, m21, m22, m23);
    CTX_LOG(@"%f %f %f %f", m30, m31, m32, m33);

    [mtlContext setProjViewMatrix:isOrtho
        m00:m00 m01:m01 m02:m02 m03:m03
        m10:m10 m11:m11 m12:m12 m13:m13
        m20:m20 m21:m21 m22:m22 m23:m23
        m30:m30 m31:m31 m32:m32 m33:m33];

    return 1;
}

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nSetTransform
  (JNIEnv *env, jclass jClass,
    jlong context,
    jdouble m00, jdouble m01, jdouble m02, jdouble m03,
    jdouble m10, jdouble m11, jdouble m12, jdouble m13,
    jdouble m20, jdouble m21, jdouble m22, jdouble m23,
    jdouble m30, jdouble m31, jdouble m32, jdouble m33)
{
    CTX_LOG(@"MTLContext_nSetTransform");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);

    CTX_LOG(@"%f %f %f %f", m00, m01, m02, m03);
    CTX_LOG(@"%f %f %f %f", m10, m11, m12, m13);
    CTX_LOG(@"%f %f %f %f", m20, m21, m22, m23);
    CTX_LOG(@"%f %f %f %f", m30, m31, m32, m33);

    // TODO: MTL: Added separate nSetTransform because previously
    // we used to use nSetProjViewMatrix only and enabled depth test
    // by default. Also check whether we need to do anything else
    // apart from just updating projection view matrix.

    [mtlContext setProjViewMatrix:m00
        m01:m01 m02:m02 m03:m03
        m10:m10 m11:m11 m12:m12 m13:m13
        m20:m20 m21:m21 m22:m22 m23:m23
        m30:m30 m31:m31 m32:m32 m33:m33];

    return 1;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetWorldTransform
  (JNIEnv *env, jclass jClass,
    jlong context,
    jdouble m00, jdouble m01, jdouble m02, jdouble m03,
    jdouble m10, jdouble m11, jdouble m12, jdouble m13,
    jdouble m20, jdouble m21, jdouble m22, jdouble m23,
    jdouble m30, jdouble m31, jdouble m32, jdouble m33)
{
    CTX_LOG(@"MTLContext_nSetWorldTransform");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);

    CTX_LOG(@"%f %f %f %f", m00, m01, m02, m03);
    CTX_LOG(@"%f %f %f %f", m10, m11, m12, m13);
    CTX_LOG(@"%f %f %f %f", m20, m21, m22, m23);
    CTX_LOG(@"%f %f %f %f", m30, m31, m32, m33);

    [mtlContext setWorldTransformMatrix:m00 m01:m01 m02:m02 m03:m03
        m10:m10 m11:m11 m12:m12 m13:m13
        m20:m20 m21:m21 m22:m22 m23:m23
        m30:m30 m31:m31 m32:m32 m33:m33];

    return;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetWorldTransformToIdentity
  (JNIEnv *env, jclass jClass,
    jlong context)
{
    CTX_LOG(@"MTLContext_nSetWorldTransformToIdentity");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);

    [mtlContext setWorldTransformIdentityMatrix];

    return;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nCreateMTLMesh
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nCreateMTLMesh
  (JNIEnv *env, jclass jClass, jlong ctx)
{
    CTX_LOG(@"MTLContext_nCreateMTLMesh");
    //return 1;
    MetalContext *pCtx = (MetalContext*) jlong_to_ptr(ctx);

    MetalMesh* mesh = ([[MetalMesh alloc] createMesh:pCtx]);
    return ptr_to_jlong(mesh);
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nReleaseMTLMesh
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nReleaseMTLMesh
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMesh)
{
    CTX_LOG(@"MTLContext_nReleaseMTLMesh");
    MetalMesh *mesh = (MetalMesh *) jlong_to_ptr(nativeMesh);
    if (mesh != nil) {
        [mesh release];
        mesh = nil;
    }
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nBuildNativeGeometryShort
 * Signature: (JJ[FI[SI)Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_prism_mtl_MTLContext_nBuildNativeGeometryShort
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMesh, jfloatArray vb, jint vbSize, jshortArray ib, jint ibSize)
{
    CTX_LOG(@"MTLContext_nBuildNativeGeometryShort");
    CTX_LOG(@"vbSize %d ibSize %d", vbSize, ibSize);
    MetalMesh *mesh = (MetalMesh *) jlong_to_ptr(nativeMesh);

    if (vbSize < 0 || ibSize < 0) {
        return JNI_FALSE;
    }

    unsigned int uvbSize = (unsigned int) vbSize;
    unsigned int uibSize = (unsigned int) ibSize;
    unsigned int vertexBufferSize = (*env)->GetArrayLength(env, vb);
    unsigned int indexBufferSize = (*env)->GetArrayLength(env, ib);
    CTX_LOG(@"vertexBufferSize %d indexBufferSize %d", vertexBufferSize, indexBufferSize);

    if (uvbSize > vertexBufferSize || uibSize > indexBufferSize) {
        return JNI_FALSE;
    }

    float *vertexBuffer = (float *) ((*env)->GetPrimitiveArrayCritical(env, vb, 0));
    if (vertexBuffer == NULL) {
        CTX_LOG(@"MTLContext_nBuildNativeGeometryShort vertexBuffer is NULL");
        return JNI_FALSE;
    }

    unsigned short *indexBuffer = (unsigned short *) ((*env)->GetPrimitiveArrayCritical(env, ib, 0));
    if (indexBuffer == NULL) {
        CTX_LOG(@"MTLContext_nBuildNativeGeometryShort indexBuffer is NULL");
        (*env)->ReleasePrimitiveArrayCritical(env, vb, vertexBuffer, 0);
        return JNI_FALSE;
    }

    bool result = [mesh buildBuffersShort:vertexBuffer
                                    vSize:uvbSize
                                  iBuffer:indexBuffer
                                    iSize:uibSize];
    (*env)->ReleasePrimitiveArrayCritical(env, ib, indexBuffer, 0);
    (*env)->ReleasePrimitiveArrayCritical(env, vb, vertexBuffer, 0);

    return result;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nBuildNativeGeometryInt
 * Signature: (JJ[FI[II)Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_prism_mtl_MTLContext_nBuildNativeGeometryInt
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMesh, jfloatArray vb, jint vbSize, jintArray ib, jint ibSize)
{
    CTX_LOG(@"MTLContext_nBuildNativeGeometryInt");
    CTX_LOG(@"vbSize %d ibSize %d", vbSize, ibSize);
    MetalMesh *mesh = (MetalMesh *) jlong_to_ptr(nativeMesh);

    if (vbSize < 0 || ibSize < 0) {
        return JNI_FALSE;
    }

    unsigned int uvbSize = (unsigned int) vbSize;
    unsigned int uibSize = (unsigned int) ibSize;
    unsigned int vertexBufferSize = (*env)->GetArrayLength(env, vb);
    unsigned int indexBufferSize = (*env)->GetArrayLength(env, ib);
    CTX_LOG(@"vertexBufferSize %d indexBufferSize %d", vertexBufferSize, indexBufferSize);

    if (uvbSize > vertexBufferSize || uibSize > indexBufferSize) {
        return JNI_FALSE;
    }

    float *vertexBuffer = (float *) ((*env)->GetPrimitiveArrayCritical(env, vb, 0));
    if (vertexBuffer == NULL) {
        CTX_LOG(@"MTLContext_nBuildNativeGeometryInt vertexBuffer is NULL");
        return JNI_FALSE;
    }

    unsigned int *indexBuffer = (unsigned int *) ((*env)->GetPrimitiveArrayCritical(env, ib, 0));
    if (indexBuffer == NULL) {
        CTX_LOG(@"MTLContext_nBuildNativeGeometryInt indexBuffer is NULL");
        (*env)->ReleasePrimitiveArrayCritical(env, vb, vertexBuffer, 0);
        return JNI_FALSE;
    }

    bool result = [mesh buildBuffersInt:vertexBuffer
                                  vSize:uvbSize
                                iBuffer:indexBuffer
                                  iSize:uibSize];
    (*env)->ReleasePrimitiveArrayCritical(env, ib, indexBuffer, 0);
    (*env)->ReleasePrimitiveArrayCritical(env, vb, vertexBuffer, 0);

    return result;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nCreateMTLPhongMaterial
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nCreateMTLPhongMaterial
  (JNIEnv *env, jclass jClass, jlong ctx)
{
    CTX_LOG(@"MTLContext_nCreateMTLPhongMaterial");
    MetalContext *pCtx = (MetalContext*) jlong_to_ptr(ctx);

    MetalPhongMaterial *phongMaterial = ([[MetalPhongMaterial alloc] createPhongMaterial:pCtx]);
    return ptr_to_jlong(phongMaterial);
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nReleaseMTLPhongMaterial
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nReleaseMTLPhongMaterial
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativePhongMaterial)
{
    CTX_LOG(@"MTLContext_nReleaseMTLPhongMaterial");
    MetalPhongMaterial *phongMaterial = (MetalPhongMaterial *) jlong_to_ptr(nativePhongMaterial);
    if (phongMaterial != nil) {
        [phongMaterial release];
        phongMaterial = nil;
    }
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetDiffuseColor
 * Signature: (JJFFFF)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetDiffuseColor
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativePhongMaterial,
        jfloat r, jfloat g, jfloat b, jfloat a)
{
    CTX_LOG(@"MTLContext_nSetDiffuseColor");
    MetalPhongMaterial *phongMaterial = (MetalPhongMaterial *) jlong_to_ptr(nativePhongMaterial);
    [phongMaterial setDiffuseColor:r g:g b:b a:a];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetSpecularColor
 * Signature: (JJZFFFF)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetSpecularColor
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativePhongMaterial,
        jboolean set, jfloat r, jfloat g, jfloat b, jfloat a)
{
    CTX_LOG(@"MTLContext_nSetSpecularColor");
    MetalPhongMaterial *phongMaterial = (MetalPhongMaterial *) jlong_to_ptr(nativePhongMaterial);
    bool specularSet = set ? true : false;
    [phongMaterial setSpecularColor:specularSet r:r g:g b:b a:a];
}
/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetMap
 * Signature: (JJIJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetMap
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativePhongMaterial,
        jint mapType, jlong nativeTexture)
{
    CTX_LOG(@"MTLContext_nSetMap");
    MetalPhongMaterial *phongMaterial = (MetalPhongMaterial *) jlong_to_ptr(nativePhongMaterial);
    MetalTexture *texMap = (MetalTexture *)  jlong_to_ptr(nativeTexture);

    [phongMaterial setMap:mapType map:[texMap getTexture]];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nCreateMTLMeshView
 * Signature: (JJ)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nCreateMTLMeshView
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMesh)
{
    CTX_LOG(@"MTLContext_nCreateMTLMeshView");
    MetalContext *pCtx = (MetalContext*) jlong_to_ptr(ctx);

    MetalMesh *pMesh = (MetalMesh *) jlong_to_ptr(nativeMesh);

    MetalMeshView* meshView = ([[MetalMeshView alloc] createMeshView:pCtx
                                                                mesh:pMesh]);
    return ptr_to_jlong(meshView);
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nReleaseMTLMeshView
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nReleaseMTLMeshView
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView)
{
    CTX_LOG(@"MTLContext_nReleaseMTLMeshView");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    if (meshView != nil) {
        [meshView release];
        meshView = nil;
    }
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetCullingMode
 * Signature: (JJI)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetCullingMode
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView, jint cullMode)
{
    CTX_LOG(@"MTLContext_nSetCullingMode");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);

    switch (cullMode) {
        case com_sun_prism_mtl_MTLContext_CULL_BACK:
            cullMode = MTLCullModeBack;
            break;
        case com_sun_prism_mtl_MTLContext_CULL_FRONT:
            cullMode = MTLCullModeFront;
            break;
        case com_sun_prism_mtl_MTLContext_CULL_NONE:
            cullMode = MTLCullModeNone;
            break;
    }
    [meshView setCullingMode:cullMode];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetMaterial
 * Signature: (JJJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetMaterial
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView, jlong nativePhongMaterial)
{
    CTX_LOG(@"MTLContext_nSetMaterial");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);

    MetalPhongMaterial *phongMaterial = (MetalPhongMaterial *) jlong_to_ptr(nativePhongMaterial);
    [meshView setMaterial:phongMaterial];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetWireframe
 * Signature: (JJZ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetWireframe
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView, jboolean wireframe)
{
    CTX_LOG(@"MTLContext_nSetWireframe");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    bool isWireFrame = wireframe ? true : false;
    [meshView setWireframe:isWireFrame];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetAmbientLight
 * Signature: (JJFFF)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetAmbientLight
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView,
        jfloat r, jfloat g, jfloat b)
{
    CTX_LOG(@"MTLContext_nSetAmbientLight");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    [meshView setAmbientLight:r g:g b:b];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetLight
 * Signature: (JJIFFFFFFFFFFFFFFFFFF)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetLight
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView, jint index,
        jfloat x, jfloat y, jfloat z, jfloat r, jfloat g, jfloat b, jfloat w,
        jfloat ca, jfloat la, jfloat qa, jfloat isAttenuated, jfloat range,
        jfloat dirX, jfloat dirY, jfloat dirZ, jfloat innerAngle, jfloat outerAngle, jfloat falloff)
{
    CTX_LOG(@"MTLContext_nSetLight");
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    [meshView setLight:index
        x:x y:y z:z
        r:r g:g b:b w:w
        ca:ca la:la qa:qa
        isA:isAttenuated range:range
        dirX:dirX dirY:dirY dirZ:dirZ
        inA:innerAngle outA:outerAngle
        falloff:falloff];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nRenderMeshView
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nRenderMeshView
  (JNIEnv *env, jclass jClass, jlong ctx, jlong nativeMeshView)
{
    CTX_LOG(@"MTLContext_nRenderMeshView");
    MetalContext *pCtx = (MetalContext*) jlong_to_ptr(ctx);
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    [pCtx renderMeshView:meshView];
    return;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nIsCurrentRTT
 * Signature: (JJ[[)Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_prism_mtl_MTLContext_nIsCurrentRTT
  (JNIEnv *env, jclass jClass, jlong ctx, jlong texPtr)
{
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);
    MetalRTTexture *rttPtr = (MetalRTTexture *)jlong_to_ptr(texPtr);
    return [pCtx isCurrentRTT:rttPtr];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nBlit
 * Signature: (JJJIIIIIIII)V
 */
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nBlit
  (JNIEnv *env, jclass jclass, jlong ctx, jlong nSrcRTT, jlong nDstRTT,
            jint srcX0, jint srcY0, jint srcX1, jint srcY1,
            jint dstX0, jint dstY0, jint dstX1, jint dstY1)
{
    CTX_LOG(@"MTLContext_nBlit");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);
    MetalRTTexture *srcRTT = (MetalRTTexture *)jlong_to_ptr(nSrcRTT);
    MetalRTTexture *dstRTT = (MetalRTTexture *)jlong_to_ptr(nDstRTT);

    id<MTLTexture> src = [srcRTT getTexture];
    id<MTLTexture> dst = [dstRTT getTexture];

    [pCtx endCurrentRenderEncoder];

    id<MTLCommandBuffer> commandBuffer = [pCtx getCurrentCommandBuffer];
    @autoreleasepool {
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        [blitEncoder synchronizeTexture:src slice:0 level:0];
        [blitEncoder synchronizeTexture:dst slice:0 level:0];
        [blitEncoder copyFromTexture:src
                sourceSlice:(NSUInteger)0
                sourceLevel:(NSUInteger)0
                sourceOrigin:MTLOriginMake(0, 0, 0)
                sourceSize:MTLSizeMake(src.width, src.height, src.depth)
                toTexture:dst
                destinationSlice:(NSUInteger)0
                destinationLevel:(NSUInteger)0
                destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blitEncoder endEncoding];
    }
    return;
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetCameraPosition
 */

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetCameraPosition
  (JNIEnv *env, jclass jClass, jlong ctx, jdouble x,
   jdouble y, jdouble z)
{
    CTX_LOG(@"MTLContext_nSetCameraPosition");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);

    return [pCtx setCameraPosition:x
            y:y z:z];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetDeviceParametersFor2D
 */

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nSetDeviceParametersFor2D
  (JNIEnv *env, jclass jClass, jlong ctx)
{
    CTX_LOG(@"MTLContext_nSetDeviceParametersFor2D");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);

    return [pCtx setDeviceParametersFor2D];
}

/*
 * Class:     com_sun_prism_mtl_MTLContext
 * Method:    nSetDeviceParametersFor3D
 */

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nSetDeviceParametersFor3D
  (JNIEnv *env, jclass jClass, jlong ctx)
{
    CTX_LOG(@"MTLContext_nSetDeviceParametersFor3D");
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);

    return [pCtx setDeviceParametersFor3D];
}

/*
* Class:     com_sun_prism_mtl_MTLContext
* Method:    nSetCompositeMode
*/
JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetCompositeMode(JNIEnv *env, jclass jClass, jlong context, jint mode)
{
    MetalContext* pCtx = (MetalContext*)jlong_to_ptr(context);
    [pCtx setCompositeMode:mode];
    return;
}

// TODO: MTL: This enables sharing of MTLCommandQueue between PRISM and GLASS, if needed.
// Note : Currently, PRISM and GLASS create their own dedicated MTLCommandQueue
// This method is unused
JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nGetCommandQueue
  (JNIEnv *env, jclass jClass, jlong context)
{
    CTX_LOG(@">>>> MTLContext_nGetCommandQueue");
    MetalContext *contextPtr = (MetalContext *)jlong_to_ptr(context);

    jlong jPtr = ptr_to_jlong((void *)[contextPtr getCommandQueue]);

    //NSLog(@"Prism - Metal context : commandQueue = %ld", jPtr);
    CTX_LOG(@"<<<< MTLContext_nGetCommandQueue");
    return jPtr;
}
