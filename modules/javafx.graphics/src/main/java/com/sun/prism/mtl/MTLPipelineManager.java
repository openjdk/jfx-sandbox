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

import java.util.Map;
import java.util.HashMap;

/**
 * TODO: MTL: Implement the class
 * MTLRenderPipelineState are re-usable heavy objects, so they should be reused as much possible.
 * This class is intended to manage these MTLRenderPipelineState objects by creating a map
 * of MTLRenderPipelineState objects, with fragment function name as key.
 * Each MTLRenderPipelineState created for 2D shapes would be a combination of
 * passThrough vertex function and any of the Prism or Decora fragment function.
 * 3D shaders can be handled similarly or in a different way.
 * This class may be renamed as MTLPipelineStateManager in future.
 */

public class MTLPipelineManager {
    /*
    Map<String, MTLRenderPipelineState> pipeStates = new HashMap<>();

    MTLRenderPipelineState getPipelineState(String fragmentFunctionName) {
        // if a value is present in the pipeStates map, return that value
        // else create a MTLRenderPipelineState, add it to map, and return it.
    }
    */
}
