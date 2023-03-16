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

import com.sun.glass.ui.Screen;
import com.sun.prism.*;

import java.nio.Buffer;

public class MTLRTTexture extends MTLTexture<MTLTextureData> implements RTTexture {
    private int[] pixels;
    private int rttWidth;
    private int rttHeight;
    private long nTexPtr;

    private boolean opaque;

    private MTLRTTexture(MTLContext context, MTLTextureResource<MTLTextureData> resource,
                         WrapMode wrapMode,
                         int physicalWidth, int physicalHeight,
                         int contentX, int contentY,
                         int contentWidth, int contentHeight,
                         int maxContentWidth, int maxContentHeight) {

        super(context, resource, PixelFormat.BYTE_BGRA_PRE, wrapMode,
                physicalWidth, physicalHeight,
                contentX, contentY,
                contentWidth, contentHeight,
                maxContentWidth, maxContentHeight, false);
        rttWidth = contentWidth;
        rttHeight = contentHeight;
        pixels = new int[rttWidth * rttHeight];
        nTexPtr = resource.getResource().getResource();
        opaque = false;

        MTLLog.Debug("MTLRTTexture(): context = " + context + ", resource = " + resource +
                ", wrapMode = " + wrapMode +
                ", physicalWidth = " + physicalWidth + ", physicalHeight = " + physicalHeight +
                ", contentX = " + contentX + ", contentY = " + contentY +
                ", contentWidth = " + contentWidth + ", contentHeight = " + contentHeight +
                ", maxContentWidth = " + maxContentWidth + ", maxContentHeight = " + maxContentHeight);

    }

    static MTLRTTexture create(MTLContext context,
                               int physicalWidth, int physicalHeight,
                               int contentWidth, int contentHeight,
                               WrapMode wrapMode, boolean msaa) {
        MTLLog.Debug("MTLRTTexture.create()  physicalWidth = " + physicalWidth +
                ", physicalHeight = " + physicalHeight + ", contentWidth = " + contentWidth +
                ", contentHeight = " + contentHeight + ", wrapMode = " + wrapMode + ", msaa = " + msaa);
        long nPtr = nCreateRT(context.getContextHandle(),
                physicalWidth, physicalHeight,
                contentWidth, contentHeight,
                wrapMode, msaa);
        MTLTextureData textData = new MTLRTTextureData(context, nPtr);
        MTLTextureResource resource = new MTLTextureResource(textData);
        return new MTLRTTexture(context, resource, wrapMode,
                physicalWidth, physicalHeight,
                0, 0,
                contentWidth, contentHeight,
                contentWidth, contentHeight);
    }

    public long getNativeHandle() {
        return nTexPtr;
    }

    @Override
    public int getMaxContentWidth() {
        // TODO: MTL: Complete implementation
        // This value should be fetched from this Texture object
        return 8192;
    }

    @Override
    public int getMaxContentHeight() {
        // TODO: MTL: Complete implementation
        // This value should be fetched from this Texture object
        return 8192;
    }

    @Override
    public void setContentWidth(int contentWidth) {
        // TODO: MTL: Complete implementation or remove to use super method
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public void setContentHeight(int contentHeight) {
        // TODO: MTL: Complete implementation or remove to use super method
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public boolean getUseMipmap() {
        // TODO: MTL: Complete implementation or remove to use super method
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public Texture getSharedTexture(WrapMode altMode) {
        // TODO: MTL: Complete implementation or remove to use super method
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public void setLinearFiltering(boolean linear) {
        // TODO: MTL: Complete implementation or remove to use super method
        throw new UnsupportedOperationException("Not implemented");
    }

    native private static long nCreateRT(long context, int pw, int ph, int cw, int ch,
                                         WrapMode wrapMode, boolean msaa);
    native private static void nReadPixels(long nativeHandle, int[] pixBuffer);
    native private static void nReadPixelsFromContextRTT(long nativeHandle, int[] pixBuffer);
    native private static long nGetPixelDataPtr(long nativeHandle);

    @Override
    public int[] getPixels() {
        MTLLog.Debug("MTLRTTexture.getPixels()");
        nReadPixelsFromContextRTT(nTexPtr, pixels);
        return pixels;
    }

    @Override
    public boolean readPixels(Buffer pixels) {
        // TODO: MTL: Complete implementation or remove to use super method
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public boolean readPixels(Buffer pixels, int x, int y, int width, int height) {
        // TODO: MTL: Complete implementation or remove to use super method
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public boolean isVolatile() {
        // TODO: MTL: Complete implementation or remove to use super method
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public Screen getAssociatedScreen() {
        // TODO: MTL: Complete implementation or remove to use super method
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public Graphics createGraphics() {
        return MTLGraphics.create(getContext(), this);
    }

    @Override
    public boolean isOpaque() {
        return opaque;
    }

    @Override
    public void setOpaque(boolean opaque) {
        this.opaque = opaque;
    }

    @Override
    public boolean isMSAA() {
        return false;
    }

    @Override
    public void update(Image img) {
        throw new UnsupportedOperationException("update() not supported for RTTextures");
    }

    @Override
    public void update(Image img, int dstx, int dsty) {
        throw new UnsupportedOperationException("update() not supported for RTTextures");
    }

    @Override
    public void update(Image img, int dstx, int dsty, int srcw, int srch) {
        throw new UnsupportedOperationException("update() not supported for RTTextures");
    }

    @Override
    public void update(Image img, int dstx, int dsty, int srcw, int srch, boolean skipFlush) {
        throw new UnsupportedOperationException("update() not supported for RTTextures");
    }

    @Override
    public void update(Buffer buffer, PixelFormat format, int dstx, int dsty, int srcx, int srcy,
                       int srcw, int srch, int srcscan, boolean skipFlush) {
        throw new UnsupportedOperationException("update() not supported for RTTextures");
    }

    @Override
    public void update(MediaFrame frame, boolean skipFlush) {
        throw new UnsupportedOperationException("update() not supported for RTTextures");
    }
}
