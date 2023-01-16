/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
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

#import "MetalPhongShader.h"
#import "MetalRTTexture.h"
#import "MetalPipelineManager.h"

#ifdef MESH_VERBOSE
#define MESH_LOG NSLog
#else
#define MESH_LOG(...)
#endif

@implementation MetalPhongShader

- (MetalPhongShader*) createPhongShader:(MetalContext*)ctx
{
    self = [super init];
    if (self) {
        MESH_LOG(@"MetalPhongShader_createPhongShader()");
        context = ctx;
        /*id<MTLCommandBuffer> commandBuffer = [context getCurrentCommandBuffer];
        phongRPD = [MTLRenderPassDescriptor new];
        phongRPD.colorAttachments[0].loadAction = MTLLoadActionClear;
        phongRPD.colorAttachments[0].clearColor = MTLClearColorMake(1, 1, 1, 1); // make this programmable
        phongRPD.colorAttachments[0].storeAction = MTLStoreActionStore;
        phongRPD.colorAttachments[0].texture = [[context getRTT] getTexture];
        phongEncoder = [commandBuffer renderCommandEncoderWithDescriptor:phongRPD];
        id<MTLRenderPipelineState> phongPipelineState =
            [[context getPipelineManager] getPhongPipeStateWithFragFuncName:@"PhongPS"];
        [phongEncoder setRenderPipelineState:phongPipelineState];*/
        // TODO remove below line once we actually encode draw call
        //[phongEncoder endEncoding];
    }
    return self;
}
@end // MetalPhongShader