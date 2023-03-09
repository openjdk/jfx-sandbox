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

#import "MetalPipelineManager.h"
#include "com_sun_prism_mtl_MTLContext.h"


@implementation MetalPipelineManager

- (void) init:(MetalContext*) ctx  libPath:(NSString*) path
{
    context = ctx;
    NSError *error = nil;
    pipeStateDict = [[NSMutableDictionary alloc] init];
    shaderLib = [[context getDevice] newLibraryWithFile:path error:&error];
    vertexFunction = [self getFunction:@"passThrough"];
    compositeMode = com_sun_prism_mtl_MTLContext_MTL_COMPMODE_SRCOVER; //default

    if (shaderLib != nil) {
        NSArray<NSString *> *functionNames = [shaderLib functionNames];
        //pipelineStates = [[NSMutableArray alloc] initWithCapacity:[functionNames count]];

        METAL_LOG(@"-> Shader library created, number of the functions in library %lu", [functionNames count]);

        for (NSString *name in functionNames) {
            if ([name isEqualToString:@"passThrough"]) {
                // passThrough is the vertex function
            } else {
                //pipeStateDict[name] = [self getPipeStateWithFragFuncName:name];
            }
        }
        /*for (NSString *name in functionNames)
        {
            METAL_LOG(@" printing from dictionary %@", ((MTLRenderPipelineDescriptor*)pipeStateDict[name]).label);
        }*/
    } else {
        METAL_LOG(@"-> Failed to create shader library");
    }
}

- (id<MTLFunction>) getFunction:(NSString*) funcName
{
    // METAL_LOG(@"------> getFunction: %@", funcName);
    return [shaderLib newFunctionWithName:funcName];
}

- (id<MTLRenderPipelineState>) getPipeStateWithFragFunc:(id<MTLFunction>) func
{
    METAL_LOG(@"MetalPipelineManager.getPipeStateWithFragFunc()");
    if (pipeStateDict[func] != nil) {
        METAL_LOG(@"MetalPipelineManager.getPipeStateWithFragFunc()  return from Dict");
        // TODO: MTL: This decision making should be moved to java side.
        //return pipeStateDict[func];
    }
    NSError* error;
    MTLRenderPipelineDescriptor* pipeDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipeDesc.vertexFunction = vertexFunction;
    pipeDesc.fragmentFunction = func;
    pipeDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm; //rtt.pixelFormat

    [self setPipelineCompositeBlendMode:pipeDesc];

    id<MTLRenderPipelineState> pipeState = [[context getDevice] newRenderPipelineStateWithDescriptor:pipeDesc error:&error];
    NSAssert(pipeState, @"Failed to create pipeline state to render to texture: %@", error);
    //pipeStateDict[func] = pipeState;

    return pipeState;
}

- (id<MTLRenderPipelineState>) getPipeStateWithFragFuncName:(NSString*) funcName
{
    return [self getPipeStateWithFragFunc:[self getFunction:funcName]];
}

- (id<MTLRenderPipelineState>) getPhongPipeStateWithFragFunc:(id<MTLFunction>) func
{
    METAL_LOG(@"MetalPipelineManager.getPhongPipeStateWithFragFunc()");
    NSError* error;
    MTLRenderPipelineDescriptor* pipeDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipeDesc.vertexFunction = [self getFunction:@"PhongVS"];
    pipeDesc.fragmentFunction = func;
    pipeDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm; //rtt.pixelFormat

    // TODO: MTL: Cleanup this code in future if we think we don't need
    // to add padding to float3 data and use VertexDescriptor
    /*MTLVertexDescriptor* vertDesc = [[MTLVertexDescriptor alloc] init];
    vertDesc.attributes[0].format = MTLVertexFormatFloat4;
    vertDesc.attributes[0].offset = 0;
    vertDesc.attributes[0].bufferIndex = 0;
    vertDesc.attributes[1].format = MTLVertexFormatFloat4;
    vertDesc.attributes[1].bufferIndex = 0;
    vertDesc.attributes[1].offset = 16;
    vertDesc.attributes[2].format = MTLVertexFormatFloat4;
    vertDesc.attributes[2].bufferIndex = 0;
    vertDesc.attributes[2].offset = 32;
    vertDesc.layouts[0].stride = 48;
    vertDesc.layouts[0].stepRate = 1;
    vertDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    pipeDesc.vertexDescriptor = vertDesc;*/
    id<MTLRenderPipelineState> pipeState = [[context getDevice] newRenderPipelineStateWithDescriptor:pipeDesc error:&error];
    NSAssert(pipeState, @"Failed to create pipeline state for phong shader: %@", error);

    return pipeState;
}

- (id<MTLRenderPipelineState>) getPhongPipeStateWithFragFuncName:(NSString*) funcName
{
    return [self getPhongPipeStateWithFragFunc:[self getFunction:funcName]];
}

- (void) setCompositeBlendMode:(int) mode
{
    METAL_LOG(@"-> Native: MetalPipelineManager setCompositeBlendMode --- mode = %d", mode);
    compositeMode = mode;
}

- (void) setPipelineCompositeBlendMode: (MTLRenderPipelineDescriptor*) pipeDesc
{
    MTLBlendFactor srcFactor;
    MTLBlendFactor dstFactor;

    switch(compositeMode) {
        case com_sun_prism_mtl_MTLContext_MTL_COMPMODE_CLEAR:
            srcFactor = MTLBlendFactorZero;
            dstFactor = MTLBlendFactorZero;
            break;

        case com_sun_prism_mtl_MTLContext_MTL_COMPMODE_SRC:
            srcFactor = MTLBlendFactorOne;
            dstFactor = MTLBlendFactorZero;
            break;

        case com_sun_prism_mtl_MTLContext_MTL_COMPMODE_SRCOVER:
            srcFactor = MTLBlendFactorOne;
            dstFactor = MTLBlendFactorOneMinusSourceAlpha;
            break;

        case com_sun_prism_mtl_MTLContext_MTL_COMPMODE_DSTOUT:
            srcFactor = MTLBlendFactorZero;
            dstFactor = MTLBlendFactorOneMinusSourceAlpha;
            break;

        case com_sun_prism_mtl_MTLContext_MTL_COMPMODE_ADD:
            srcFactor = MTLBlendFactorOne;
            dstFactor = MTLBlendFactorOne;
            break;

        default:
            srcFactor = MTLBlendFactorOne;
            dstFactor = MTLBlendFactorOneMinusSourceAlpha;
            break;
    }

    pipeDesc.colorAttachments[0].blendingEnabled = YES;
    pipeDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipeDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

    pipeDesc.colorAttachments[0].sourceAlphaBlendFactor = srcFactor;
    pipeDesc.colorAttachments[0].sourceRGBBlendFactor = srcFactor;
    pipeDesc.colorAttachments[0].destinationAlphaBlendFactor = dstFactor;
    pipeDesc.colorAttachments[0].destinationRGBBlendFactor = dstFactor;
}

@end // MetalPipelineManager
