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

@implementation MetalPipelineManager

- (void) init:(MetalContext*) ctx  libPath:(NSString*) path
{
    context = ctx;
    NSError *error = nil;
    pipeStateDict = [[NSMutableDictionary alloc] init];
    shaderLib = [[context getDevice] newLibraryWithFile:path error:&error];
    vertexFunction = [self getFunction:@"passThrough"];

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
    pipeDesc.colorAttachments[0].blendingEnabled = YES;
    pipeDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipeDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipeDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    //pipeDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    //pipeDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    //pipeDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    id<MTLRenderPipelineState> pipeState = [[context getDevice] newRenderPipelineStateWithDescriptor:pipeDesc error:&error];
    NSAssert(pipeState, @"Failed to create pipeline state to render to texture: %@", error);
    //pipeStateDict[func] = pipeState;

    return pipeState;
}

- (id<MTLRenderPipelineState>) getPipeStateWithFragFuncName:(NSString*) funcName
{
    return [self getPipeStateWithFragFunc:[self getFunction:funcName]];
}

@end // MetalPipelineManager
