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
#import "MetalTexture.h"
#import "MetalPipelineManager.h"
#import "MetalRingBuffer.h"

@implementation MetalTexture

// This method creates a native MTLTexture
- (MetalTexture*) createTexture : (MetalContext*) ctx
             ofWidth : (NSUInteger) w
            ofHeight : (NSUInteger) h
            pixelFormat : (NSUInteger) format
            useMipMap   : (bool)useMipMap
{
    //return [self createTexture:ctx ofUsage:MTLTextureUsageShaderRead ofWidth:w ofHeight:h];

    TEX_LOG(@"\n");
    TEX_LOG(@">>>> MetalTexture.createTexture()  w = %lu, h= %lu", w, h);

    self = [super init];
    if (self) {
        width = w;
        height = h;
        context = ctx;
        usage = MTLTextureUsageShaderRead;
        usage = MTLTextureUsageUnknown; // TODO: MTL: - MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        pixelFormat = MTLPixelFormatBGRA8Unorm;
        if (format == 4) { // TODO: MTL: have proper format to pixelFormat mapping
            pixelFormat = MTLPixelFormatA8Unorm;
            TEX_LOG(@"Creating texture with native format MTLPixelFormatA8Unorm");
        }
        if (format == 7) {
            pixelFormat = MTLPixelFormatRGBA32Float;
            TEX_LOG(@"Creating texture with native format MTLPixelFormatRGBA32Float");
        }

        mipmapped = useMipMap;
        // We create 1x1 diffuse map when we have only diffuse
        // color for PhongMaterial, in such a case if generate mipmap
        // it causes assertion error at generateMipMap because
        // mipmapLevelCount will be 1, ignore generating mipmap for
        // texture 1x1
        if (useMipMap &&
            (width == 1 && height == 1)) {
            mipmapped = false;
        }
        TEX_LOG(@"useMipMap : %d", useMipMap);
        MTLTextureDescriptor *texDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                                                                 width:width
                                                                                                height:height
                                                                                             mipmapped:mipmapped];
        texDescriptor.usage = usage;

        // Create buffer to store pixel data and then a texture using that buffer
        id<MTLDevice> device = [context getDevice];
        texture = [device newTextureWithDescriptor:texDescriptor];

        /*
        // for testing purpose
        unsigned char img[10*10*4];
        for (int i = 0; i< 10; i++) {
            for (int j = 0; j < 10; j++) {
                  int index = j*10*4 + i*4;
                  img[index] = 0;
                  img[index+1] = 0;
                  img[index+2] = 255;
                  img[index+3] = 255;
              }
          }

        MTLRegion region = {{0,0,0}, {10, 10, 1}};

        [texture replaceRegion:region
                 mipmapLevel:0
                 withBytes:img
                 bytesPerRow: 10 * 4];
        */
    }
    TEX_LOG(@">>>> MetalTexture.createTexture()  width = %lu, height = %lu, format = %lu", width, height, format);
    TEX_LOG(@">>>> MetalTexture.createTexture()  created MetalTexture = %p", texture);
    return self;
}


