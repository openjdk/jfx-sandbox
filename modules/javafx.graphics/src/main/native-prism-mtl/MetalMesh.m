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

#import "MetalMesh.h"

#ifdef MESH_VERBOSE
#define MESH_LOG NSLog
#else
#define MESH_LOG(...)
#endif
typedef struct
{
    vector_float4 position;
    vector_float4 color;
} MBEVertex;
@implementation MetalMesh

- (id) createMesh:(MetalContext*)ctx
{
    self = [super init];
    if (self) {
        MESH_LOG(@"MetalMesh->createMesh()");
        context = ctx;
        numVertices = 0;
        numIndices = 0;
        indexType = MTLIndexTypeUInt16;
    }
    return self;
}

- (bool) buildBuffersShort:(float*)vb
                vSize:(unsigned int)vbSize
              iBuffer:(unsigned short*)ib
                iSize:(unsigned int)ibSize
{
    MESH_LOG(@"MetalMesh->buildBuffersShort");
    id<MTLDevice> device = [context getDevice];
    unsigned int size = vbSize * sizeof (float);
    unsigned int vbCount = vbSize / NUM_OF_FLOATS_PER_VERTEX;
    MESH_LOG(@"vbCount %d", vbCount);
    // TODO: MTL: Cleanup this code in future if we think we don't need
    // to add padding to float3 data
    /*VS_PHONG_INPUT* pVert = vertices;
    for (int i = 0; i < vbCount; i++) {
        pVert->position.x = *(vb + (i * 9));
        pVert->position.y = *(vb + (i * 9) + 1);
        pVert->position.z = *(vb + (i * 9) + 2);
        pVert->position.w = 1.0;
        pVert->texCoord.x = *(vb + (i * 9) + 3);
        pVert->texCoord.y = *(vb + (i * 9) + 4);
        pVert->texCoord.z = 1.0;
        pVert->texCoord.w = 1.0;
        pVert->normal.x = *(vb + (i * 9) + 5);
        pVert->normal.y = *(vb + (i * 9) + 6);
        pVert->normal.z = *(vb + (i * 9) + 7);
        pVert->normal.w = *(vb + (i * 9) + 8);
        pVert++;
    }*/

    if (numVertices != vbCount) {
        [self releaseVertexBuffer];
        [self createVertexBuffer:size];
        numVertices = vbCount;
        MESH_LOG(@"numVertices %lu", numVertices);
    }

    NSUInteger currentIndex = [context getCurrentBufferIndex];
    if (vertexBuffer[currentIndex] != nil) {
        MESH_LOG(@"Updating VertexBuffer");
        memcpy(vertexBuffer[currentIndex].contents, vb, size);
    }

    size = ibSize * sizeof (unsigned short);
    MESH_LOG(@"IndexBuffer size %d", size);
    if (numIndices != ibSize) {
        [self releaseIndexBuffer];
        [self createIndexBuffer:size];
        numIndices = ibSize;
        MESH_LOG(@"numIndices %lu", numIndices);
    }

    if (indexBuffer[currentIndex] != nil) {
        MESH_LOG(@"Updating IndexBuffer");
        memcpy(indexBuffer[currentIndex].contents, ib, size);
    }
    indexType = MTLIndexTypeUInt16;
    MESH_LOG(@"MetalMesh->buildBuffersShort done");
    return true;
}

- (bool) buildBuffersInt:(float*)vb
                vSize:(unsigned int)vbSize
              iBuffer:(unsigned int*)ib
                iSize:(unsigned int)ibSize
{
    MESH_LOG(@"MetalMesh->buildBuffersInt");
    id<MTLDevice> device = [context getDevice];
    unsigned int size = vbSize * sizeof (float);
    unsigned int vbCount = vbSize / NUM_OF_FLOATS_PER_VERTEX;
    MESH_LOG(@"vbCount %d", vbCount);

    if (numVertices != vbCount) {
        [self releaseVertexBuffer];
        [self createVertexBuffer:size];
        numVertices = vbCount;
        MESH_LOG(@"numVertices %lu", numVertices);
    }

    NSUInteger currentIndex = [context getCurrentBufferIndex];
    if (vertexBuffer[currentIndex] != nil) {
        MESH_LOG(@"Updating VertexBuffer");
        memcpy(vertexBuffer[currentIndex].contents, vb, size);
    }

    size = ibSize * sizeof (unsigned int);
    MESH_LOG(@"IndexBuffer size %d", size);
    if (numIndices != ibSize) {
        [self releaseIndexBuffer];
        [self createIndexBuffer:size];
        numIndices = ibSize;
        MESH_LOG(@"numIndices %lu", numIndices);
    }

    if (indexBuffer[currentIndex] != nil) {
        MESH_LOG(@"Updating IndexBuffer");
        memcpy(indexBuffer[currentIndex].contents, ib, size);
    }

    indexType = MTLIndexTypeUInt32;
    MESH_LOG(@"MetalMesh->buildBuffersInt done");
    return true;
}

- (void) release
{
    MESH_LOG(@"MetalMesh->release");
    [self releaseVertexBuffer];
    [self releaseIndexBuffer];
    context = nil;
}

- (void) createVertexBuffer:(unsigned int)size;
{
    MESH_LOG(@"MetalMesh->createVertexBuffer");
    id<MTLDevice> device = [context getDevice];
    for (int i = 0; i < BUFFER_SIZE; i++) {
        vertexBuffer[i] = [[device newBufferWithLength:size
            options:MTLResourceStorageModeShared] autorelease];
    }
}

- (void) releaseVertexBuffer
{
    MESH_LOG(@"MetalMesh->releaseVertexBuffer");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        vertexBuffer[i] = nil;
    }
    numVertices = 0;
}

- (void) createIndexBuffer:(unsigned int)size;
{
    MESH_LOG(@"MetalMesh->createIndexBuffer");
    id<MTLDevice> device = [context getDevice];
    for (int i = 0; i < BUFFER_SIZE; i++) {
        indexBuffer[i] = [[device newBufferWithLength:size
            options:MTLResourceStorageModeShared] autorelease];
    }
}

- (void) releaseIndexBuffer
{
    MESH_LOG(@"MetalMesh->releaseIndexBuffer");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        indexBuffer[i] = nil;
    }
    numIndices = 0;
}

- (id<MTLBuffer>) getVertexBuffer
{
    return vertexBuffer[[context getCurrentBufferIndex]];
}

- (id<MTLBuffer>) getIndexBuffer
{
    return indexBuffer[[context getCurrentBufferIndex]];
}

- (NSUInteger) getNumVertices
{
    return numVertices;
}

- (NSUInteger) getNumIndices
{
    return numIndices;
}

- (NSUInteger) getIndexType
{
    return indexType;
}
@end // MetalMesh
