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
#import "MetalResourceFactory.h"

@implementation MetalResourceFactory

- (MetalTexture*) createTexture {return nil;}

- (MetalRTTexture*) createRTT {return nil;}
- (void) disposeRTT:(MetalRTTexture*) rtt {}

- (void) disposeTexture:(MetalTexture*) texture {}

- (id<MTLBuffer>) allocateBuffer {return 0;}
- (void) releaseBuffer {}

- (void) loadMTLLibrary {}
- (id<MTLFunction>) getVertexFunction {return 0;}
- (id<MTLFunction>) getFragmentFunction {return 0;}

@end // MetalResourceFactory


JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLResourceFactory_nCreateTexture
    (JNIEnv *env, jclass class, jlong pContext, jint format, jint hint,
        jboolean isRTT, jint width, jint height, jint samples, jboolean useMipmap) {

        METAL_LOG(@"-> MTLResourceFactory_nCreateTexture");
        MetalContext* context = (MetalContext*) jlong_to_ptr(pContext);
        jlong rtt = ptr_to_jlong([[MetalTexture alloc] createTexture:context ofWidth:width ofHeight:height pixelFormat:format]);
        return rtt;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLResourceFactory_nReleaseTexture
    (JNIEnv *env, jclass class, jlong pContext, jlong pTexture) {

        MetalContext* context = (MetalContext*) jlong_to_ptr(pContext);
        MetalTexture* pTex = (MetalTexture*) jlong_to_ptr(pTexture);

        METAL_LOG(@"-> MTLResourceFactory_nReleaseTexture : Releasing MetalTexture = %lu", pTexture);

        [pTex dealloc];
        pTex = NULL;
}
