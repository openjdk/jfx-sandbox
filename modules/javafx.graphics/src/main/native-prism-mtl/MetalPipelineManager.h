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

#ifndef METAL_PIPELINE_MANAGER_H
#define METAL_PIPELINE_MANAGER_H

#import "MetalCommon.h"
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import "MetalContext.h"
#import "MetalRTTexture.h"

/**
 * native interface for the Java class MTLPipelineManager
 */
@interface MetalPipelineManager : NSObject
{
    id<MTLLibrary> shaderLib;
    id<MTLFunction> vertexFunction;
    MetalContext *context;
    int compositeMode;

    id<MTLRenderPipelineState> solidColorPipeState;


    /*
    MTLRenderPipelineDescriptor* pipeDesc;
    id<MTLRenderPipelineState> pipeState;
    NSMutableArray<id<MTLRenderPipelineState>> *pipelineStates;
    */

    // TODO: MTL: This should be controlled from Java class MTLPipelineManager
    // See MTLPipelineManager class TODOs for more info
    NSMutableDictionary *pipeStateDict;

    // Maintain a set of MTLRenderPipelineStates, based on combination of
    // Vertex and Fragment function
    // MTLRenderCommandEncoder is constructed using two major objects
    // 1. MTLRenderPassDescriptor
    // 2. MTLRenderPipelineState -> Vertex and Fragment function
    // We intend to reuse the MTLRenderPipelineState objects.
    // It is better to keep a reference to each native object on java side
    // for resource management and source maintenance. and hence most of the
    // management should be moved to java side. and this interface should
    // contain helper methods to accomplish the same.
}

- (void) init:(MetalContext*) ctx libPath:(NSString*) libPath;
- (id<MTLFunction>) getFunction:(NSString*) funcName;
- (id<MTLRenderPipelineState>) getPipeStateWithFragFunc:(id<MTLFunction>) fragFunc;
- (id<MTLRenderPipelineState>) getPipeStateWithFragFuncName:(NSString*) funcName;
- (id<MTLRenderPipelineState>) getPhongPipeStateWithFragFunc:(id<MTLFunction>) fragFunc;
- (id<MTLRenderPipelineState>) getPhongPipeStateWithFragFuncName:(NSString*) funcName;
- (id<MTLComputePipelineState>) getComputePipelineStateWithFunc:(NSString*) funcName;
- (id<MTLDepthStencilState>) getDepthStencilState;
- (void) setPipelineCompositeBlendMode:(MTLRenderPipelineDescriptor*) pipeDesc;
- (void) setCompositeBlendMode:(int) mode;
- (void) dealloc;
@end

#endif