// This method creates a native MTLTexture and corrresponding MTLBuffer
// Note : Currently this method is invoked with texUsage - MTLTextureUsageRenderTarget
// from method (MetalRTTexture*) createTexture
- (MetalTexture*) createTexture : (MetalContext*) ctx
                         ofUsage : (MTLTextureUsage) texUsage
                         ofWidth : (NSUInteger) w
                        ofHeight : (NSUInteger) h
                            msaa : (bool) msaa
{
    TEX_LOG(@"\n");
    TEX_LOG(@">>>> MetalTexture.createTexture()2  w = %lu, h= %lu", w, h);

    self = [super init];
    if (self) {
        width   = w;
        height  = h;
        context = ctx;
        usage   = texUsage;
        type    = MTLTextureType2D;
        pixelFormat = MTLPixelFormatBGRA8Unorm;
        storageMode = MTLResourceStorageModeShared;

        MTLTextureDescriptor *texDescriptor = [[MTLTextureDescriptor new] autorelease];
        texDescriptor.usage  = usage;
        texDescriptor.width  = width;
        texDescriptor.height = height;
        texDescriptor.textureType = type;
        texDescriptor.pixelFormat = pixelFormat;
        texDescriptor.sampleCount = 1;
        texDescriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;

        id<MTLDevice> device = [context getDevice];

        texture = [device newTextureWithDescriptor: texDescriptor];
        if (msaa) {
            TEX_LOG(@">>>> MetalTexture.createTexture()2 msaa texture");
            MTLTextureDescriptor *msaaTexDescriptor = [[MTLTextureDescriptor new] autorelease];
            msaaTexDescriptor.storageMode = MTLStorageModePrivate;
            msaaTexDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
            msaaTexDescriptor.width  = width;
            msaaTexDescriptor.height = height;
            msaaTexDescriptor.textureType = MTLTextureType2DMultisample;
            msaaTexDescriptor.pixelFormat = pixelFormat;
            //By default all SoC's on macOS support 4 sample count
            msaaTexDescriptor.sampleCount = 4;
            msaaTexture = [device newTextureWithDescriptor: msaaTexDescriptor];
        } else {
            msaaTexture = nil;
        }
        isMSAA = msaa;

        depthTexture = nil;
        depthMSAATexture = nil;
    }
    TEX_LOG(@">>>> MetalTexture.createTexture()2  (buffer backed texture) -- width = %lu, height = %lu", width, height);
    TEX_LOG(@">>>> MetalTexture.createTexture()2  created MetalTexture = %p", texture);
    return self;
}

- (MetalTexture*) createTexture : (MetalContext*) ctx
                         mtlTex : (long) pTex
                         ofWidth : (NSUInteger) w
                        ofHeight : (NSUInteger) h
{
    width   = w;
    height  = h;
    context = ctx;
    // usage   = texUsage;
    // type    = MTLTextureType2D;
    // pixelFormat = MTLPixelFormatBGRA8Unorm;
    // storageMode = MTLResourceStorageModeShared;

    id <MTLTexture> tex = (__bridge id<MTLTexture>)(jlong_to_ptr(pTex));

    //NSLog(@"Prism ----- tex = %@", tex);

    texture = tex;
    return self;
}

- (void) createDepthTexture
{
    TEX_LOG(@">>>> MetalTexture.createDepthTexture()");
    id<MTLDevice> device = [context getDevice];
    if (depthTexture.width != width ||
        depthTexture.height != height ||
        lastDepthMSAA != isMSAA) {
        lastDepthMSAA = isMSAA;
        MTLTextureDescriptor *depthDesc = [[MTLTextureDescriptor new] autorelease];
        depthDesc.width  = width;
        depthDesc.height = height;
        depthDesc.pixelFormat = MTLPixelFormatDepth32Float;
        depthDesc.textureType = MTLTextureType2D;
        depthDesc.sampleCount = 1;
        depthDesc.usage = MTLTextureUsageRenderTarget;
        depthDesc.storageMode = MTLStorageModePrivate;
        depthTexture = [device newTextureWithDescriptor: depthDesc];
        if (isMSAA) {
            TEX_LOG(@">>>> MetalTexture.createDepthMSAATexture()");
            depthDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
            depthDesc.textureType = MTLTextureType2DMultisample;
            //By default all SoC's on macOS support 4 sample count
            depthDesc.sampleCount = 4;
            depthMSAATexture = [device newTextureWithDescriptor: depthDesc];
        }
    }
}

- (id<MTLBuffer>) getPixelBuffer
{
    TEX_LOG(@">>>> MetalTexture.getPixelBuffer()");

    [context endCurrentRenderEncoder];

    id<MTLCommandBuffer> commandBuffer = [context getCurrentCommandBuffer];
    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

    [blitEncoder synchronizeTexture:texture slice:0 level:0];
    [blitEncoder copyFromTexture:texture
                sourceSlice:(NSUInteger)0
                sourceLevel:(NSUInteger)0
               sourceOrigin:MTLOriginMake(0, 0, 0)
                sourceSize:MTLSizeMake(texture.width, texture.height, texture.depth)
                toBuffer:[context getPixelBuffer]
            destinationOffset:(NSUInteger)0
            destinationBytesPerRow:(NSUInteger)texture.width * 4
            destinationBytesPerImage:(NSUInteger)texture.width * texture.height * 4];

    [blitEncoder endEncoding];
    [context commitCurrentCommandBufferAndWait];

    return [context getPixelBuffer];
}

