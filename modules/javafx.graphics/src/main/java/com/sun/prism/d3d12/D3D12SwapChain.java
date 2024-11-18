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

package com.sun.prism.d3d12;

import com.sun.glass.ui.Screen;
import com.sun.javafx.geom.Rectangle;
import com.sun.prism.Graphics;
import com.sun.prism.GraphicsResource;
import com.sun.prism.PixelFormat;
import com.sun.prism.Presentable;
import com.sun.prism.PresentableState;
import com.sun.prism.Texture.WrapMode;
import com.sun.prism.d3d12.ni.D3D12NativeSwapChain;

class D3D12SwapChain implements Presentable, GraphicsResource {

    private D3D12Context mContext;
    private D3D12RTTexture mOffscreenRTT;
    private D3D12NativeSwapChain mSwapChain;
    private PresentableState mState;
    private float mRenderScaleX;
    private float mRenderScaleY;
    private int mWidth;
    private int mHeight;
    private boolean mMSAA;

    D3D12SwapChain(D3D12Context context, PresentableState state) {
        if (!context.getDevice().isValid()) {
            throw new NullPointerException("D3D12 device is NULL");
        }
        mContext = context;
        mState = state;
        mRenderScaleX = mState.getRenderScaleX();
        mRenderScaleY = mState.getRenderScaleY();
        mWidth = mState.getWidth();
        mHeight = mState.getHeight();
        mMSAA = mState.isMSAA();

        mSwapChain = ((D3D12ResourceFactory)context.getResourceFactory()).getNativeInstance().createSwapChain(
            mContext.getDevice(), mState.getNativeView()
        );
        if (!mSwapChain.isValid()) {
            throw new NullPointerException("D3D12 swapchain is NULL");
        }

        mOffscreenRTT = (D3D12RTTexture)context.getResourceFactory().createRTTexture(
            mSwapChain.getWidth(), mSwapChain.getHeight(), WrapMode.CLAMP_NOT_NEEDED, mMSAA
        );
        if (!mOffscreenRTT.isValid()) {
            throw new NullPointerException("D3D12 swapchain is NULL");
        }
    }

    public static D3D12SwapChain create(D3D12Context context, PresentableState state) {
        return new D3D12SwapChain(context, state);
    }

    PresentableState getPresentableState() {
        return mState;
    }

    @Override
    public Screen getAssociatedScreen() {
        return mContext.getAssociatedScreen();
    }

    @Override
    public Graphics createGraphics() {
        Graphics g = new D3D12Graphics(mContext, mOffscreenRTT);
        g.scale(mState.getRenderScaleX(), mState.getRenderScaleY());
        return g;
    }

    @Override
    public boolean isOpaque() {
        return mOffscreenRTT.isOpaque();
    }

    @Override
    public void setOpaque(boolean opaque) {
        mOffscreenRTT.setOpaque(opaque);
    }

    @Override
    public boolean isMSAA() {
        return mOffscreenRTT != null ? mOffscreenRTT.isMSAA() : false;
    }

    @Override
    public int getPhysicalWidth() {
        return mState.getOutputWidth();
    }

    @Override
    public int getPhysicalHeight() {
        return mState.getOutputHeight();
    }

    @Override
    public int getContentX() {
        return (int) (mState.getWindowX() * mState.getOutputScaleX());
    }

    @Override
    public int getContentY() {
        return (int) (mState.getWindowY() * mState.getOutputScaleY());
    }

    @Override
    public int getContentWidth() {
        return mState.getOutputWidth();
    }

    @Override
    public int getContentHeight() {
        return mState.getOutputHeight();
    }

    @Override
    public boolean lockResources(PresentableState pState) {
        if (mState != pState ||
            mRenderScaleX != pState.getRenderScaleX() ||
            mRenderScaleY != pState.getRenderScaleY()) {
            return true;
        }

        mOffscreenRTT.lock();

        if (mWidth != pState.getRenderWidth() || mHeight != pState.getRenderHeight()) {
            mWidth = pState.getRenderWidth();
            mHeight = pState.getRenderHeight();
            if (!mSwapChain.resize(mWidth, mHeight)) {
                // simple resize failed, ask to recreate the presentable
                return true;
            }
            if (!mOffscreenRTT.resize(mSwapChain.getWidth(), mSwapChain.getHeight())) {
                return true;
            }
        }

        return false;
    }

    @Override
    public boolean prepare(Rectangle dirtyregion) {
        mContext.flushVertexBuffer();

        if (mOffscreenRTT.isMSAA()) {
            mContext.getDevice().resolveToSwapChain(mSwapChain, mOffscreenRTT.getNativeTexture());
        } else {
            mContext.getDevice().copyToSwapChain(mSwapChain, mOffscreenRTT.getNativeTexture());
        }

        mOffscreenRTT.unlock();

        if (dirtyregion == null) {
            return mSwapChain.prepare(-1, -1, -1, -1);
        } else {
            return mSwapChain.prepare(dirtyregion.x, dirtyregion.y, dirtyregion.width, dirtyregion.height);
        }
    }

    @Override
    public boolean present() {
        mContext.getDevice().finishFrame();
        return mSwapChain.present();
    }

    @Override
    public float getPixelScaleFactorX() {
        return mState.getRenderScaleX();
    }

    @Override
    public float getPixelScaleFactorY() {
        return mState.getRenderScaleY();
    }

    @Override
    public void dispose() {
        mSwapChain.close();
        mOffscreenRTT.dispose();
    }
}
