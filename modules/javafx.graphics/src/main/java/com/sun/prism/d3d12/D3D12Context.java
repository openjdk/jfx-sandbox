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
import com.sun.javafx.geom.Vec3d;
import com.sun.javafx.geom.transform.Affine3D;
import com.sun.javafx.geom.transform.BaseTransform;
import com.sun.javafx.geom.transform.GeneralTransform3D;
import com.sun.javafx.sg.prism.NGCamera;
import com.sun.javafx.sg.prism.NGDefaultCamera;
import com.sun.prism.CompositeMode;
import com.sun.prism.Graphics;
import com.sun.prism.PixelFormat;
import com.sun.prism.RTTexture;
import com.sun.prism.RenderTarget;
import com.sun.prism.Texture;
import com.sun.prism.impl.PrismSettings;
import com.sun.prism.impl.ps.BaseShaderContext;
import com.sun.prism.ps.Shader;
import com.sun.prism.d3d12.ni.D3D12NativeDevice;


class D3D12Context extends BaseShaderContext {
    public static final int NUM_QUADS = PrismSettings.superShader ? 4096 : 256;

    private D3D12NativeDevice mDevice;
    private D3D12RTTexture mCurrentRT;
    private State mState;
    private Vec3d mCameraPos = new Vec3d();
    private static final Affine3D mScratchAffine3DTx = new Affine3D();
    private GeneralTransform3D mScratchTx = new GeneralTransform3D();
    private GeneralTransform3D mViewProjTx = new GeneralTransform3D();

    D3D12Context(D3D12NativeDevice device, Screen screen, D3D12ResourceFactory factory) {
        super(screen, factory, NUM_QUADS);
        if (!device.isValid()) {
            throw new NullPointerException("D3D12 device is NULL");
        }

        mDevice = device;
        mState = new State();
    }

    D3D12NativeDevice getDevice() {
        return mDevice;
    }

    int getMSAASampleSize(PixelFormat format) {
        int maxSamples = mDevice.getMaximumMSAASampleSize(format);
        return maxSamples < 2 ? 1 : (maxSamples < 4 ? 2 : 4);
    }

    @Override
    protected State updateRenderTarget(RenderTarget target, NGCamera camera, boolean depthTest) {
        if (checkDisposed()) return null;

        mCurrentRT = (D3D12RTTexture)target;
        mDevice.setRenderTarget(mCurrentRT.getNativeRenderTarget(), depthTest);

        int w = mCurrentRT.getWidth();
        int h = mCurrentRT.getHeight();

        // Need to validate the camera before getting its computed data.
        if (camera instanceof NGDefaultCamera) {
            ((NGDefaultCamera) camera).validate(w, h);
            camera.getProjViewTx(mViewProjTx);
        } else {
            camera.getProjViewTx(mViewProjTx);
            // TODO: D3D12: verify that this is the right solution. There may be
            // other use-cases where rendering needs different viewport size.
            double vw = camera.getViewWidth();
            double vh = camera.getViewHeight();
            if (w != vw || h != vh) {
                mViewProjTx.scale(vw / w, vh / h, 1.0);
            }
        }

        // Set projection view matrix
        mDevice.setViewProjTransform(mViewProjTx);

        mCameraPos = camera.getPositionInWorld(mCameraPos);

        return mState;
    }

    @Override
    protected void updateTexture(int texUnit, Texture tex) {
        D3D12Texture texture = (D3D12Texture)tex;
        if (texture != null) {
            mDevice.setTexture(texUnit, texture.getNativeTexture());
        } else {
            mDevice.setTexture(texUnit, null);
        }
    }

    @Override
    protected void updateShaderTransform(Shader shader, BaseTransform xform) {
        if (xform == null) {
            xform = BaseTransform.IDENTITY_TRANSFORM;
        }

        final GeneralTransform3D perspectiveTransform = getPerspectiveTransformNoClone();
        mScratchTx.setIdentity();
        if (xform.isIdentity() && perspectiveTransform.isIdentity()) {
            // noop
        } else if (perspectiveTransform.isIdentity()) {
            mScratchTx.mul(xform);
        } else {
            mScratchTx.mul(xform).mul(perspectiveTransform);
        }

        // TODO: D3D12: this does not take shader into account. Maybe it should?
        mDevice.setWorldTransform(mScratchTx);
    }

    @Override
    protected void updateWorldTransform(BaseTransform xform) {
        mDevice.setWorldTransform(xform);
    }

    @Override
    protected void updateClipRect(Rectangle clipRect) {
        if (clipRect == null || clipRect.isEmpty()) {
            mDevice.setScissor(false, 0, 0, mCurrentRT.getWidth(), mCurrentRT.getHeight());
        } else {
            int x1 = clipRect.x;
            int y1 = clipRect.y;
            int x2 = x1 + clipRect.width;
            int y2 = y1 + clipRect.height;
            mDevice.setScissor(true, x1, y1, x2, y2);
        }
    }

    @Override
    protected void updateCompositeMode(CompositeMode mode) {
        mDevice.setCompositeMode(mode);
    }

    @Override
    public void blit(RTTexture srcRTT, RTTexture dstRTT, int srcX0, int srcY0, int srcX1, int srcY1,
                    int dstX0, int dstY0, int dstX1, int dstY1) {
        throw new UnsupportedOperationException("Unimplemented method 'blit'");
    }

    void renderMeshView(D3D12MeshView meshView, Graphics g) {
        // Support retina display by scaling the mViewProjTX and pass it to the shader.
        mScratchTx = mScratchTx.set(mViewProjTx);
        float pixelScaleFactorX = g.getPixelScaleFactorX();
        float pixelScaleFactorY = g.getPixelScaleFactorY();
        if (pixelScaleFactorX != 1.0 || pixelScaleFactorY != 1.0) {
            mScratchTx.scale(pixelScaleFactorX, pixelScaleFactorY, 1.0);
        }

        // Set projection view matrix
        mDevice.setViewProjTransform(mScratchTx);
        mDevice.setCameraPos(mCameraPos);

        // Undo the SwapChain scaling done in createGraphics() because 3D needs
        // this information in the shader (via projViewTx)
        BaseTransform xform = g.getTransformNoClone();
        if (pixelScaleFactorX != 1.0 || pixelScaleFactorY != 1.0) {
            mScratchAffine3DTx.setToIdentity();
            mScratchAffine3DTx.scale(1.0 / pixelScaleFactorX, 1.0 / pixelScaleFactorY);
            mScratchAffine3DTx.concatenate(xform);
            updateWorldTransform(mScratchAffine3DTx);
        } else {
            updateWorldTransform(xform);
        }

        mDevice.renderMeshView(meshView.getNativeMeshView());
    }

    @Override
    protected void renderQuads(float[] coordArray, byte[] colorArray, int numVertices) {
        mDevice.renderQuads(coordArray, colorArray, numVertices);
    }

    @Override
    public void dispose() {
        mDevice.close();
        super.dispose();
    }
}
