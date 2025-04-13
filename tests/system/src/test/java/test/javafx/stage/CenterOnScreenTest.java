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

import javafx.geometry.Rectangle2D;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.layout.StackPane;
import javafx.stage.Screen;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import test.util.Util;

import java.util.function.Consumer;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static test.util.Util.PARAMETERIZED_TEST_DISPLAY;

public class CenterOnScreenTest extends StageTestBase {
    private static final float CENTER_ON_SCREEN_X_FRACTION = 1.0f / 2;
    private static final float CENTER_ON_SCREEN_Y_FRACTION = 1.0f / 3;

    private static final double STAGE_WIDTH = 400;
    private static final double STAGE_HEIGHT = 200;

    // Must be cointained in Stage dimensions
    private static final double SCENE_WIDTH = 300;
    private static final double SCENE_HEIGHT = 100;

    @Override
    protected Parent createRoot() {
        StackPane stackPane = new StackPane();
        stackPane.setPrefSize(SCENE_WIDTH, SCENE_HEIGHT);
        return stackPane;
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenWhenShown(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, stage -> {
            stage.setWidth(STAGE_WIDTH);
            stage.setHeight(STAGE_HEIGHT);
        });
        Util.sleep(500);
        assertStageCentered(stageStyle, false);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenWhenShownWithSceneSize(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, null);
        Util.sleep(500);
        assertStageCentered(stageStyle, true);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenAfterShown(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, stage -> {
            stage.setWidth(STAGE_WIDTH);
            stage.setHeight(STAGE_HEIGHT);
            stage.setX(0);
            stage.setY(0);
        });

        Util.sleep(500);
        Util.runAndWait(() -> getStage().centerOnScreen());
        Util.sleep(500);
        assertStageCentered(stageStyle, false);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenAfterShownWithSceneSize(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, stage -> {
            stage.setX(0);
            stage.setY(0);
        });

        Util.sleep(500);
        Util.runAndWait(() -> getStage().centerOnScreen());
        Util.sleep(500);
        assertStageCentered(stageStyle, true);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenWhileFullscreen(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, stage -> {
            stage.setWidth(STAGE_WIDTH);
            stage.setHeight(STAGE_HEIGHT);
            stage.setX(0);
            stage.setY(0);
            stage.setFullScreen(true);
        });

        Util.sleep(500);
        Util.doTimeLine(500,
                () -> getStage().centerOnScreen(),
                () -> assertTrue(getStage().isFullScreen(), "centerOnScreen() should not change window state"),
                () -> getStage().setFullScreen(false));

        Util.sleep(500);

        assertStageCentered(stageStyle, false);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenWhileMaximized(StageStyle stageStyle) {
        setupStageWithStyle(stageStyle, stage -> {
            stage.setX(0);
            stage.setY(0);
            stage.setWidth(STAGE_WIDTH);
            stage.setHeight(STAGE_HEIGHT);
            stage.setMaximized(true);
        });

        Util.sleep(500);
        Util.doTimeLine(500,
                () -> getStage().centerOnScreen(),
                () -> assertTrue(getStage().isFullScreen(), "centerOnScreen() should not change window state"),
                () -> getStage().setMaximized(false));

        Util.sleep(500);

        assertStageCentered(stageStyle, false);
    }

    private void assertStageCentered(StageStyle stageStyle, boolean useSceneSize) {
        Screen screen = Util.getScreen(getStage());

        double decorationY = 0;
        double decorationX = 0;

        if (stageStyle == StageStyle.DECORATED
                && !getStage().isFullScreen()
                && !getStage().isMaximized()) {
            decorationX = getStage().getScene().getX() * CENTER_ON_SCREEN_X_FRACTION;
            decorationY = getStage().getScene().getY() * CENTER_ON_SCREEN_Y_FRACTION;

            assertNotEquals(0, decorationX, "Decorated Stage's should have viewX");
            assertNotEquals(0, decorationY, "Decorated Stage's should have viewY");
        }

        Rectangle2D bounds = screen.getVisualBounds();
        double centerX =
                (bounds.getMinX() + (bounds.getWidth() - ((useSceneSize) ? SCENE_WIDTH : STAGE_WIDTH))
                        * CENTER_ON_SCREEN_X_FRACTION) - decorationX;
        double centerY =
                (bounds.getMinY() + (bounds.getHeight() - ((useSceneSize) ? SCENE_HEIGHT : STAGE_HEIGHT))
                        * CENTER_ON_SCREEN_Y_FRACTION) - decorationY;

        assertEquals(centerX, getStage().getX(), 1, "Stage is not centered in X axis");
        assertEquals(centerY, getStage().getY(), 1, "Stage is not centered in Y axis");
    }
}
