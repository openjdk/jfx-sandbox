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

// ---------------------------- Debug helper for Developers -------------------------
// Note : TODO: MTL: Remove this functionality before integrating Metal pipeline code to the mainline.
//
// This implementation is to utilize "Metal Debuger" present in Xcode.
// See - https://developer.apple.com/documentation/xcode/capturing-a-metal-workload-programmatically
//
// Limitation - Attaching to a java process and debugging frame by frame is not possible.
//
// Currently, this debug helper can capture each frame related data to a file.
// It is configured to capture frame data from MTLDevice for entire lifetime of the application.
// This creates a trace file which is in GBs. So, please use this debug helper judiciously
// (i.e. with applications running for a short time and with less animation.)
// The generated trace file can be opened in Xcode and replayed.
//
// If needed, this can be changed in future to capture only "scope of interest" as explained at -
// https://developer.apple.com/documentation/xcode/creating-and-using-custom-capture-scopes
//
// Prerequisites:
// 1. MacOS 14 (Sonoma)
// 2. On terminal, set environment variable MTL_CAPTURE_ENABLED=1.
//
// Uncomment below line to capture Metal GPU Debug Trace to file GPUTraceFilename specified below.
//#define JFX_MTL_DEBUG_CAPTURE
#ifdef JFX_MTL_DEBUG_CAPTURE
NSString *GPUTraceFilename = @"file:///tmp/fx_metal.gputrace";
#endif
// ---------------------------- Debug helper for Developers -------------------------

@implementation MetalPipelineManager

- (void) init:(MetalContext*) ctx  libData:(dispatch_data_t) libData
{
    context = ctx;
    NSError *error = nil;
    shaderLib = [[context getDevice] newLibraryWithData:libData error:&error];

    if (shaderLib != nil) {
        vertexFunction = [self getFunction:@"passThrough"];
    } else {
        METAL_LOG(@"-> MetalPipelineManager.init: Failed to create shader library");
    }
    clearRttPipeStateDict = [[NSMutableDictionary alloc] init];

#ifdef JFX_MTL_DEBUG_CAPTURE

    if (@available(macOS 14, *)) {
        @autoreleasepool {
            NSLog(@"JFX_MTL_DEBUG_CAPTURE is enabled");

            MTLCaptureManager* captureManager = [MTLCaptureManager sharedCaptureManager];
            if (![captureManager supportsDestination:MTLCaptureDestinationGPUTraceDocument]) {
                NSLog(@" MTLCaptureDestinationGPUTraceDocument destination is not supported.");
            } else {
                NSLog(@"MTLCaptureDestinationGPUTraceDocument destination is supported.");
                MTLCaptureDescriptor* captureDescriptor = [MTLCaptureDescriptor new];
                [captureDescriptor setCaptureObject: [context getDevice]];
                [captureDescriptor setDestination:MTLCaptureDestinationGPUTraceDocument];

                NSURL* url = [NSURL URLWithString:GPUTraceFilename];
                [captureDescriptor setOutputURL:url];
                [captureManager startCaptureWithDescriptor:captureDescriptor error:nil];
            }
        }
    }
#endif
}

- (id<MTLFunction>) getFunction:(NSString*) funcName
{
    // METAL_LOG(@"------> getFunction: %@", funcName);
    return [shaderLib newFunctionWithName:funcName];
}

- (id<MTLRenderPipelineState>) getClearRttPipeState
{
    METAL_LOG(@">>>> MetalPipelineManager.getClearRttPipeState()");

    int sampleCount = 1;
    if ([[context getRTT] isMSAAEnabled]) {
        sampleCount = 4;
    }
    NSNumber *keySampleCount = [NSNumber numberWithInt:sampleCount];
    id<MTLRenderPipelineState> clearRttPipeState = clearRttPipeStateDict[keySampleCount];
    if (clearRttPipeState == nil) {
        MTLRenderPipelineDescriptor* pipeDesc = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
        pipeDesc.vertexFunction   = [self getFunction:@"clearVF"];;
        pipeDesc.fragmentFunction = [self getFunction:@"clearFF"];
        pipeDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm; //[[context getRTT] getPixelFormat]; //rtt.pixelFormat
        pipeDesc.sampleCount = sampleCount;

        NSError* error;
        clearRttPipeState = [[context getDevice] newRenderPipelineStateWithDescriptor:pipeDesc error:&error];
        NSAssert(clearRttPipeState, @"Failed to create clear pipeline state: %@", error);
        [clearRttPipeStateDict setObject:clearRttPipeState forKey:keySampleCount];
    }
    METAL_LOG(@"<<<< MetalPipelineManager.getClearRttPipeState()\n");
    return clearRttPipeState;
}

