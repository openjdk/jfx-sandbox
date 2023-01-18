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
#import "MetalResourceFactory.h"
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

#define MAX_QUADS_IN_A_BATCH 14
@implementation MetalContext

- (id) createContext:(NSString*)shaderLibPath
{
    CTX_LOG(@"-> MetalContext.createContext()");
    self = [super init];
    if (self) {
        // RTT must be cleared before drawing into it for the first time,
        // after drawing first time the loadAction shall be set to MTLLoadActionLoad
        // so that it becomes a stable rtt.
        // loadAction is set to MTLLoadActionLoad in [resetRenderPass]
        rttLoadAction = MTLLoadActionClear;

        device = MTLCreateSystemDefaultDevice();
        commandQueue = [device newCommandQueue];
        commandQueue.label = @"The only MTLCommandQueue";
        pipelineManager = [MetalPipelineManager alloc];
        [pipelineManager init:self libPath:shaderLibPath];

        rttPassDesc = [MTLRenderPassDescriptor new];
        rttPassDesc.colorAttachments[0].loadAction = rttLoadAction;
        rttPassDesc.colorAttachments[0].clearColor = MTLClearColorMake(1, 1, 1, 1); // make this programmable
        rttPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    }
    return self;
}

- (void) setRTT:(MetalRTTexture*)rttPtr
{
    CTX_LOG(@"-> Native: MetalContext.setRTT()");
    rtt = rttPtr;
    rttPassDesc.colorAttachments[0].texture = [rtt getTexture];
}

- (MetalRTTexture*) getRTT
{
    CTX_LOG(@"-> Native: MetalContext.getRTT()");
    return rtt;
}


- (void) setTex0:(MetalTexture*)texPtr
{
    CTX_LOG(@"-> Native: MetalContext.setTex0() ----- %p", texPtr);
    tex0 = texPtr;
}

- (MetalTexture*) getTex0
{
    CTX_LOG(@"-> Native: MetalContext.getTex0()");
    return tex0;
}


- (id<MTLDevice>) getDevice
{
    // CTX_LOG(@"MetalContext.getDevice()");
    return device;
}

- (id<MTLCommandBuffer>) newCommandBuffer
{
    CTX_LOG(@"MetalContext.newCommandBuffer()");
    currentCommandBuffer = [self newCommandBuffer:@"Command Buffer"];
    return currentCommandBuffer;
}

- (id<MTLCommandBuffer>) newCommandBuffer:(NSString*)label
{
    CTX_LOG(@"MetalContext.newCommandBufferWithLabel()");
    currentCommandBuffer = [commandQueue commandBuffer];
    currentCommandBuffer.label = label;
    [currentCommandBuffer addScheduledHandler:^(id<MTLCommandBuffer> cb) {
         CTX_LOG(@"------------------> Native: commandBuffer Scheduled");
    }];
    [currentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
         currentCommandBuffer = nil;
         CTX_LOG(@"------------------> Native: commandBuffer Completed");
    }];
    return currentCommandBuffer;
}

- (id<MTLCommandBuffer>) getCurrentCommandBuffer
{
    CTX_LOG(@"MetalContext.getCurrentCommandBuffer() --- current value = %p", currentCommandBuffer);
    if (currentCommandBuffer == nil) {
        return [self newCommandBuffer];
    }
    return currentCommandBuffer;
}

- (MetalResourceFactory*) getResourceFactory
{
    CTX_LOG(@"MetalContext.getResourceFactory()");
    return resourceFactory;
}

- (NSInteger) drawIndexedQuads : (struct PrismSourceVertex const *)pSrcFloats
                      ofColors : (char const *)pSrcColors
                   vertexCount : (NSUInteger)numVerts
{
    CTX_LOG(@"MetalContext.drawIndexedQuads()");

    CTX_LOG(@"numVerts = %d", numVerts);

    id<MTLCommandBuffer> commandBuffer = [self getCurrentCommandBuffer];

    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:rttPassDesc];

    if (currentPipeState != nil) {
        [renderEncoder setRenderPipelineState:currentPipeState];
    } else {
        id<MTLRenderPipelineState> pipeline = [pipelineManager getPipeStateWithFragFuncName:@"Solid_Color"];
        [renderEncoder setRenderPipelineState:pipeline];
    }

    [renderEncoder setVertexBytes:&mvpMatrix
                               length:sizeof(mvpMatrix)
                              atIndex:VertexInputMatrixMVP];

    [renderEncoder setFragmentBuffer:currentFragArgBuffer
                              offset:0
                             atIndex:0];

    if (tex0 != nil) {
        id<MTLTexture> tex = [tex0 getTexture];
        [currentShader setTexture: @"inputTex" texture:tex];
        [renderEncoder useResource:tex usage:MTLResourceUsageRead];
    }

    int numQuads = numVerts/4;

    // size of VS_INPUT is 48 bytes
    // We can use setVertexBytes() method to pass a vertext buffer of max size 4KB
    // 4096 / 48 = 85 vertices at a time.

    // No of quads when represneted as 2 triangles of 3 vertices each = 85/6 = 14
    // We can issue 14 quads draw from a single vertex buffer batch
    // 14 quads ==> 14 * 4 vertices = 56 vertices

    // FillVB methods fills 84 vertices in vertex batch from 56 given vertices
    // Send 56 vertices at max in each iteration

    for (int i = 0; i < numQuads; i += MAX_QUADS_IN_A_BATCH) {

        int quads = MAX_QUADS_IN_A_BATCH;
        if ((i + MAX_QUADS_IN_A_BATCH) > numQuads) {
            quads = numQuads - i;
        }
        CTX_LOG(@"Quads in this iteration =========== %d", quads);

        [self fillVB:pSrcFloats + (i * 4)
              colors:pSrcColors + (i * 4 * 4)
              numVertices:quads * 4];

        [renderEncoder setVertexBytes:vertices
                               length:sizeof(vertices)
                              atIndex:VertexInputIndexVertices];

        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:quads * 2 * 3];
    }

    [renderEncoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    [self resetRenderPass];

    return 1;
}

