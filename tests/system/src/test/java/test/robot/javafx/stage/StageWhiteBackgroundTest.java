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
import javafx.scene.layout.Background;
import javafx.scene.layout.StackPane;
import javafx.scene.paint.Color;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import test.robot.testharness.VisualTestBase;
import test.util.Util;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.jupiter.api.Assertions.assertTrue;
import static test.util.Util.PARAMETERIZED_TEST_DISPLAY;
import static test.util.Util.TIMEOUT;

class StageWhiteBackgroundTest extends VisualTestBase {
    private static final int WIDTH = 400;
    private static final int HEIGHT = 400;
    private static final double TOLERANCE = 0.07;

    private Stage stage;

    private void setupStage(StageStyle stageStyle, boolean withScene)
            throws InterruptedException {
        final CountDownLatch stageShownLatch = new CountDownLatch(1);

        runAndWait(() -> {
            stage = getStage(true);
            stage.initStyle(stageStyle);
            if (withScene) {
                StackPane pane = new StackPane();
                pane.setBackground(Background.fill(Color.TRANSPARENT));
                Scene scene = new Scene(pane, WIDTH, HEIGHT);
                stage.setScene(scene);
            }
            stage.setX(0);
            stage.setY(0);
            stage.setOnShown(e -> Platform.runLater(stageShownLatch::countDown));
            stage.show();
        });

        assertTrue(stageShownLatch.await(TIMEOUT, TimeUnit.MILLISECONDS),
                "Timeout waiting for bottom stage to be shown");

        sleep(500);
    }


    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED",
            "UTILITY"})
    void testStageWithoutSceneColor(StageStyle stageState) throws InterruptedException {
        setupStage(stageState, false);

        Util.sleep(500);

        Util.runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(Color.WHITE, color, TOLERANCE);
        });
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"DECORATED", "UNDECORATED",
            "UTILITY"})
    void testStageWithSceneColor(StageStyle stageState) throws InterruptedException {
        setupStage(stageState, true);

        Util.sleep(500);

        Util.runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorEquals(Color.WHITE, color, TOLERANCE);
        });
    }

    @ParameterizedTest(name = PARAMETERIZED_TEST_DISPLAY)
    @EnumSource(value = StageStyle.class, mode = EnumSource.Mode.INCLUDE, names = {"TRANSPARENT"})
    void testTransparentStageWithoutSceneColor(StageStyle stageState) throws InterruptedException {
        setupStage(stageState, false);

        Util.sleep(500);

        Util.runAndWait(() -> {
            Color color = getColor(200, 200);
            assertColorDoesNotEqual(Color.WHITE, color, TOLERANCE);
        });
    }
}
