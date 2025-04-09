/*
 * Copyright (c) 2021, 2025, Oracle and/or its affiliates. All rights reserved.
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
import com.sun.glass.utils.NativeLibLoader;
import com.sun.prism.GraphicsPipeline;
import com.sun.prism.ResourceFactory;
import com.sun.prism.impl.PrismSettings;

import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.List;
import java.util.HashMap;

public class MTLPipeline extends GraphicsPipeline {

    private static MTLPipeline theInstance;
    private static MTLResourceFactory mtlResourceFactory;

    static {
        String libName = "prism_mtl";

        if (PrismSettings.verbose) {
            MTLLog.Debug("Loading native metal library, named: " + libName);
        }
        NativeLibLoader.loadLibrary(libName);
        if (PrismSettings.verbose) {
            MTLLog.Debug("Succeeded: Loading native metal library.");
        }
        theInstance = new MTLPipeline();
    }

    private MTLPipeline() {
    }

    public static MTLPipeline getInstance() {
        MTLLog.Debug("MTLPipeline.getInstance()");
        return theInstance;
    }

    @Override
    public boolean init() {
        MTLLog.Debug("MTLPipeline.init()");
        HashMap devDetails = new HashMap();
        setDeviceDetails(devDetails);
        return true;
    }

    @Override
    public int getAdapterOrdinal(Screen screen) {
        return 0;
    }

    @Override
    public ResourceFactory getDefaultResourceFactory(List<Screen> screens) {
        // TODO: MTL: This creates only one resource factory for the main screen.
        //  We need to create and maintain multiple Resource Factories, one for each screen.
        return getResourceFactory(Screen.getMainScreen());
    }

    @Override
    public ResourceFactory getResourceFactory(Screen screen) {
        // TODO: MTL: This method should return appropriate resource factory for the screen
        //  and not just the member mtlResourceFactory
        if (mtlResourceFactory == null) {
            mtlResourceFactory = new MTLResourceFactory(screen);

            // This enables sharing of MTLCommandQueue between PRISM and GLASS
            HashMap devDetails = (HashMap) MTLPipeline.
                getInstance().getDeviceDetails();
            devDetails.put("mtlCommandQueue",
                mtlResourceFactory.getContext().getMetalCommandQueue());
        }
        return mtlResourceFactory;
    }

    @Override
    public void dispose() {
        if (mtlResourceFactory != null) {
            mtlResourceFactory.dispose();
            mtlResourceFactory = null;
        }

        super.dispose();
    }

    @Override
    public boolean is3DSupported() {
        return true;
    }

    @Override
    public final boolean isMSAASupported() {
        return true;
    }

    @Override
    public boolean isVsyncSupported() {
        return true;
    }

    @Override
    public boolean supportsShaderType(ShaderType type) {
        switch (type) {
            case MSL: // Metal Shading Language
                return true;
            default:
                return false;
        }
    }

    @Override
    public boolean supportsShaderModel(ShaderModel model) {
        switch (model) {
            case SM3:
                return true;
            default:
                return false;
        }
    }
}
