/*
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
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

package test.robot.javafx.stage;

import javafx.application.Platform;
import javafx.scene.Scene;
import javafx.scene.layout.Pane;
import javafx.scene.paint.Color;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import org.junit.jupiter.params.provider.MethodSource;
import test.robot.testharness.VisualTestBase;
import test.util.Util;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.stream.IntStream;
import java.util.stream.Stream;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static test.util.Util.PARAMETERIZED_TEST_DISPLAY;
import static test.util.Util.TIMEOUT;

public class StageStatesTest extends VisualTestBase {

    private static final int WIDTH = 400;
    private static final int HEIGHT = 400;

    private static final Color BOTTOM_COLOR = Color.LIME;
    private static final Color TOP_COLOR = Color.RED;

    private static final double TOLERANCE = 0.07;

    private Stage bottomStage;
    private Scene topScene;
    private Stage topStage;

    private void setupStages(boolean overlayed, boolean topShown, StageStyle topStageStyle)
            throws InterruptedException {
        final CountDownLatch bottomShownLatch = new CountDownLatch(1);
        final CountDownLatch topShownLatch = new CountDownLatch(1);

        runAndWait(() -> {
            bottomStage = getStage(false);
            bottomStage.initStyle(StageStyle.DECORATED);
            Scene bottomScene = new Scene(new Pane(), WIDTH, HEIGHT);
            bottomScene.setFill(BOTTOM_COLOR);
            bottomStage.setScene(bottomScene);
            bottomStage.setX(0);
            bottomStage.setY(0);
            bottomStage.setOnShown(e -> Platform.runLater(bottomShownLatch::countDown));
            bottomStage.show();
        });

        assertTrue(bottomShownLatch.await(TIMEOUT, TimeUnit.MILLISECONDS),
                "Timeout waiting for bottom stage to be shown");

        runAndWait(() -> {
            topStage = getStage(true);
            topStage.initStyle(topStageStyle);
            topScene = new Scene(new Pane(), WIDTH, HEIGHT);
            topScene.setFill(TOP_COLOR);
            topStage.setScene(topScene);
            if (overlayed) {
                topStage.setX(0);
                topStage.setY(0);
            } else {
                topStage.setX(WIDTH);
                topStage.setY(HEIGHT);
            }
            if (topShown) {
                topStage.setOnShown(e -> Platform.runLater(topShownLatch::countDown));
                topStage.show();
            }
        });

        if (topShown) {
            assertTrue(topShownLatch.await(TIMEOUT, TimeUnit.MILLISECONDS),
                    "Timeout waiting for top stage to be shown");
        }

        sleep(500);
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED"})
    public void testIconifiedStage(StageStyle stageStyle) throws InterruptedException {
        setupStages(true, true, stageStyle);

        runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(TOP_COLOR, color, TOLERANCE);

            topStage.setIconified(true);
        });

        // wait a bit to let window system animate the change
        sleep(500);

        runAndWait(() -> {
            assertTrue(topStage.isIconified());
            Color color = getColor(200, 200);
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);
        });
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED"})
    public void testMaximizedStage(StageStyle stageStyle) throws InterruptedException {
        setupStages(false, true, stageStyle);

        runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

            topStage.setMaximized(true);
        });

        // wait a bit to let window system animate the change
        sleep(500);

        runAndWait(() -> {
            assertTrue(topStage.isMaximized());

            // maximized stage should take over the bottom stage
            Color color = getColor(200, 200);
            assertColorEquals(TOP_COLOR, color, TOLERANCE);
        });
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED"})
    public void testFullScreenStage(StageStyle stageStyle) throws InterruptedException {
        setupStages(false, true, stageStyle);

        runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

            topStage.setFullScreen(true);
        });

        // wait a bit to let window system animate the change
        sleep(500);

        int maxX = (int) Util.getScreen(topStage).getVisualBounds().getMaxX() - 1;
        int maxY = (int) Util.getScreen(topStage).getVisualBounds().getMaxY() - 1;

        runAndWait(() -> {
            assertTrue(topStage.isFullScreen());

            // fullscreen stage should take over the bottom stage
            Color color = getColor(200, 200);
            assertColorEquals(TOP_COLOR, color, TOLERANCE);

            color = getColor(maxX, maxY);
            assertColorEquals(TOP_COLOR, color, TOLERANCE);
        });

        // wait a little bit between getColor() calls - on macOS the below one
        // would fail without this wait
        sleep(300);

        runAndWait(() -> {
            // top left corner (plus some tolerance) should NOT show decorations
            Color color = getColor((int) topStage.getX() + 5, (int) topStage.getY() + 5);
            assertColorEquals(TOP_COLOR, color, TOLERANCE);
        });
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED"})
    public void testIconifiedStageBeforeShow(StageStyle stageStyle) throws InterruptedException {
        setupStages(true, false, stageStyle);

        runAndWait(() -> {
            Color color = getColor(200, 200);
            // top stage was not shown yet in this case, but the bottom stage should be ready
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

            topStage.setIconified(true);
            topStage.show();
        });

        // wait a bit to let window system animate the change
        sleep(500);

        runAndWait(() -> {
            assertTrue(topStage.isIconified());

            // bottom stage should still be visible
            Color color = getColor(200, 200);
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);
        });
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED"})
    public void testMaximizedStageBeforeShow(StageStyle stageStyle) throws InterruptedException {
        setupStages(false, false, stageStyle);

        runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

            topStage.setMaximized(true);
            topStage.show();
        });

        // wait a bit to let window system animate the change
        sleep(500);

        runAndWait(() -> {
            assertTrue(topStage.isMaximized());

            // maximized stage should take over the bottom stage
            Color color = getColor(200, 200);
            assertColorEquals(TOP_COLOR, color, TOLERANCE);
        });
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED"})
    public void testFullScreenStageBeforeShow(StageStyle stageStyle) throws InterruptedException {
        setupStages(false, false, stageStyle);

        runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

            topStage.setFullScreen(true);
            topStage.show();
        });

        // wait a bit to let window system animate the change
        sleep(500);

        int maxX = (int) Util.getScreen(topStage).getVisualBounds().getMaxX() - 1;
        int maxY = (int) Util.getScreen(topStage).getVisualBounds().getMaxY() - 1;

        runAndWait(() -> {
            assertTrue(topStage.isFullScreen());

            // fullscreen stage should take over the bottom stage
            Color color = getColor(200, 200);
            assertColorEquals(TOP_COLOR, color, TOLERANCE);

            color = getColor(maxX, maxY);
            assertColorEquals(TOP_COLOR, color, TOLERANCE);
        });

        runAndWait(() -> {
            // top left corner (plus some tolerance) should NOT show decorations
            Color color = getColor((int) topStage.getX() + 5, (int) topStage.getY() + 5);
            assertColorEquals(TOP_COLOR, color, TOLERANCE);
        });
    }

    private record StageState(StageStyle stageStyle, boolean iconified, boolean fullscreen, boolean maximized) {

    }

    static Stream<StageState> testStageStatesParamSource() {
        return Stream.of(StageStyle.DECORATED, StageStyle.UNDECORATED)
                .flatMap(style ->
                        IntStream.range(0, 8)
                                .mapToObj(i -> new StageState(
                                        style,
                                        (i & 0b001) != 0,
                                        (i & 0b010) != 0,
                                        (i & 0b100) != 0
                                ))
                );
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @MethodSource("testStageStatesParamSource")
    public void testStageStatePrecedenceOrderOnShow(StageState stageState) throws InterruptedException {
        setupStages(false, false, stageState.stageStyle());

        runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

            topStage.setMaximized(stageState.maximized());
            topStage.setFullScreen(stageState.fullscreen());
            topStage.setIconified(stageState.iconified());
            topStage.show();
        });

        assertStates(stageState.fullscreen(), stageState.iconified(), stageState.maximized());
    }


    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED"})
    public void testStagePrecendenceOrderDeiconifyUnfullscreenUnmaximize(StageStyle stageStyle)
            throws InterruptedException {
        setupStages(false, false, stageStyle);

        runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

            topStage.setMaximized(true);
            topStage.setFullScreen(true);
            topStage.setIconified(true);
            topStage.show();
        });

        Util.doTimeLine(500,
                () -> assertStatesAndColorAtPosition(true, true, true, BOTTOM_COLOR,
                        () -> topStage.setIconified(false)),
                () -> assertStatesAndColorAtPosition(true, false, true, TOP_COLOR,
                        () -> topStage.setFullScreen(false)),
                () -> assertStatesAndColorAtPosition(false, false, true, TOP_COLOR,
                        () -> topStage.setMaximized(false)),
                () -> assertStatesAndColorAtPosition(false, false, false, BOTTOM_COLOR,
                        null));
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED"})
    public void testStagePrecendenceOrderDeiconifyUnmaximizeUnfullscreen(StageStyle stageStyle)
            throws InterruptedException {
        setupStages(false, false, stageStyle);

        runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

            topStage.setMaximized(true);
            topStage.setFullScreen(true);
            topStage.setIconified(true);
            topStage.show();
        });

        Util.doTimeLine(500,
                () -> assertStatesAndColorAtPosition(true, true, true, BOTTOM_COLOR,
                        () -> topStage.setIconified(false)),
                () -> assertStatesAndColorAtPosition(true, false, true, TOP_COLOR,
                        () -> topStage.setMaximized(false)),
                () -> assertStatesAndColorAtPosition(true, false, true, TOP_COLOR,
                        () -> topStage.setFullScreen(false)),
                () -> assertStatesAndColorAtPosition(false, false, false, BOTTOM_COLOR,
                        null));
    }


    private void assertStatesAndColorAtPosition(boolean fullScreen, boolean iconified, boolean maximized,
                                                Color expectedColor, Runnable runAndWait) {
        assertStates(fullScreen, iconified, maximized);
        Util.runAndWait(() -> {
            Color currentColor = getColor(200, 200);
            assertColorEquals(expectedColor, currentColor, TOLERANCE);
        });

        if (runAndWait != null) {
            Util.runAndWait(runAndWait);
        }
    }

    private void assertStates(boolean fullScreen, boolean iconified, boolean maximized) {
        assertEquals(fullScreen, topStage.isFullScreen());
        assertEquals(iconified, topStage.isIconified());
        assertEquals(maximized, topStage.isMaximized());
    }
}