- (id<MTLTexture>) getTexture
{
    return texture;
}

- (id<MTLTexture>) getDepthTexture
{
    return depthTexture;
}

- (id<MTLTexture>) getDepthMSAATexture
{
    return depthMSAATexture;
}

- (id<MTLTexture>) getMSAATexture
{
    return msaaTexture;
}

- (bool) isMSAAEnabled
{
    return isMSAA;
}

- (bool) isMipmapped
{
    return mipmapped;
}

- (void)dealloc
{
    if (texture != nil) {
        TEX_LOG(@">>>> MetalTexture.dealloc -- releasing native MTLTexture");
        [texture release];
        texture = nil;
    }

    if (depthTexture != nil) {
        TEX_LOG(@">>>> MetalTexture.dealloc -- releasing depthTexture");
        [depthTexture release];
        depthTexture = nil;
    }

    if (depthMSAATexture != nil) {
        TEX_LOG(@">>>> MetalTexture.dealloc -- releasing depthMSAATexture");
        [depthMSAATexture release];
        depthMSAATexture = nil;
    }

    if (msaaTexture != nil) {
        TEX_LOG(@">>>> MetalTexture.dealloc -- releasing msaaTexture");
        [msaaTexture release];
        msaaTexture = nil;
    }

    [super dealloc];
}


@end // MetalTexture

