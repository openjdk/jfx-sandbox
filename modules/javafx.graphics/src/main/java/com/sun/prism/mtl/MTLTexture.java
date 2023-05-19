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
import java.nio.IntBuffer;
import java.nio.FloatBuffer;

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

        MTLLog.Debug("MTLTexture(): context = " + context + ", resource = " + resource + ", format = " + format + ", wrapMode = " + wrapMode + ", physicalWidth = " + physicalWidth + ", physicalHeight = " + physicalHeight + ", contentX = " + contentX + ", contentY = " + contentY + ", contentWidth = " + contentWidth + ", contentHeight = " + contentHeight + ", useMipmap = " + useMipmap);
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

        MTLLog.Debug("MTLTexture(): context = " + context + ", resource = " + resource + ", format = " + format + ", wrapMode = " + wrapMode + ", physicalWidth = " + physicalWidth + ", physicalHeight = " + physicalHeight + ", contentX = " + contentX + ", contentY = " + contentY + ", contentWidth = " + contentWidth + ", contentHeight = " + contentHeight + ", maxContentWidth = " + maxContentWidth + ", maxContentHeight = " + maxContentHeight + ", useMipmap = " + useMipmap);
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
    native private static void nUpdateFloat(long contextHandle, long pResource,
                                            float[] pixels,
                                            int dstx, int dsty, int srcx, int srcy, int w, int h, int stride);

    native private static void nUpdateInt(long contextHandle, long pResource,
                                            int[] pixels,
                                            int dstx, int dsty, int srcx, int srcy, int w, int h, int stride);


