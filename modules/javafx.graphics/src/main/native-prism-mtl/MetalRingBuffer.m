/*
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
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

#import "MetalRingBuffer.h"

@implementation MetalRingBuffer

static const unsigned int BUFFER_LENGTH = RING_BUFF_SIZE;

// start: making the class perfect Singleton
static MetalRingBuffer* instance = nil;

+ (instancetype) getInstance {
    if (instance == nil) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            instance = [[self alloc] init];
        });
    }
    return instance;
}

+ (instancetype) allocWithZone:(struct _NSZone *)zone {
    if (instance == nil) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            instance = [super allocWithZone:zone];
        });
    }
    return instance;
}

- (id) copyWithZone:(NSZone *)zone {
    return self;
}

- (id) mutableCopyWithZone:(NSZone *)zone {
    return self;
}
// end: making the class perfect Singleton

- (instancetype) init {
    self = [super init];
    if (self) {
        currentOffset = 0;
        numReservedBytes = 0;
        currentBufferIndex = 0;

        for (int i = 0; i < NUM_BUFFERS; i++) {
            isBufferInUse[i] = false;
            buffer[i] = [MTLCreateSystemDefaultDevice() newBufferWithLength:BUFFER_LENGTH
                                                                    options:MTLResourceStorageModeShared];
            buffer[i].label = [NSString stringWithFormat:@"JFX Ring Buffer"];
        }
    }
    return self;
}

- (bool) isBufferAvailable {
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (!isBufferInUse[i]) {
            return true;
        }
    }
    return false;
}

// This method assumes that caller has made sure that a buffer is available
// by calling the method isBufferAvailable().
// If there is no buffer available then the behavior is undefined and
// should cause visual artefacts or may Metal validation may fail or crash.

- (void) updateBufferInUse {
    unsigned int prevBufferIndex = currentBufferIndex;
    for (int i = currentBufferIndex + 1; i < NUM_BUFFERS; i++) {
        if (!isBufferInUse[i]) {
            currentBufferIndex = i;
        }
    }
    if (prevBufferIndex == currentBufferIndex) {
        for (int i = 0; i < currentBufferIndex; i++) {
            if (!isBufferInUse[i]) {
                currentBufferIndex = i;
            }
        }
    }
    isBufferInUse[currentBufferIndex] = true;
    currentOffset = 0;
    numReservedBytes = 0;
}

- (id<MTLBuffer>) getBuffer {
    return [self getCurrentBuffer];
}

- (id<MTLBuffer>) getCurrentBuffer {
    return buffer[currentBufferIndex];
}

- (unsigned int) getCurrentBufferIndex {
    return currentBufferIndex;
}

- (void) resetBuffer:(unsigned int)index {
    isBufferInUse[index] = false;
}

- (int) reserveBytes:(unsigned int)length {
    if (length > BUFFER_LENGTH * RESERVE_SIZE_THRESHOLD) {
        // The requested length is too large. return -2 indicating
        // that bytes are not reserved on the RingBuffer and
        // caller should take care of allocating a separate buffer.
        return -2;
    }
    currentOffset = numReservedBytes;
    unsigned int remainder = currentOffset % BUFFER_OFFSET_ALIGNMENT;
    if (remainder != 0) {
        currentOffset = currentOffset + BUFFER_OFFSET_ALIGNMENT - remainder;
    }

    if (currentOffset > BUFFER_LENGTH || length > (BUFFER_LENGTH - currentOffset)) {
        // RingBuffer overflows with requested length.
        // Caller should commit the command buffer and reserve buffer again.
        return -1;
    }
    numReservedBytes = currentOffset + length;
    return currentOffset;
}

- (void) dealloc {
    for (int i = 0; i < NUM_BUFFERS; i++) {
        [buffer[i] release];
        buffer[i] = nil;
    }
    [super dealloc];
    instance = nil;
}

@end