static int copyPixelDataToRingBuffer(MetalContext* context, void* pixels, unsigned int length)
{
    int offset = [[MetalRingBuffer getInstance] reserveBytes:length];
    if (offset == -2) {
        return offset;
    }
    if (offset == -1) {
        [context commitCurrentCommandBuffer];
        offset = [[MetalRingBuffer getInstance] reserveBytes:length];
    }
    memcpy([[MetalRingBuffer getInstance] getBuffer].contents + offset, pixels, length);
    return offset;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLTexture_nUpdate
(JNIEnv *env, jclass jClass, jlong ctx, jlong nTexturePtr, jbyteArray pixData, jint dstx, jint dsty, jint srcx, jint srcy, jint w, jint h, jint scanStride) {
    TEX_LOG(@"\n");
    TEX_LOG(@"-> Native: MTLTexture_nUpdate srcx: %d, srcy: %d, width: %d, height: %d --- scanStride = %d", srcx, srcy, w, h, scanStride);
    MetalContext* context = (MetalContext*)jlong_to_ptr(ctx);
    MetalTexture* mtlTex  = (MetalTexture*)jlong_to_ptr(nTexturePtr);

    id<MTLTexture> tex = [mtlTex getTexture];
    jbyte *pixels = (*env)->GetByteArrayElements(env, pixData, 0);

    jsize length = (*env)->GetArrayLength(env, pixData);
    if (length == 0) {
        (*env)->ReleaseByteArrayElements(env, pixData, pixels, 0);
        return 0;
    }

    id<MTLBuffer> pixelMTLBuf = nil;
    int offset = copyPixelDataToRingBuffer(context, pixels, length);
    if (offset == -2) {
        TEX_LOG(@"MetalTexture_nUpdate -- creating non Ring Buffer");
        pixelMTLBuf = [context getTransientBufferWithBytes:pixels length:length];
        offset = 0;
    } else {
        pixelMTLBuf = [[MetalRingBuffer getInstance] getBuffer];
    }

    (*env)->ReleaseByteArrayElements(env, pixData, pixels, 0);

    [context endCurrentRenderEncoder];

    id<MTLBlitCommandEncoder> blitEncoder = [[context getCurrentCommandBuffer] blitCommandEncoder];

    [blitEncoder synchronizeTexture:tex slice:0 level:0];
    [blitEncoder copyFromBuffer:pixelMTLBuf
                   sourceOffset:(NSUInteger)offset
              sourceBytesPerRow:(NSUInteger)scanStride
            sourceBytesPerImage:(NSUInteger)0 // 0 for 2D image
                     sourceSize:MTLSizeMake(w, h, 1)
                      toTexture:tex
               destinationSlice:(NSUInteger)0
               destinationLevel:(NSUInteger)0
              destinationOrigin:MTLOriginMake(dstx, dsty, 0)];

    if ([mtlTex isMipmapped]) {
        [blitEncoder generateMipmapsForTexture:tex];
    }

    [blitEncoder endEncoding];

    // TODO: MTL: add error detection and return appropriate jlong
    return 0;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLTexture_nUpdateFloat
(JNIEnv *env, jclass jClass, jlong ctx, jlong nTexturePtr, jfloatArray pixData, jint dstx, jint dsty, jint srcx, jint srcy, jint w, jint h, jint scanStride) {
    TEX_LOG(@"\n");
    TEX_LOG(@"-> Native: MTLTexture_nUpdateFloat srcx: %d, srcy: %d, width: %d, height: %d --- scanStride = %d", srcx, srcy, w, h, scanStride);
    MetalContext* context = (MetalContext*)jlong_to_ptr(ctx);
    MetalTexture* mtlTex  = (MetalTexture*)jlong_to_ptr(nTexturePtr);

    id<MTLTexture> tex = [mtlTex getTexture];
    jfloat *pixels = (*env)->GetFloatArrayElements(env, pixData, 0);

    jsize length = (*env)->GetArrayLength(env, pixData);
    if (length == 0) {
        (*env)->ReleaseFloatArrayElements(env, pixData, pixels, 0);
        return 0;
    }

    id<MTLBuffer> pixelMTLBuf = nil;
    int offset = copyPixelDataToRingBuffer(context, pixels, length * sizeof(float));
    if (offset == -2) {
        TEX_LOG(@"MetalTexture_nUpdateFloat -- creating non Ring Buffer");
        pixelMTLBuf = [context getTransientBufferWithBytes:pixels length:length * sizeof(float)];
        offset = 0;
    } else {
        pixelMTLBuf = [[MetalRingBuffer getInstance] getBuffer];
    }

    (*env)->ReleaseFloatArrayElements(env, pixData, pixels, 0);

    [context endCurrentRenderEncoder];

    id<MTLBlitCommandEncoder> blitEncoder = [[context getCurrentCommandBuffer] blitCommandEncoder];

    [blitEncoder synchronizeTexture:tex slice:0 level:0];
    [blitEncoder copyFromBuffer:pixelMTLBuf
                   sourceOffset:(NSUInteger)offset
              sourceBytesPerRow:(NSUInteger)scanStride
            sourceBytesPerImage:(NSUInteger)0 // 0 for 2D image
                     sourceSize:MTLSizeMake(w, h, 1)
                      toTexture:tex
               destinationSlice:(NSUInteger)0
               destinationLevel:(NSUInteger)0
              destinationOrigin:MTLOriginMake(dstx, dsty, 0)];

    if ([mtlTex isMipmapped]) {
        [blitEncoder generateMipmapsForTexture:tex];
    }

    [blitEncoder endEncoding];

    // TODO: MTL: add error detection and return appropriate jlong
    return 0;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLTexture_nUpdateInt
(JNIEnv *env, jclass jClass, jlong ctx, jlong nTexturePtr, jintArray pixData, jint dstx, jint dsty, jint srcx, jint srcy, jint w, jint h, jint scanStride) {
    TEX_LOG(@"\n");
    TEX_LOG(@"-> Native: MTLTexture_nUpdateInt srcx: %d, srcy: %d, width: %d, height: %d --- scanStride = %d", srcx, srcy, w, h, scanStride);
    MetalContext* context = (MetalContext*)jlong_to_ptr(ctx);
    MetalTexture* mtlTex  = (MetalTexture*)jlong_to_ptr(nTexturePtr);

    id<MTLTexture> tex = [mtlTex getTexture];
    jint *pixels = (*env)->GetIntArrayElements(env, pixData, 0);

    jsize length = (*env)->GetArrayLength(env, pixData);
    if (length == 0) {
        (*env)->ReleaseIntArrayElements(env, pixData, pixels, 0);
        return 0;
    }

    id<MTLBuffer> pixelMTLBuf = nil;
    int offset = copyPixelDataToRingBuffer(context, pixels, length * sizeof(int));
    if (offset == -2) {
        TEX_LOG(@"MetalTexture_nUpdateInt -- creating non Ring Buffer");
        pixelMTLBuf = [context getTransientBufferWithBytes:pixels length:length * sizeof(int)];
        offset = 0;
    } else {
        pixelMTLBuf = [[MetalRingBuffer getInstance] getBuffer];
    }

    (*env)->ReleaseIntArrayElements(env, pixData, pixels, 0);

    [context endCurrentRenderEncoder];

    id<MTLBlitCommandEncoder> blitEncoder = [[context getCurrentCommandBuffer] blitCommandEncoder];

    [blitEncoder synchronizeTexture:tex slice:0 level:0];
    [blitEncoder copyFromBuffer:pixelMTLBuf
                   sourceOffset:(NSUInteger)offset
              sourceBytesPerRow:(NSUInteger)scanStride
            sourceBytesPerImage:(NSUInteger)0 // 0 for 2D image
                     sourceSize:MTLSizeMake(w, h, 1)
                      toTexture:tex
               destinationSlice:(NSUInteger)0
               destinationLevel:(NSUInteger)0
              destinationOrigin:MTLOriginMake(dstx, dsty, 0)];

    if ([mtlTex isMipmapped]) {
        [blitEncoder generateMipmapsForTexture:tex];
    }

    [blitEncoder endEncoding];

    // TODO: MTL: add error detection and return appropriate jlong
    return 0;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLTexture_nUpdateYUV422
(JNIEnv *env, jclass jClass, jlong ctx, jlong nTexturePtr, jbyteArray pixData, jint dstx, jint dsty, jint srcx, jint srcy, jint w, jint h, jint scanStride) {
    TEX_LOG(@"\n");
    TEX_LOG(@"-> Native: MTLTexture_nUpdateYUV422 srcx: %d, srcy: %d, width: %d, height: %d --- scanStride = %d", srcx, srcy, w, h, scanStride);
    MetalContext* context = (MetalContext*)jlong_to_ptr(ctx);
    MetalTexture* mtlTex  = (MetalTexture*)jlong_to_ptr(nTexturePtr);

    id<MTLTexture> tex = [mtlTex getTexture];
    jbyte* pixels = (*env)->GetByteArrayElements(env, pixData, 0);
    jbyte* p = pixels;

    @autoreleasepool {

        id<MTLDevice> device = [context getDevice];

        id<MTLBuffer> srcBuff = [[device newBufferWithLength: (w * h * 2) options: MTLResourceStorageModeManaged] autorelease];
        for (int row = 0; row < h; row++) {
            // Copy each row in srcBuff
            memcpy(srcBuff.contents + (row * w * 2),
                   (char*) pixels, w*2);

            pixels += (w * 2);
            pixels += scanStride - (w*2);
        }

        [srcBuff didModifyRange:NSMakeRange(0, srcBuff.length)];

        [context endCurrentRenderEncoder];

        MTLSize _threadgroupSize = MTLSizeMake(2, 1, 1);

        MTLSize _threadgroupCount;
        _threadgroupCount.width  = w / _threadgroupSize.width;
        _threadgroupCount.height = h / _threadgroupSize.height;
        _threadgroupCount.depth = 1;

        id<MTLComputePipelineState> _computePipelineState = [[context getPipelineManager] getComputePipelineStateWithFunc:@"uyvy422_to_rgba"];

        id<MTLCommandBuffer> commandBuffer = [context getCurrentCommandBuffer];

        id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];

        [computeEncoder setComputePipelineState:_computePipelineState];

        [computeEncoder setBuffer:srcBuff
                            offset:0
                           atIndex:0];

        [computeEncoder setTexture:tex
                           atIndex:0];

        [computeEncoder dispatchThreadgroups:_threadgroupCount
                       threadsPerThreadgroup:_threadgroupSize];

        [computeEncoder endEncoding];

        [context commitCurrentCommandBuffer];
    }

    pixels = p;

    (*env)->ReleaseByteArrayElements(env, pixData, pixels, 0);

    // TODO: MTL: add error detection and return appropriate jlong
    return 0;
}
