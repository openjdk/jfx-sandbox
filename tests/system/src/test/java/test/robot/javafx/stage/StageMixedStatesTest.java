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
 * Please contact Oracle, S Oracle Parkway, Redwood Shores, CA 94065 USA
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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.stream.Stream;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static test.util.Util.PARAMETERIZED_TEST_DISPLAY;
import static test.util.Util.TIMEOUT;

public class StageMixedStatesTest extends VisualTestBase {

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

    private record StageState(StageStyle stageStyle,
                              List<Consumer<Stage>> initStates,
                              boolean iconified,
                              boolean fullscreen,
                              boolean maximized) {

    }

    static final BiConsumer<Stage, Boolean> SET_MAXIMIZED = Stage::setMaximized;
    static final BiConsumer<Stage, Boolean> SET_ICONIFIED = Stage::setIconified;
    static final BiConsumer<Stage, Boolean> SET_FULL_SCREEN = Stage::setFullScreen;

    static final List<List<BiConsumer<Stage, Boolean>>> INIT_STATES_PERMUTATION = List.of(
            List.of(SET_MAXIMIZED, SET_ICONIFIED, SET_FULL_SCREEN),
            List.of(SET_MAXIMIZED, SET_FULL_SCREEN, SET_ICONIFIED),
            List.of(SET_ICONIFIED, SET_FULL_SCREEN, SET_MAXIMIZED),
            List.of(SET_ICONIFIED, SET_MAXIMIZED, SET_FULL_SCREEN),
            List.of(SET_FULL_SCREEN, SET_MAXIMIZED, SET_ICONIFIED),
            List.of(SET_FULL_SCREEN, SET_ICONIFIED, SET_MAXIMIZED)
    );

    static final List<List<Boolean>> STATE_VALUE_PERMUTATION = List.of(
            List.of(true, true, true),
            List.of(true, true, false),
            List.of(true, false, true),
            List.of(false, true, true),
            List.of(false, true, false),
            List.of(false, false, false)
    );

    static final List<StageStyle> TEST_STATE_STYLES = List.of(StageStyle.DECORATED, StageStyle.UNDECORATED);

    static Stream<StageState> testStatesPermutation() {
        List<StageState> stageStates = new ArrayList<>();
        for (List<BiConsumer<Stage, Boolean>> ss : INIT_STATES_PERMUTATION) {
            for (List<Boolean> sp : STATE_VALUE_PERMUTATION) {
                List<Consumer<Stage>> initStatesConsumer = getInitStateConsumers(ss, sp);
                for (StageStyle style : TEST_STATE_STYLES) {
                    stageStates.add(new StageState(style, initStatesConsumer, sp.get(0), sp.get(1), sp.get(2)));
                }
            }
        }

        return stageStates.stream();
    }

    private static List<Consumer<Stage>> getInitStateConsumers(List<BiConsumer<Stage, Boolean>> ss, List<Boolean> sp) {
        return List.of(
                (stage) -> ss.get(0).accept(stage, sp.get(0)),
                (stage) -> ss.get(1).accept(stage, sp.get(1)),
                (stage) -> ss.get(2).accept(stage, sp.get(2))
        );
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @MethodSource("testStatesPermutation")
    public void testStageStatePrecedenceOrderBeforeShow(StageState stageState) throws InterruptedException {
        setupStages(false, false, stageState.stageStyle());

        Util.doTimeLine(500,
                () -> {
                    for (Consumer<Stage> stageConsumer : stageState.initStates()) {
                        stageConsumer.accept(topStage);
                    }
                    topStage.show();
                },
                () -> {
                    assertStates(stageState.fullscreen(), stageState.iconified(), stageState.maximized());
                    Color color = getColor(200, 200);

                    boolean bottom = (stageState.iconified() || (!stageState.fullscreen() && !stageState.maximized()));
                    assertColorEquals((bottom) ? BOTTOM_COLOR : TOP_COLOR, color, TOLERANCE);
                });
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @MethodSource("testStatesPermutation")
    public void testStageStatePrecedenceOrderAfterShow(StageState stageState) throws InterruptedException {
        setupStages(false, false, stageState.stageStyle());

        List<Runnable> runnables = new ArrayList<>();
        runnables.add(() -> topStage.show());

        for (Consumer<Stage> stageConsumer : stageState.initStates()) {
            runnables.add(() -> stageConsumer.accept(topStage));
        }

        runnables.add(() -> {
            assertStates(stageState.fullscreen(), stageState.iconified(), stageState.maximized());
            Color color = getColor(200, 200);

            boolean bottom = (stageState.iconified() || (!stageState.fullscreen() && !stageState.maximized()));
            assertColorEquals((bottom) ? BOTTOM_COLOR : TOP_COLOR, color, TOLERANCE);
        });

        Util.doTimeLine(500, runnables.toArray(new Runnable[0]));
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED"})
    public void testStagePrecendenceOrderDeiconifyUnfullscreenUnmaximize(StageStyle stageStyle)
            throws InterruptedException {
        setupStages(false, false, stageStyle);

        Util.doTimeLine(500,
                () -> {
                    Color color = getColor(200, 200);
                    assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

                    topStage.setMaximized(true);
                    topStage.setFullScreen(true);
                    topStage.setIconified(true);
                    topStage.show();
                },
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

        Util.doTimeLine(500,
                () -> {
                    Color color = getColor(200, 200);
                    assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

                    topStage.setFullScreen(true);
                    topStage.setMaximized(true);
                    topStage.setIconified(true);
                    topStage.show();
                },
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