- (id<MTLRenderPipelineState>) getPipeStateWithFragFunc:(id<MTLFunction>) func
                                          compositeMode:(int) compositeMode
{
    METAL_LOG(@"MetalPipelineManager.getPipeStateWithFragFunc()");
    NSError* error;
    MTLRenderPipelineDescriptor* pipeDesc = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
    pipeDesc.vertexFunction = vertexFunction;
    pipeDesc.fragmentFunction = func;
    pipeDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm; //rtt.pixelFormat

    if ([[context getRTT] isMSAAEnabled]) {
        pipeDesc.sampleCount = 4;
    } else {
        pipeDesc.sampleCount = 1;
    }

    [self setPipelineCompositeBlendMode:pipeDesc
                          compositeMode:compositeMode];

    id<MTLRenderPipelineState> pipeState = [[context getDevice] newRenderPipelineStateWithDescriptor:pipeDesc error:&error];
    NSAssert(pipeState, @"Failed to create pipeline state to render to texture: %@", error);

    return pipeState;
}

- (id<MTLComputePipelineState>) getComputePipelineStateWithFunc:(NSString*) funcName
{
    NSError* error;

    id<MTLFunction> kernelFunction = [shaderLib newFunctionWithName:funcName];

    id<MTLComputePipelineState> pipeState =  [[context getDevice] newComputePipelineStateWithFunction:kernelFunction
                                                                       error:&error];

    NSAssert(pipeState, @"Failed to create compute pipeline state: %@", error);

    return pipeState;
}


- (id<MTLRenderPipelineState>) getPhongPipeStateWithFragFunc:(id<MTLFunction>) func
                                               compositeMode:(int) compositeMode
{
    METAL_LOG(@"MetalPipelineManager.getPhongPipeStateWithFragFunc()");
    NSError* error;
    MTLRenderPipelineDescriptor* pipeDesc = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
    pipeDesc.vertexFunction = [self getFunction:@"PhongVS"];
    pipeDesc.fragmentFunction = func;
    pipeDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm; //rtt.pixelFormat
    // Not seeing further increase in performance after making 3D MTLBuffers immutable
    // with triple buffer implementation. Keeping the property as a comment
    // for future exploration.
    //pipeDesc.vertexBuffers[0].mutability = MTLMutabilityImmutable;
    if ([context isDepthEnabled]) {
        pipeDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    }
    if ([[context getRTT] isMSAAEnabled]) {
        pipeDesc.sampleCount = 4;
    } else {
        pipeDesc.sampleCount = 1;
    }

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
    [self setPipelineCompositeBlendMode:pipeDesc
                          compositeMode:compositeMode];
    id<MTLRenderPipelineState> pipeState = [[[context getDevice]
        newRenderPipelineStateWithDescriptor:pipeDesc error:&error] autorelease];
    NSAssert(pipeState, @"Failed to create pipeline state for phong shader: %@", error);

    return pipeState;
}

- (id<MTLRenderPipelineState>) getPhongPipeStateWithFragFuncName:(NSString*) funcName
                                                   compositeMode:(int) compositeMode;
{
    return [self getPhongPipeStateWithFragFunc:[self getFunction:funcName]
                                 compositeMode:compositeMode];
}

- (id<MTLDepthStencilState>) getDepthStencilState
{
    MTLDepthStencilDescriptor *depthStencilDescriptor = [[MTLDepthStencilDescriptor new] autorelease];
    if ([context isDepthEnabled]) {
        depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLess;
        depthStencilDescriptor.depthWriteEnabled = YES;
    } else {
        depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
        depthStencilDescriptor.depthWriteEnabled = NO;
    }
    id<MTLDepthStencilState> depthStencilState = [[context getDevice] newDepthStencilStateWithDescriptor:depthStencilDescriptor];
    return depthStencilState;
}

- (void) setPipelineCompositeBlendMode:(MTLRenderPipelineDescriptor*) pipeDesc
                         compositeMode:(int) compositeMode
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

- (void) dealloc
{
    METAL_LOG(@"MetalPipelineManager.dealloc ----- releasing native resources");

#ifdef JFX_MTL_DEBUG_CAPTURE
    if (@available(macOS 14, *)) {
        NSLog(@"stopping capture...");
        MTLCaptureManager* captureManager = [MTLCaptureManager sharedCaptureManager];
        [captureManager stopCapture];
    }
#endif

    if (shaderLib != nil) {
        [shaderLib release];
        shaderLib = nil;
    }

    [super dealloc];
}

@end // MetalPipelineManager
