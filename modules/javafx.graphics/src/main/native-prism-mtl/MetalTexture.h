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

#ifndef METAL_TEXTURE_H
#define METAL_TEXTURE_H

#import "MetalCommon.h"
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import "MetalContext.h"

#ifdef TEX_VERBOSE
#define TEX_LOG NSLog
#else
#define TEX_LOG(...)
#endif

@interface MetalTexture : NSObject
{
    MetalContext *context;

    id<MTLTexture> texture;
    id<MTLTexture> depthTexture;
    id<MTLTexture> depthMSAATexture;
    id<MTLTexture> msaaTexture;

    // Specifying Texture Attributes: https://developer.apple.com/documentation/metal/mtltexturedescriptor
    NSUInteger width;
    NSUInteger height;
    MTLTextureType type;
    MTLTextureUsage usage;
    MTLPixelFormat pixelFormat;
    MTLResourceOptions storageMode;
    NSUInteger mipmapLevelCount;
    bool mipmapped;
    bool isMSAA;
    bool lastDepthMSAA;
}
- (id<MTLTexture>) getTexture;
- (id<MTLTexture>) getDepthTexture;
- (id<MTLTexture>) getDepthMSAATexture;
- (id<MTLTexture>) getMSAATexture;
- (MetalTexture*) createTexture:(MetalContext*)context ofWidth:(NSUInteger)w ofHeight:(NSUInteger)h pixelFormat:(NSUInteger) format useMipMap:(bool)useMipMap;
- (MetalTexture*) createTexture:(MetalContext*)context ofUsage:(MTLTextureUsage)texUsage ofWidth:(NSUInteger)w ofHeight:(NSUInteger)h msaa:(bool)msaa;
- (MetalTexture*) createTexture : (MetalContext*) ctx mtlTex:(long)pTex ofWidth : (NSUInteger)w ofHeight : (NSUInteger)h;
- (void) createDepthTexture;
- (id<MTLBuffer>) getPixelBuffer;
- (bool) isMSAAEnabled;
- (bool) isMipmapped;
- (void)dealloc;

//- (void) blitTo:(MetalTexture*) tex;

@end

#endif
