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

@implementation MetalTexture

// This method creates a native MTLTexture
- (MetalTexture*) createTexture : (MetalContext*) ctx
             ofWidth : (NSUInteger) w
            ofHeight : (NSUInteger) h
            pixelFormat : (NSUInteger) format
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
        }
        type = MTLTextureType2D;
        storageMode = MTLResourceStorageModeShared;

        texDescriptor = [MTLTextureDescriptor new];
        texDescriptor.textureType = type;
        texDescriptor.width = width;
        texDescriptor.height = height;
        texDescriptor.pixelFormat = pixelFormat;
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

        texDescriptor = [MTLTextureDescriptor new];
        texDescriptor.usage  = usage;
        texDescriptor.width  = width;
        texDescriptor.height = height;
        texDescriptor.textureType = type;
        texDescriptor.pixelFormat = pixelFormat;

        id<MTLDevice> device = [context getDevice];

        texture = [device newTextureWithDescriptor: texDescriptor];

        // Create buffer for reading - used in getPixelBuffer
        pixelBuffer = [device newBufferWithLength: (width * height * 4) options: storageMode];
    }
    TEX_LOG(@">>>> MetalTexture.createTexture()2  (buffer backed texture) -- width = %lu, height = %lu", width, height);
    TEX_LOG(@">>>> MetalTexture.createTexture()2  PB length: %d", (int)([pixelBuffer length]));
    TEX_LOG(@">>>> MetalTexture.createTexture()2  created MetalTexture = %p", texture);
    return self;
}

- (id<MTLBuffer>) getPixelBuffer
{
    TEX_LOG(@"\n");
    id<MTLCommandQueue> queue             = [[context getDevice] newCommandQueue];
    id<MTLCommandBuffer> commandBuffer    = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
    [blitEncoder synchronizeTexture:texture slice:0 level:0];
    [blitEncoder endEncoding];

    TEX_LOG(@">>>> MetalTexture.getPixelBuffer() = %p", commandBuffer);

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    [texture getBytes:(void *)pixelBuffer.contents
             bytesPerRow: width * 4
             fromRegion:MTLRegionMake2D(0, 0, width, height)
             mipmapLevel:0];
    return pixelBuffer;
}

- (id<MTLTexture>) getTexture
{
    return texture;
}

@end // MetalTexture


JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLTexture_nUpdate
(JNIEnv *env, jclass jClass, jlong ctx, jlong nTexturePtr, jbyteArray pixData, jint dstx, jint dsty, jint srcx, jint srcy, jint w, jint h, jint scanStride) {
    TEX_LOG(@"\n");
    TEX_LOG(@"-> Native: MTLTexture_nUpdate srcx: %d, srcy: %d, width: %d, height: %d --- scanStride = %d", srcx, srcy, w, h, scanStride);
    MetalContext* context = (MetalContext*)jlong_to_ptr(ctx);
    MetalTexture* mtlTex  = (MetalTexture*)jlong_to_ptr(nTexturePtr);

    id<MTLTexture> tex = [mtlTex getTexture];
    jbyte *pixels = (*env)->GetByteArrayElements(env, pixData, 0);

    MTLRegion region = {{dstx,dsty,0}, {w, h, 1}};

    [tex replaceRegion:region
             mipmapLevel:0
             withBytes:pixels
             bytesPerRow: scanStride];

    (*env)->ReleaseByteArrayElements(env, pixData, pixels, 0);

    // TODO: MTL: add error detection and return appropriate jlong
    return 0;
}
