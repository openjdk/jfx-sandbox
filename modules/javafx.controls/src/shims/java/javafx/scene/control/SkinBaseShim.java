/*
 * Copyright (c) 2012, 2021, Oracle and/or its affiliates. All rights reserved.
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

package javafx.scene.control;

import java.util.List;
import java.util.function.Consumer;

import javafx.beans.Observable;
import javafx.beans.value.ObservableValue;
import javafx.collections.ListChangeListener.Change;
import javafx.collections.ObservableList;
import javafx.scene.Node;

public class SkinBaseShim<C extends Control> extends SkinBase<C> {

    public SkinBaseShim(final C control) {
        super(control);
    }

    //--------------- statics --------------

    public static List<Node> getChildren(SkinBase<?> skin) {
        return skin.getChildren();
    }

//-------------- access listener api

    public static Consumer<ObservableValue<?>> unregisterChangeListeners(SkinBase<?> skin, ObservableValue<?> ov) {
        return skin.unregisterChangeListeners(ov);
    }

    public static Consumer<Observable> unregisterInvalidationListeners(SkinBase<?> skin, Observable ov) {
        return skin.unregisterInvalidationListeners(ov);
    }

    public static Consumer<Change<?>> unregisterListChangeListeners(SkinBase<?> skin, ObservableList<?> ov) {
        return skin.unregisterListChangeListeners(ov);
    }

}
