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
import static org.junit.jupiter.api.Assertions.assertTrue;
import static test.util.Util.PARAMETERIZED_TEST_DISPLAY;

public class ResizeUnresizableTest extends StageTestBase {
    private static final int WIDTH = 300;
    private static final int HEIGHT = 300;

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT", "UTILITY"})
    public void testMaximizeUnresizable(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, s -> {
            s.initStyle(stageStyle);
            s.setWidth(WIDTH);
            s.setHeight(HEIGHT);
            s.setResizable(false);
        });
        Util.runAndWait(() -> getStage().setMaximized(true));
        Util.sleep(500);
        assertTrue(getStage().isMaximized(), "Unresizable stage should be maximized");
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT", "UTILITY"})
    public void testFullscreenUnresizable(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, s -> {
            s.initStyle(stageStyle);
            s.setWidth(WIDTH);
            s.setHeight(HEIGHT);
            s.setResizable(false);
        });

        Util.runAndWait(() -> getStage().setFullScreen(true));
        Util.sleep(500);
        assertTrue(getStage().isFullScreen(), "Unresizable stage should be fullscreen");
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT", "UTILITY"})
    public void testMaximizeMaxSize(StageStyle stageStyle) {
        int maxSize = 500;

        setupStageWithStyle(stageStyle, s -> {
            s.initStyle(stageStyle);
            s.setWidth(WIDTH);
            s.setHeight(HEIGHT);
            s.setMaxWidth(maxSize);
            s.setMaxHeight(maxSize);
        });


        Util.doTimeLine(500,
                () -> getStage().setMaximized(true),
                () -> getStage().setMaximized(false));

        assertEquals(WIDTH, getStage().getWidth(), "Stage width should have remained");
        assertEquals(HEIGHT, getStage().getHeight(), "Stage height should have remained");
        assertEquals(maxSize, getStage().getMaxWidth(), "Stage max width should have remained");
        assertEquals(maxSize, getStage().getMaxHeight(), "Stage max height should have remained");
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT", "UTILITY"})
    public void testMaxSizeShouldBeProgramaticallyResized(StageStyle stageStyle) {
        int maxSize = 300;
        int newSize = 500;

        setupStageWithStyle(stageStyle, s -> {
            s.initStyle(stageStyle);
            s.setMaxWidth(maxSize);
            s.setMaxHeight(maxSize);
        });

        Util.sleep(500);

        Util.runAndWait(() -> {
            getStage().setWidth(newSize);
            getStage().setHeight(newSize);
        });

        Util.sleep(500);

        assertEquals(newSize, getStage().getWidth(), "Stage width should be programatically resized beyond max width");
        assertEquals(newSize, getStage().getHeight(), "Stage height should be programatically resized beyond max height");
        assertEquals(maxSize, getStage().getMaxWidth(), "Stage max width should have remained");
        assertEquals(maxSize, getStage().getMaxHeight(), "Stage max height should have remained");
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT", "UTILITY"})
    public void testMinSizeShouldBeProgramaticallyResized(StageStyle stageStyle) {
        int minSize = 700;
        int newSize = 500;

        setupStageWithStyle(stageStyle, s -> {
            s.initStyle(stageStyle);
            s.setMinWidth(minSize);
            s.setMinHeight(minSize);
        });

        Util.sleep(500);

        Util.runAndWait(() -> {
            getStage().setWidth(newSize);
            getStage().setHeight(newSize);
        });

        Util.sleep(500);

        assertEquals(newSize, getStage().getWidth(), "Stage width should be programatically resized beyond max width");
        assertEquals(newSize, getStage().getHeight(), "Stage height should be programatically resized beyond max height");
        assertEquals(minSize, getStage().getMinWidth(), "Stage min width should have remained");
        assertEquals(minSize, getStage().getMinHeight(), "Stage min height should have remained");
    }
}
