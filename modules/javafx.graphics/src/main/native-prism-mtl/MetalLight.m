/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
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

#import "MetalLight.h"

#ifdef MESH_VERBOSE
#define MESH_LOG NSLog
#else
#define MESH_LOG(...)
#endif

@implementation MetalLight

- (MetalLight*) createLight:(float)x y:(float)y z:(float)z
            r:(float)r g:(float)g b:(float)b w:(float)w
            ca:(float)ca la:(float)la qa:(float)qa
            isA:(float)isAttenuated range:(float)range
            dirX:(float)dirX dirY:(float)dirY dirZ:(float)dirZ
            inA:(float)innerAngle outA:(float)outerAngle
            falloff:(float)falloff
{
    self = [super init];
    if (self) {
        MESH_LOG(@"MetalLight_createLight()");
        position[0] = x;
        position[1] = y;
        position[2] = z;
        color[0] = r;
        color[1] = g;
        color[2] = b;
        a = w;
        attenuation[0] = ca;
        attenuation[1] = la;
        attenuation[2] = qa;
        attenuation[3] = isAttenuated;
        maxRange = range;
        direction[0] = dirX;
        direction[1] = dirY;
        direction[2] = dirZ;
        inAngle = innerAngle;
        outAngle = outerAngle;
        foff = falloff;
    }
    return self;
}
@end // MetalLight