@Override
    public void update(Buffer buffer, PixelFormat format, int dstx, int dsty, int srcx, int srcy, int srcw, int srch, int srcscan, boolean skipFlush) {

        if (format.getDataType() == PixelFormat.DataType.INT) {
            if (format == PixelFormat.INT_ARGB_PRE) {
                IntBuffer buf = (IntBuffer)buffer;
                int[] arr = buf.hasArray() ? buf.array() : null;
                if (arr == null) {
                    arr = new int[buf.remaining()];
                    buf.get(arr);
                }

                nUpdateInt(this.context.getContextHandle(), this.getNativeHandle(), arr, dstx, dsty, srcx, srcy, srcw, srch, srcscan);
            } else {
                throw new IllegalArgumentException("Unsupported INT PixelFormat"+ format);
            }
        } else if (format.getDataType() == PixelFormat.DataType.FLOAT) {
            if (format == PixelFormat.FLOAT_XYZW) {
                MTLLog.Debug("FLOAT_XYZW - data type of buffer is : " + buffer.getClass().getName());
                MTLLog.Debug("Buffer capacity : " + buffer.capacity());
                MTLLog.Debug("Buffer limit : " + buffer.limit());
                MTLLog.Debug("srcscan  = " + srcscan);
                MTLLog.Debug("srcw  = " + srcw);
                MTLLog.Debug("srch  = " + srch);

                FloatBuffer buf = (FloatBuffer)buffer;
                float[] arr = buf.hasArray()? buf.array(): null;

                if (arr == null) {
                    arr = new float[buf.remaining()];
                    buf.get(arr);
                }

                nUpdateFloat(this.context.getContextHandle(), /*MetalTexture*/this.getNativeHandle(), arr, dstx, dsty, srcx, srcy, srcw, srch, srcscan);
            } else {
                throw new IllegalArgumentException("Unsupported FLOAT PixelFormat"+ format);
            }
        } else if (format.getDataType() == PixelFormat.DataType.BYTE) {
            ByteBuffer buf = (ByteBuffer)buffer;
            byte[] arr = buf.hasArray()? buf.array(): null;

            if (arr == null) {
                arr = new byte[buf.remaining()];
                buf.get(arr);
            }

            if (format == PixelFormat.BYTE_BGRA_PRE || format == PixelFormat.BYTE_ALPHA) {
                nUpdate(this.context.getContextHandle(), /*MetalTexture*/this.getNativeHandle(), arr, dstx, dsty, srcx, srcy, srcw, srch, srcscan);
            } else if (format == PixelFormat.BYTE_RGB) {
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
            } else if (format == PixelFormat.BYTE_GRAY) {
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
            } else if (format == PixelFormat.MULTI_YCbCr_420 || format == PixelFormat.BYTE_APPLE_422) {
                throw new IllegalArgumentException("Format not yet supported by Metal pipeline :"+ format);
            }
        } else {
            throw new IllegalArgumentException("Unsupported PixelFormat DataType : "+ format);
        }
    }

    @Override
    public void update(MediaFrame frame, boolean skipFlush) {

        MTLLog.Debug("MTLTexture - update for MediaFrame.");

        // TODO: MTL: Check whether we need to implement MULTI_YCbCr_420 format
        // using multi-texturing.
        if (frame.getPixelFormat() == PixelFormat.MULTI_YCbCr_420 ||
            frame.getPixelFormat() != PixelFormat.BYTE_APPLE_422) {
            // shouldn't have gotten this far
            throw new IllegalArgumentException("Unsupported format " + frame.getPixelFormat());
        }

        frame.holdFrame();

        ByteBuffer pixels = frame.getBufferForPlane(0);
        byte[] arr = pixels.hasArray()? pixels.array(): null;
        if (arr == null) {
            arr = new byte[pixels.remaining()];
            pixels.get(arr);
        }

        // Below logic reads BYTE_APPLE_422 (YUV422) data from `arr` and
        // converts it to BGRA array using standard equations in loops.
        // This can be optimised in future by implementing the logic in metal shader
        int srcIndex = 0;
        int dstIndex = 0;

        byte BGRA_arr[] = new byte[frame.getWidth() * frame.getHeight() * 4];

        for (int row = 0; row < frame.getHeight(); row++) {
            for (int col = 0; col < frame.getWidth() * 2 ; col += 4) {

                // Get the UYVY bytes
                int u  = (arr[srcIndex++] & 0xFF) - 128;
                int y1 = (arr[srcIndex++] & 0xFF);
                int v  = (arr[srcIndex++] & 0xFF) - 128;
                int y2 = (arr[srcIndex++] & 0xFF);

                int compR = (int)(1.402 * v);
                int compG = (int)(0.34414 * u + 0.71414 * v);
                int compB = (int)(1.772 * u);

                // Calculate RGB for the 1st pixel
                int r = y1 + compR;
                int g = y1 - compG;
                int b = y1 + compB;

                r = (r > 255)? 255 : (r < 0)? 0 : r;
                g = (g > 255)? 255 : (g < 0)? 0 : g;
                b = (b > 255)? 255 : (b < 0)? 0 : b;

                BGRA_arr[dstIndex++] = (byte)b;
                BGRA_arr[dstIndex++] = (byte)g;
                BGRA_arr[dstIndex++] = (byte)r;
                BGRA_arr[dstIndex++] = (byte)255;


                // Calculate RGB for the 2nd pixel
                r = y2 + compR;
                g = y2 - compG;
                b = y2 + compB;

                r = (r > 255)? 255 : (r < 0)? 0 : r;
                g = (g > 255)? 255 : (g < 0)? 0 : g;
                b = (b > 255)? 255 : (b < 0)? 0 : b;

                BGRA_arr[dstIndex++] = (byte)b;
                BGRA_arr[dstIndex++] = (byte)g;
                BGRA_arr[dstIndex++] = (byte)r;
                BGRA_arr[dstIndex++] = (byte)255;
            }

            // Skip padding bytes at the end of each row
            srcIndex += frame.strideForPlane(0) - (frame.getWidth()*2);
        }

        nUpdate(this.context.getContextHandle(),
                this.getNativeHandle(),
                BGRA_arr, 0, 0, 0, 0,
                frame.getEncodedWidth(), frame.getEncodedHeight(),
                frame.getWidth() * 4);

        frame.releaseFrame();
    }
}
