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

#import "MetalRTTexture.h"
#import "com_sun_prism_mtl_MTLRTTexture.h"

@implementation MetalRTTexture

- (MetalRTTexture*) createTexture : (MetalContext*) ctx
                          ofWidth : (NSUInteger) w
                         ofHeight : (NSUInteger) h
{
    TEX_LOG(@"-> MetalRTTexture.createTexture()");
    self = [super init];
    if (self) {
        pw = w;
        ph = h;
        [super createTexture:ctx ofUsage:MTLTextureUsageRenderTarget ofWidth:w ofHeight:h];
    }
    return self;
}

- (id<MTLTexture>) getTexture
{
    return [super getTexture];
}

- (void) setContentDimensions:(int) w
                       height:(int) h
{
    cw = w;
    ch = h;
}

- (int) getPw { return pw; }
- (int) getPh { return ph; }
- (int) getCw { return cw; }
- (int) getCh { return ch; }

@end // MetalRTTexture


JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLRTTexture_nCreateRT
  (JNIEnv *env, jclass jClass, jlong ctx, jint pw, jint ph, jint cw,
    jint ch, jobject wrapMode, jboolean msaa)
{
    TEX_LOG(@"-> Native: MTLRTTexture_nCreateRT pw: %d, ph: %d, cw: %d, ch: %d", pw, ph, cw, ch);
    MetalContext* context = (MetalContext*)jlong_to_ptr(ctx);
    MetalRTTexture* rtt = [[MetalRTTexture alloc] createTexture:context ofWidth:pw ofHeight:ph];
    [rtt setContentDimensions:cw height:ch];
    jlong rtt_ptr = ptr_to_jlong(rtt);
    return rtt_ptr;
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLRTTexture_nReadPixelsFromContextRTT
    (JNIEnv *env, jclass class, jlong jTexPtr, jobject pixData)
{
    TEX_LOG(@"-> Native: MTLRTTexture_nReadPixelsFromContextRTT");

    MetalRTTexture* rtt = (MetalRTTexture*) jlong_to_ptr(jTexPtr);
    int *texContent = (int*)[[rtt getPixelBuffer] contents];
    int cw = [rtt getCw];
    int ch = [rtt getCh];

    int* pDst = (int*) (*env)->GetDirectBufferAddress(env, pixData);
    memcpy(pDst, texContent, cw * ch * 4);
}
