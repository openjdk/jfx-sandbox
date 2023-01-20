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

package com.sun.prism.mtl;

import com.sun.prism.MediaFrame;
import com.sun.prism.PixelFormat;
import com.sun.prism.Texture;
import com.sun.prism.impl.BaseTexture;

import java.nio.Buffer;
import java.nio.ByteBuffer;

public class MTLTexture<T extends MTLTextureData> extends BaseTexture<MTLTextureResource<T>> {

    private final MTLContext context;
    private long texPtr;

    MTLTexture(MTLContext context, MTLTextureResource<T> resource,
               PixelFormat format, WrapMode wrapMode,
               int physicalWidth, int physicalHeight,
               int contentX, int contentY, int contentWidth, int contentHeight, boolean useMipmap) {

        super(resource, format, wrapMode,
              physicalWidth, physicalHeight,
              contentX, contentY, contentWidth, contentHeight, useMipmap);
        this.context = context;
        texPtr = resource.getResource().getResource();

        System.err.println("MTLTexture(): context = " + context + ", resource = " + resource + ", format = " + format + ", wrapMode = " + wrapMode + ", physicalWidth = " + physicalWidth + ", physicalHeight = " + physicalHeight + ", contentX = " + contentX + ", contentY = " + contentY + ", contentWidth = " + contentWidth + ", contentHeight = " + contentHeight + ", useMipmap = " + useMipmap);
    }

    MTLTexture(MTLContext context, MTLTextureResource<T> resource,
               PixelFormat format, WrapMode wrapMode,
               int physicalWidth, int physicalHeight,
               int contentX, int contentY, int contentWidth, int contentHeight,
               int maxContentWidth, int maxContentHeight, boolean useMipmap) {

        super(resource, format, wrapMode,
              physicalWidth, physicalHeight,
              contentX, contentY, contentWidth, contentHeight,
              maxContentWidth, maxContentHeight, useMipmap);
        this.context = context;

        texPtr = resource.getResource().getResource();

        System.err.println("MTLTexture(): context = " + context + ", resource = " + resource + ", format = " + format + ", wrapMode = " + wrapMode + ", physicalWidth = " + physicalWidth + ", physicalHeight = " + physicalHeight + ", contentX = " + contentX + ", contentY = " + contentY + ", contentWidth = " + contentWidth + ", contentHeight = " + contentHeight + ", maxContentWidth = " + maxContentWidth + ", maxContentHeight = " + maxContentHeight + ", useMipmap = " + useMipmap);
    }

    public long getNativeHandle() {
        return texPtr;
    }

    public MTLContext getContext() {
        return context;
    }

    @Override
    protected Texture createSharedTexture(WrapMode newMode) {
        // TODO: MTL: Complete implementation
        return null;
    }

    native private static void nUpdate(long contextHandle, long pResource,
                                       byte[] pixels,
                                       int dstx, int dsty, int srcx, int srcy, int w, int h, int stride);


    @Override
    public void update(Buffer buffer, PixelFormat format, int dstx, int dsty, int srcx, int srcy, int srcw, int srch, int srcscan, boolean skipFlush) {

        // ------------------------------------------------------------------------------
        // TODO: MTL: Copy according to PixelFormat - this works for BGRA & RGB formats as of now
        // ------------------------------------------------------------------------------

        ByteBuffer buf = (ByteBuffer)buffer;
        byte[] arr = buf.hasArray()? buf.array(): null;

        if (arr == null) {
            arr = new byte[buf.remaining()];
            buf.get(arr);
        }

        // TODO: MTL: Implement cases for other PixelFormats
        switch(format) {
            case BYTE_RGB: {
                    // Metal does not support 24-bit format
                    // hence `arr` data needs to be converted to BGRA format that
                    // the native metal texture expects
                    byte[] arr32Bit = new byte[srcw * srch * 4];
                    int dstIndex = 0;
                    int index = 0;

                    final int rowStride = srcw * 3;
                    final int totalBytes = srch * rowStride;

                    for (int rowIndex = 0; rowIndex < totalBytes; rowIndex += rowStride) {
                        for (int colIndex = 0; colIndex < rowStride; colIndex += 3) {
                            index = rowIndex + colIndex;
                            arr32Bit[dstIndex++] = arr[index+2];
                            arr32Bit[dstIndex++] = arr[index+1];
                            arr32Bit[dstIndex++] = arr[index];
                            arr32Bit[dstIndex++] = (byte)255;
                        }
                    }

                    nUpdate(this.context.getContextHandle(), /*MetalTexture*/this.getNativeHandle(), arr32Bit, dstx, dsty, srcx, srcy, srcw, srch, srcw*4);
                }
                break;

            case BYTE_GRAY:
                    // Suitable 8-bit native formats are MTLPixelFormatA8Unorm & MTLPixelFormatR8Unorm.
                    // These formats do not work well with our generated shader - Texture_RGB.
                    // hence `arr` data is converted to BGRA format here.
                    //
                    // In future, if needed for performance reason:
                    // Texture_RGB shader can be tweaked to fill up R,G,B fields from single byte grayscale value.
                    // Care must be taken not to break current behavior of this shader.
                    byte[] arr32Bit = new byte[srcw * srch * 4];
                    int dstIndex = 0;
                    int index = 0;

                    final int totalBytes = srch * srcw;

                    for (int rowIndex = 0; rowIndex < totalBytes; rowIndex += srcw) {
                        for (int colIndex = 0; colIndex < srcw; colIndex++) {
                            index = rowIndex + colIndex;
                            arr32Bit[dstIndex++] = arr[index];
                            arr32Bit[dstIndex++] = arr[index];
                            arr32Bit[dstIndex++] = arr[index];
                            arr32Bit[dstIndex++] = (byte)255;
                        }
                    }

                    nUpdate(this.context.getContextHandle(), /*MetalTexture*/this.getNativeHandle(), arr32Bit, dstx, dsty, srcx, srcy, srcw, srch, srcw*4);
                break;

            default:
                // assume that `arr` data is in BGRA format
                nUpdate(this.context.getContextHandle(), /*MetalTexture*/this.getNativeHandle(), arr, dstx, dsty, srcx, srcy, srcw, srch, srcscan);
                break;
        }
    }

    @Override
    public void update(MediaFrame frame, boolean skipFlush) {
        // TODO: MTL: Complete implementation
    }
}
