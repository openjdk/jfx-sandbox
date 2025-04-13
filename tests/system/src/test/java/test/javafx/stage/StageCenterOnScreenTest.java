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
import javafx.stage.Screen;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import test.util.Util;

import java.util.Objects;
import java.util.function.Consumer;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static test.util.Util.PARAMETERIZED_TEST_DISPLAY;

public class StageCenterOnScreenTest extends StageTestBase {
    private static final float CENTER_ON_SCREEN_X_FRACTION = 1.0f / 2;
    private static final float CENTER_ON_SCREEN_Y_FRACTION = 1.0f / 3;

    private static final double STAGE_WIDTH = 400;
    private static final double STAGE_HEIGHT = 200;

    @Override
    public void setupStageWithStyle(StageStyle stageStyle, Consumer<Stage> sc) {
        Consumer<Stage> stageConsumer = (stage) -> {
            stage.setWidth(STAGE_WIDTH);
            stage.setHeight(STAGE_HEIGHT);
        };

        if (sc != null) {
            stageConsumer = stageConsumer.andThen(sc);
        }

        super.setupStageWithStyle(stageStyle, stageConsumer);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenWhenShown(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle, null);
        Util.sleep(500);
        assertStageCentered(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenAfterShown(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle, stage -> {
            stage.setX(0);
            stage.setY(0);
        });

        Util.sleep(500);
        Util.runAndWait(() -> getStage().centerOnScreen());
        Util.sleep(500);
        assertStageCentered(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenWhileFullscreen(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle, stage -> {
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

        assertStageCentered(stageStyle);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class,
            mode = EnumSource.Mode.INCLUDE,
            names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testStateCenterOnScreenWhileMaximized(StageStyle stageStyle) throws Exception {
        setupStageWithStyle(stageStyle, stage -> {
            stage.setX(0);
            stage.setY(0);
            stage.setMaximized(true);
        });

        Util.sleep(500);
        Util.doTimeLine(500,
                () -> getStage().centerOnScreen(),
                () -> assertTrue(getStage().isFullScreen(), "centerOnScreen() should not change window state"),
                () -> getStage().setMaximized(false));

        Util.sleep(500);

        assertStageCentered(stageStyle);
    }

    private void assertStageCentered(StageStyle stageStyle) {
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
                (bounds.getMinX() + (bounds.getWidth() - STAGE_WIDTH)
                        * CENTER_ON_SCREEN_X_FRACTION) - decorationX;
        double centerY =
                (bounds.getMinY() + (bounds.getHeight() - STAGE_HEIGHT)
                        * CENTER_ON_SCREEN_Y_FRACTION) - decorationY;

        assertEquals(centerX, getStage().getX(), 1, "Stage is not centered in X axis");
        assertEquals(centerY, getStage().getY(), 1, "Stage is not centered in Y axis");
    }
}
