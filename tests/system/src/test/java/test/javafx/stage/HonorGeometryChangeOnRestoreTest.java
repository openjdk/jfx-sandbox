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
package test.javafx.stage;

import javafx.stage.StageStyle;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import test.util.Util;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static test.util.Util.PARAMETERIZED_TEST_DISPLAY;

public class HonorGeometryChangeOnRestoreTest extends StageTestBase {
    private static final int POS_X = 100;
    private static final int POS_Y = 150;
    private static final int WIDTH = 100;
    private static final int HEIGHT = 150;

    private static final int SHOW_WIDTH = 500;
    private static final int SHOW_HEIGHT = 500;
    private static final int SHOW_X = 500;
    private static final int SHOW_Y = 500;

    private void setupStageWithStyle(StageStyle style) {
        super.setupStageWithStyle(style, s -> {
            s.setWidth(SHOW_WIDTH);
            s.setHeight(SHOW_HEIGHT);
            s.setX(SHOW_X);
            s.setY(SHOW_Y);
        });
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testUnFullscreenChangedPosition(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle);

        Util.doTimeLine(500,
                () -> getStage().setFullScreen(true),
                () -> {
                    getStage().setX(POS_X);
                    getStage().setY(POS_Y);
                },
                () -> getStage().setFullScreen(false));

        assertEquals(POS_X, getStage().getX(), "Window failed to restore position set while fullscreened");
        assertEquals(POS_Y, getStage().getY(),  "Window failed to restore position set while fullscreened");
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testUnFullscreenChangedSize(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle);

        Util.doTimeLine(500,
                () -> getStage().setFullScreen(true),
                () -> {
                    getStage().setWidth(WIDTH);
                    getStage().setHeight(HEIGHT);
                },
                () -> getStage().setFullScreen(false));

        assertEquals(WIDTH, getStage().getWidth(), "Window failed to restore size set while fullscreened");
        assertEquals(HEIGHT, getStage().getHeight(),  "Window failed to restore size set while fullscreened");
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testUnMaximzeChangedPosition(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle);

        Util.doTimeLine(500,
                () -> getStage().setMaximized(true),
                () -> {
                    getStage().setX(POS_X);
                    getStage().setY(POS_Y);
                },
                () -> getStage().setMaximized(false));

        assertEquals(POS_X, getStage().getX(), "Window failed to restore position set while maximized");
        assertEquals(POS_Y, getStage().getY(),  "Window failed to restore position set while maximized");
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testUnMaximizeChangedSize(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle);

        Util.doTimeLine(500,
                () -> getStage().setMaximized(true),
                () -> {
                    getStage().setWidth(WIDTH);
                    getStage().setHeight(HEIGHT);
                },
                () -> getStage().setMaximized(false));

        assertEquals(WIDTH, getStage().getWidth(), "Window failed to restore size set while maximized");
        assertEquals(HEIGHT, getStage().getHeight(),  "Window failed to restore size set while maximized");
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testDeIconfyChangedPosition(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle);

        Util.doTimeLine(500,
                () -> getStage().setIconified(true),
                () -> {
                    getStage().setX(POS_X);
                    getStage().setY(POS_Y);
                },
                () -> getStage().setIconified(false));

        assertEquals(POS_X, getStage().getX(), "Window failed to restore position set while iconified");
        assertEquals(POS_Y, getStage().getY(),  "Window failed to restore position set while iconified");
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testDeIconifyChangedSize(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle);

        Util.doTimeLine(500,
                () -> getStage().setIconified(true),
                () -> {
                    getStage().setWidth(WIDTH);
                    getStage().setHeight(HEIGHT);
                },
                () -> getStage().setIconified(false));

        assertEquals(WIDTH, getStage().getWidth(), "Window failed to restore size set while iconified");
        assertEquals(HEIGHT, getStage().getHeight(),  "Window failed to restore size set while iconified");
    }
}
