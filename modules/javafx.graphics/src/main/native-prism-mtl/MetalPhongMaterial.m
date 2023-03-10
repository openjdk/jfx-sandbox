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

#import "MetalPhongMaterial.h"

#ifdef MESH_VERBOSE
#define MESH_LOG NSLog
#else
#define MESH_LOG(...)
#endif

@implementation MetalPhongMaterial

- (MetalPhongMaterial*) createPhongMaterial:(MetalContext*)ctx
{
    self = [super init];
    if (self) {
        MESH_LOG(@"MetalPhongMaterial_createPhongMaterial()");
        context = ctx;
        diffuseColor[0] = 0;
        diffuseColor[1] = 0;
        diffuseColor[2] = 0;
        diffuseColor[3] = 0;
        specularColorSet = false;
        specularColor[0] = 1;
        specularColor[1] = 1;
        specularColor[2] = 1;
        specularColor[3] = 32;
        // TODO: MTL: Enable below maps once we have
        // texture support
        /*map[DIFFUSE] = NULL;
        map[SPECULAR] = NULL;
        map[BUMP] = NULL;
        map[SELFILLUMINATION] = NULL;*/
    }
    return self;
}

- (void) setDiffuseColor:(float)r
                       g:(float)g
                       b:(float)b
                       a:(float)a
{
    MESH_LOG(@"MetalPhongMaterial_setDiffuseColor()");
    diffuseColor.x = r;
    diffuseColor.y = g;
    diffuseColor.z = b;
    diffuseColor.w = a;
}

- (void) setSpecularColor:(bool)set
                        r:(float)r
                        g:(float)g
                        b:(float)b
                        a:(float)a
{
    MESH_LOG(@"MetalPhongMaterial_setSpecularColor()");
    specularColorSet = set;
    specularColor[0] = r;
    specularColor[1] = g;
    specularColor[2] = b;
    specularColor[3] = a;
}

- (vector_float4) getDiffuseColor
{
    return diffuseColor;
}
@end // MetalPhongMaterial
