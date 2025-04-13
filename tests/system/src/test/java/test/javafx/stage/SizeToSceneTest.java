/*
 * Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
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

import javafx.scene.Parent;
import javafx.scene.layout.StackPane;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import javafx.stage.Window;
import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import test.util.Util;

import java.util.function.Consumer;

import static test.util.Util.PARAMETERIZED_TEST_DISPLAY;

class SizeToSceneTest extends StageTestBase {
    private static final double STAGE_MIN_WIDTH = 300;
    private static final double STAGE_MIN_HEIGHT = 300;
    private static final double SCENE_WIDTH = 130;
    private static final double SCENE_HEIGHT = 120;
    private static final double DECORATION_DELTA = 50;

    @Override
    protected Parent createRoot() {
        StackPane root = new StackPane();
        root.setPrefSize(SCENE_WIDTH, SCENE_HEIGHT);
        return root;
    }

    @Override
    protected void setupStageWithStyle(StageStyle stageStyle, Consumer<Stage> pc) {
        Consumer<Stage> cs = stage -> {
            stage.setMinWidth(STAGE_MIN_WIDTH);
            stage.setMinHeight(STAGE_MIN_HEIGHT);
        };

        if (pc != null) {
            cs = cs.andThen(pc);
        }

        super.setupStageWithStyle(stageStyle, cs);
    }

    private double getDelta(StageStyle stageStyle) {
        if (stageStyle == StageStyle.DECORATED || stageStyle == StageStyle.UTILITY) {
            return DECORATION_DELTA;
        }

        return 0;
    }

    private void assertWindowSizeMatchesToScene(StageStyle stageStyle) {
        Assertions.assertEquals(SCENE_WIDTH, getStage().getWidth(), getDelta(stageStyle));
        Assertions.assertEquals(SCENE_HEIGHT, getStage().getHeight(), getDelta(stageStyle));
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
                mode = EnumSource.Mode.INCLUDE,
                names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnMaximizedThenSizeToScene(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, s -> {
            s.setMaximized(true);
            s.sizeToScene();
            s.setMaximized(false);
        });
        Util.sleep(500);
        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnFullscreenThenSizeToScene(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, s -> {
            s.setFullScreen(true);
            s.sizeToScene();
            s.setFullScreen(false);
        });
        Util.sleep(500);
        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnSizeToSceneThenMaximized(StageStyle stageStyle) throws InterruptedException {
        setupStageWithStyle(stageStyle, Window::sizeToScene);
        Util.doTimeLine(500,
                () -> getStage().setMaximized(true),
                () -> getStage().setMaximized(false));

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnSizeToSceneThenFullscreen(StageStyle stageStyle) throws InterruptedException {
        setupStageWithStyle(stageStyle, Window::sizeToScene);
        Util.doTimeLine(500,
                () -> getStage().setFullScreen(true),
                () -> getStage().setFullScreen(false));
        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeAfterShowSizeToSceneThenFullscreen(StageStyle stageStyle) throws InterruptedException {
        setupStageWithStyle(stageStyle, null);
        Util.doTimeLine(500,
                () -> getStage().sizeToScene(),
                () -> getStage().setFullScreen(true),
                () -> getStage().setFullScreen(false));
        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeAfterShowSizeToSceneThenMaximized(StageStyle stageStyle) throws InterruptedException {
        setupStageWithStyle(stageStyle, null);
        Util.doTimeLine(500,
                () -> getStage().sizeToScene(),
                () -> getStage().setMaximized(true),
                () -> getStage().setMaximized(false));
        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeAfterShowFullscreenThenSizeToScene(StageStyle stageStyle) throws InterruptedException {
        setupStageWithStyle(stageStyle, null);
        Util.doTimeLine(500,
                () -> getStage().setFullScreen(true),
                () -> getStage().sizeToScene(),
                () -> getStage().setFullScreen(false));
        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeAfterShowMaximizedThenSizeToScene(StageStyle stageStyle) throws InterruptedException {
        setupStageWithStyle(stageStyle, null);

        Util.doTimeLine(500,
                () -> getStage().setMaximized(true),
                () -> getStage().sizeToScene(),
                () -> getStage().setMaximized(false));
        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnSizeToScene(StageStyle stageStyle) throws InterruptedException {
        setupStageWithStyle(stageStyle, Window::sizeToScene);
        Util.sleep(500);
        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeSizeToSceneMaximizedOnOff(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, s -> {
            s.sizeToScene();
            s.setMaximized(true);
            s.setMaximized(false);
        });

        Util.sleep(500);
        assertWindowSizeMatchesToScene(stageStyle);
    }
}
