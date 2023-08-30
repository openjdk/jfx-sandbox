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
        diffuseColor.x = 0;
        diffuseColor.y = 0;
        diffuseColor.z = 0;
        diffuseColor.w = 0;
        specularColorSet = false;
        specularColor.x = 1;
        specularColor.y = 1;
        specularColor.z = 1;
        specularColor.w = 32;
        map[DIFFUSE] = NULL;
        map[SPECULAR] = NULL;
        map[BUMP] = NULL;
        map[SELFILLUMINATION] = NULL;
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
    specularColor.x = r;
    specularColor.y = g;
    specularColor.z = b;
    specularColor.w = a;
}

- (vector_float4) getDiffuseColor
{
    return diffuseColor;
}

- (vector_float4) getSpecularColor
{
    return specularColor;
}

- (bool) isSpecularMap
{
    return map[SPECULAR] ? true : false;
}

- (bool) isSpecularColor
{
    return specularColorSet;
}

- (int) getSpecType
{
    if ([self isSpecularMap]) {
        return [self isSpecularColor] ? SPEC_MIX : SPEC_TEX;
    }
    return [self isSpecularColor] ? SPEC_CLR : SPEC_NONE;
}

- (bool) isBumpMap
{
    return map[BUMP] ? true : false;
}

- (bool) isSelfIllumMap
{
    return map[SELFILLUMINATION] ? true : false;
}

- (void) setMap:(int)mapID
            map:(id<MTLTexture>)texMap
{
    MESH_LOG(@"MetalPhongMaterial_setMap()");
    // Within the range of DIFFUSE, SPECULAR, BUMP, SELFILLUMINATION
    if (mapID >= 0 && mapID <= 3) {
        map[mapID] = texMap;
    } else {
        MESH_LOG(@"MetalPhongMaterial_setMap(): mapID is out of range");
    }
}

- (id<MTLTexture>) getMap:(int)mapID
{
    // Within the range of DIFFUSE, SPECULAR, BUMP, SELFILLUMINATION
    if (mapID >= 0 && mapID <= 3) {
        return map[mapID];
    }
    MESH_LOG(@"MetalPhongMaterial_getMap(): mapID is out of range");
    return NULL;
}
@end // MetalPhongMaterial
