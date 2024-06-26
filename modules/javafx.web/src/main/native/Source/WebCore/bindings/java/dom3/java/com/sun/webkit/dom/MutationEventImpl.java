/*
 * Copyright (c) 2013, 2024, Oracle and/or its affiliates. All rights reserved.
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

package com.sun.webkit.dom;

import org.w3c.dom.Node;
import org.w3c.dom.events.MutationEvent;

public class MutationEventImpl extends EventImpl implements MutationEvent {
    MutationEventImpl(long peer) {
        super(peer);
    }

    static MutationEvent getImpl(long peer) {
        return (MutationEvent)create(peer);
    }


// Constants
    public static final int MODIFICATION = 1;
    public static final int ADDITION = 2;
    public static final int REMOVAL = 3;

// Attributes
    @Override
    public Node getRelatedNode() {
        return NodeImpl.getImpl(getRelatedNodeImpl(getPeer()));
    }
    native static long getRelatedNodeImpl(long peer);

    @Override
    public String getPrevValue() {
        return getPrevValueImpl(getPeer());
    }
    native static String getPrevValueImpl(long peer);

    @Override
    public String getNewValue() {
        return getNewValueImpl(getPeer());
    }
    native static String getNewValueImpl(long peer);

    @Override
    public String getAttrName() {
        return getAttrNameImpl(getPeer());
    }
    native static String getAttrNameImpl(long peer);

    @Override
    public short getAttrChange() {
        return getAttrChangeImpl(getPeer());
    }
    native static short getAttrChangeImpl(long peer);


// Functions
    @Override
    public void initMutationEvent(String type
        , boolean canBubble
        , boolean cancelable
        , Node relatedNode
        , String prevValue
        , String newValue
        , String attrName
        , short attrChange)
    {
        initMutationEventImpl(getPeer()
            , type
            , canBubble
            , cancelable
            , NodeImpl.getPeer(relatedNode)
            , prevValue
            , newValue
            , attrName
            , attrChange);
    }
    native static void initMutationEventImpl(long peer
        , String type
        , boolean canBubble
        , boolean cancelable
        , long relatedNode
        , String prevValue
        , String newValue
        , String attrName
        , short attrChange);


}

