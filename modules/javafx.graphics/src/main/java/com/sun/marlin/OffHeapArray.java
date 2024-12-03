/*
 * Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
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

package com.sun.marlin;

import java.lang.foreign.Arena;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.VarHandle;

// FIXME: We must replace the terminally deprecated sun.misc.Unsafe
// memory access methods; see JDK-8334137
/**
 * An off-heap memory segment (array) that stores {@link Renderer#EDGE_LAYOUT edges}.
 */
// TODO: this class is a temporary wrapper for MemSeg for easier transition. It should be removed in favor
// of directly handling MemSeg probably (unless we find that it's useful to restrict access to MemSeg).
@SuppressWarnings("removal")
final class OffHeapArray  {

    /**
     * An edge layout. Equivalent to:
     * <p>
     * {@code record EDGE_LAYOUT(int curxOr, int error, int bumpX, int bumpErr, int next, int yMax)} 
     */
    static final MemoryLayout EDGE_LAYOUT = MemoryLayout.structLayout(
            ValueLayout.JAVA_INT.withName("curxOr"),
            ValueLayout.JAVA_INT.withName("error"),
            ValueLayout.JAVA_INT.withName("bumpX"),
            ValueLayout.JAVA_INT.withName("bumpErr"),
            ValueLayout.JAVA_INT.withName("next"),
            ValueLayout.JAVA_INT.withName("yMax")
    );

    static final VarHandle curxOr = EDGE_LAYOUT.varHandle(PathElement.groupElement("curxOr"));
    static final VarHandle error = EDGE_LAYOUT.varHandle(PathElement.groupElement("error"));
    static final VarHandle bumpX = EDGE_LAYOUT.varHandle(PathElement.groupElement("bumpX"));
    static final VarHandle bumpErr = EDGE_LAYOUT.varHandle(PathElement.groupElement("bumpErr"));
    static final VarHandle next = EDGE_LAYOUT.varHandle(PathElement.groupElement("next"));
    static final VarHandle yMax = EDGE_LAYOUT.varHandle(PathElement.groupElement("yMax"));

    // size of one edge in bytes
    static final long SIZEOF_EDGE_BYTES = EDGE_LAYOUT.byteSize();

    private static final Arena ARENA = Arena.global();
    static MemorySegment memSeg;

    private static MemorySegment edgeMemSeg() {
        return ARENA.allocate(EDGE_LAYOUT);
    }
    
    // unsafe reference
//    static final Unsafe UNSAFE;
    // size of int / float
//    static final int SIZE_INT;

//    static {
//        try {
//            final Field field = Unsafe.class.getDeclaredField("theUnsafe");
//            field.setAccessible(true);
//            UNSAFE = (Unsafe) field.get(null);
//        } catch (Exception e) {
//            throw new InternalError("Unable to get sun.misc.Unsafe instance", e);
//        }
//
//        SIZE_INT = Unsafe.ARRAY_INT_INDEX_SCALE;
//    }

    /* members */
//    long address;
//    long length;
//    int  used;

    long count;
    long index;
    
    /**
     * Creates an off-heap memory segment capable of holding {@code count} number of edges.
     */
    private OffHeapArray(final Object parent, final long count) {
        this.count = count;
        memSeg = ARENA.allocate(EDGE_LAYOUT, count);

        // note: may throw OOME:
//        this.address = UNSAFE.allocateMemory(len);
//        this.length  = len;
//        this.used    = 0;
//        if (LOG_UNSAFE_MALLOC) {
//            MarlinUtils.logInfo(System.currentTimeMillis()
//                                + ": OffHeapArray.allocateMemory =   "
//                                + len + " to addr = " + this.address);
//        }

        // Register a cleaning function to ensure freeing off-heap memory:
        MarlinUtils.getCleaner().register(parent, this::free);
    }

    private void add(MemorySegment edgeMedSeg) {
        // add edge segment
        index++;
    }
    
    /*
     * As realloc may change the address, updating address is MANDATORY
     * @param len new array length
     * @throws OutOfMemoryError if the allocation is refused by the system
     */
    private void resize(final long newCount) {
        memSeg = memSeg.reinterpret(newCount * EDGE_LAYOUT.byteSize());
        count = newCount;
        
        // note: may throw OOME:
//        this.address = UNSAFE.reallocateMemory(address, len);
//        this.length  = len;
//        if (LOG_UNSAFE_MALLOC) {
//            MarlinUtils.logInfo(System.currentTimeMillis()
//                                + ": OffHeapArray.reallocateMemory = "
//                                + len + " to addr = " + this.address);
//        }
    }

    /**
     * Frees the allocation in the memory segment.
     * NOTE: it's not possible to deallocate a memseg from the global arena!
     */
    private void free() {
        memSeg.fill((byte) 0);
        index = 0;

//        UNSAFE.freeMemory(this.address);
//        if (LOG_UNSAFE_MALLOC) {
//            MarlinUtils.logInfo(System.currentTimeMillis()
//                                + ": OffHeapArray.freeMemory =       "
//                                + this.length
//                                + " at addr = " + this.address);
//        }
//        this.address = 0L;
    }

    /**
     * Fills the memory segment with the given value in order to be able to reuse it by another renderer.
     */
    private void fill(final byte val) {
        memSeg.fill(val);
        index = 0;
        
//        UNSAFE.setMemory(this.address, this.length, val);
    }
}
