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

#import "MetalMeshView.h"

#ifdef MESH_VERBOSE
#define MESH_LOG NSLog
#else
#define MESH_LOG(...)
#endif

@implementation MetalMeshView

- (MetalMeshView*) createMeshView:(MetalContext*)ctx
                             mesh:(MetalMesh*)mtlMesh
{
    self = [super init];
    if (self) {
        MESH_LOG(@"MetalMeshView_createMeshView()");
        context = ctx;
        mesh = mtlMesh;
        material = NULL;
        ambientLightColor[0] = 0;
        ambientLightColor[1] = 0;
        ambientLightColor[2] = 0;
        numLights = 0;
        lightsDirty = TRUE;
        cullMode = MTLCullModeNone;
        wireframe = FALSE;
    }
    return self;
}

- (void) setMaterial:(MetalPhongMaterial*)pMaterial
{
    MESH_LOG(@"MetalMeshView_setMaterial()");
    material = pMaterial;
}

- (void) setCullingMode:(int)cMode
{
    MESH_LOG(@"MetalMeshView_setCullingMode()");
    cullMode = cMode;
}

- (void) setWireframe:(bool)isWireFrame
{
    MESH_LOG(@"MetalMeshView_setWireframe()");
    wireframe = isWireFrame;
}

- (void) setAmbientLight:(float)r
                       g:(float)g
                       b:(float)b
{
    MESH_LOG(@"MetalMeshView_setAmbientLight()");
    ambientLightColor[0] = r;
    ambientLightColor[1] = g;
    ambientLightColor[2] = b;
}

- (void) setLight:(int)index
        x:(float)x y:(float)y z:(float)z
        r:(float)r g:(float)g b:(float)b w:(float)w
        ca:(float)ca la:(float)la qa:(float)qa
        isA:(float)isAttenuated range:(float)range
        dirX:(float)dirX dirY:(float)dirY dirZ:(float)dirZ
        inA:(float)innerAngle outA:(float)outerAngle
        falloff:(float)falloff
{
    MESH_LOG(@"MetalMeshView_setLight()");
    // NOTE: We only support up to 3 point lights at the present
    if (index >= 0 && index <= MAX_NUM_LIGHTS - 1) {
        MetalLight* light = ([[MetalLight alloc] createLight:x y:y z:z
            r:r g:g b:b w:w
            ca:ca la:la qa:qa
            isA:isAttenuated range:range
            dirX:dirX dirY:dirY dirZ:dirZ
            inA:innerAngle outA:outerAngle
            falloff:falloff]);
        lights[index] = light;
        lightsDirty = TRUE;
    }
}

- (MetalMesh*) getMesh
{
    return mesh;
}

- (int) getCullingMode
{
    return cullMode;
}
@end // MetalMeshView