- (void) setProjViewMatrix:(bool)isOrtho
        m00:(float)m00 m01:(float)m01 m02:(float)m02 m03:(float)m03
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

- (void) setWorldTransformIdentityMatrix:(float)m00
        m01:(float)m01 m02:(float)m02 m03:(float)m03
        m10:(float)m10 m11:(float)m11 m12:(float)m12 m13:(float)m13
        m20:(float)m20 m21:(float)m21 m22:(float)m22 m23:(float)m23
        m30:(float)m30 m31:(float)m31 m32:(float)m32 m33:(float)m33
{
    CTX_LOG(@"MetalContext.setWorldTransformIdentityMatrix()");
    worldMatrix = simd_matrix(
        (simd_float4){ m00, m01, m02, m03 },
        (simd_float4){ m10, m11, m12, m13 },
        (simd_float4){ m20, m21, m22, m23 },
        (simd_float4){ m30, m31, m32, m33 }
    );
}

- (void) renderMeshView:(MetalMeshView*)meshView
{
    // TODO: MTL: Move creation of MTLRenderPassDescriptor to commom class
    // like MetalMeshView
    id<MTLCommandBuffer> commandBuffer = [self getCurrentCommandBuffer];
    MTLRenderPassDescriptor* phongRPD = [MTLRenderPassDescriptor new];
    phongRPD.colorAttachments[0].loadAction = MTLLoadActionClear;
    phongRPD.colorAttachments[0].clearColor = MTLClearColorMake(1, 1, 1, 1); // make this programmable
    phongRPD.colorAttachments[0].storeAction = MTLStoreActionStore;
    phongRPD.colorAttachments[0].texture = [[self getRTT] getTexture];

    id<MTLRenderCommandEncoder> phongEncoder = [commandBuffer renderCommandEncoderWithDescriptor:phongRPD];
    id<MTLRenderPipelineState> phongPipelineState =
        [[self getPipelineManager] getPhongPipeStateWithFragFuncName:@"PhongPS"];
    [phongEncoder setRenderPipelineState:phongPipelineState];
    // In Metal default winding order is Clockwise but the vertex data that
    // we are getting is in CounterClockWise order, so we need to set
    // MTLWindingCounterClockwise explicitly
    [phongEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [phongEncoder setCullMode:[meshView getCullingMode]];
    [phongEncoder setVertexBytes:&mvpMatrix
                               length:sizeof(mvpMatrix)
                              atIndex:1];
    [phongEncoder setVertexBytes:&worldMatrix
                               length:sizeof(worldMatrix)
                              atIndex:2];
    MetalMesh* mesh = [meshView getMesh];
    id<MTLBuffer> vBuffer = [mesh getVertexBuffer];
    [phongEncoder setVertexBuffer:vBuffer
                           offset:0
                            atIndex:0];
    [phongEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
        indexCount:[mesh getNumIndices]
        indexType:MTLIndexTypeUInt16
        indexBuffer:[mesh getIndexBuffer]
        indexBufferOffset:0];
    [phongEncoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    // TODO: MTL: Check whether we need to resetRenderPass
}

- (void) resetRenderPass
{
    CTX_LOG(@"MetalContext.resetRenderPass()");
    rttPassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
}

- (void) setRTTLoadActionToClear
{
    CTX_LOG(@"MetalContext.clearRTT()");
    rttPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
}

- (void) resetProjViewMatrix
{
    CTX_LOG(@"MetalContext.resetProjViewMatrix()");
    mvpMatrix = simd_matrix(
        (simd_float4){1, 0, 0, 0},
        (simd_float4){0, 1, 0, 0},
        (simd_float4){0, 0, 1, 0},
        (simd_float4){0, 0, 0, 1}
    );
}

- (void) fillVB:(struct PrismSourceVertex const *)pSrcFloats colors:(char const *)pSrcColors
                 numVertices:(int)numVerts
{
    VS_INPUT* pVert = vertices;
    numTriangles = numVerts / 2;
    int numQuads = numTriangles / 2;


    CTX_LOG(@"fillVB : numVerts = %d, numTriangles = %d, numQuads = %d", numVerts, numTriangles, numQuads);

    for (int i = 0; i < numQuads; i++) {
        unsigned char const* colors = (unsigned char*)(pSrcColors + i * 4 * 4);
        struct PrismSourceVertex const * inVerts = pSrcFloats + i * 4;
        for (int k = 0; k < 2; k++) {
            for (int j = 0; j < 3; j++) {
                pVert->position.x = inVerts->x;
                pVert->position.y = inVerts->y;

                pVert->color.x = ((float)(*(colors)))/255.0f;
                pVert->color.y = ((float)(*(colors + 1)))/255.0f;
                pVert->color.z = ((float)(*(colors + 2)))/255.0f;
                pVert->color.w = ((float)(*(colors + 3)))/255.0f;

                pVert->texCoord0.x = inVerts->tu1;
                pVert->texCoord0.y = inVerts->tv1;

                pVert->texCoord1.x = inVerts->tu2;
                pVert->texCoord1.y = inVerts->tv2;

                inVerts++;
                colors += 4;
                pVert++;
            }
            inVerts -= 2;
            colors -= 4; colors -= 4;
        }
    }
}

- (MetalPipelineManager*) getPipelineManager
{
    return pipelineManager;
}

- (void) setCurrentPipeState:(id<MTLRenderPipelineState>) pipeState
{
    currentPipeState = pipeState;
}

- (void) setCurrentArgumentBuffer:(id<MTLBuffer>) argBuffer
{
    currentFragArgBuffer = argBuffer;
}

- (MetalShader*) getCurrentShader
{
    return currentShader;
}

- (void) setCurrentShader:(MetalShader*) shader
{
    currentShader = shader;
}

- (NSInteger) setDeviceParametersFor3D
{
    // TODO: MTL: Check whether we can do RenderPassDescriptor
    // initialization in this call
    CTX_LOG(@"MetalContext_setDeviceParametersFor3D()");

    if (!phongShader) {
        phongShader = ([[MetalPhongShader alloc] createPhongShader:self]);
    }
    return 1;
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

@end // MetalContext


JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLContext_nInitialize
  (JNIEnv *env, jclass jClass, jstring shaderLibPathStr)
{
    CTX_LOG(@">>>> MTLContext_nInitialize");
    jlong jContextPtr = 0L;
    NSString* shaderLibPath = [MetalContext nsStringWithJavaString:shaderLibPathStr withEnv:env];
    CTX_LOG(@"----> shaderLibPath: %@", shaderLibPath);
    jContextPtr = ptr_to_jlong([[MetalContext alloc] createContext:shaderLibPath]);
    CTX_LOG(@"<<<< MTLContext_nInitialize");
    return jContextPtr;
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

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nUpdateRenderTarget
  (JNIEnv *env, jclass jClass, jlong context, jlong texPtr)
{
    CTX_LOG(@"MTLContext_nUpdateRenderTarget");
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    MetalRTTexture *rtt = (MetalRTTexture *)jlong_to_ptr(texPtr);
    [mtlContext setRTT:rtt];
}

JNIEXPORT jint JNICALL Java_com_sun_prism_mtl_MTLContext_nResetTransform
  (JNIEnv *env, jclass jClass, jlong context)
{
    CTX_LOG(@"MTLContext_nResetTransform");

    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    //[mtlContext resetProjViewMatrix];
    return 1;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLContext_nSetTex0
  (JNIEnv *env, jclass jClass, jlong context, jlong texPtr)
{
    CTX_LOG(@"MTLContext_nSetTex0 : texPtr = %ld", texPtr);
    MetalContext *mtlContext = (MetalContext *)jlong_to_ptr(context);
    MetalTexture *tex = (MetalTexture *)jlong_to_ptr(texPtr);
    [mtlContext setTex0:tex];
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

    [mtlContext setWorldTransformIdentityMatrix:1 m01:0 m02:0 m03:0
        m10:0 m11:1 m12:0 m13:0
        m20:0 m21:0 m22:1 m23:0
        m30:0 m31:0 m32:0 m33:1];

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
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nReleaseMTLMesh");
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

    bool result = [mesh buildBuffers:vertexBuffer
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
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nBuildNativeGeometryInt");
    return JNI_TRUE;
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
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nReleaseMTLPhongMaterial");
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
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nSetMap");
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
    // TODO: MTL: Complete the implementation
    CTX_LOG(@"MTLContext_nReleaseMTLMeshView");
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
    MetalContext *pCtx = (MetalContext*)jlong_to_ptr(ctx);
    MetalMeshView *meshView = (MetalMeshView *) jlong_to_ptr(nativeMeshView);
    [pCtx renderMeshView:meshView];
    return;
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
    MetalContext* mtlCtx = (MetalContext*)jlong_to_ptr(context);

    MetalPipelineManager* pipeLineMgr = [mtlCtx getPipelineManager];

    [pipeLineMgr setCompositeBlendMode:mode];

    return;
}
