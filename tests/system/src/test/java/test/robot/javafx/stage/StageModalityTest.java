/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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

import javafx.animation.KeyFrame;
import javafx.animation.ScaleTransition;
import javafx.animation.Timeline;
import javafx.application.Platform;
import javafx.beans.binding.Bindings;
import javafx.scene.Scene;
import javafx.scene.control.Label;
import javafx.scene.layout.Background;
import javafx.scene.layout.StackPane;
import javafx.scene.paint.Color;
import javafx.stage.Modality;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import javafx.util.Duration;
import org.junit.jupiter.api.Test;
import test.robot.testharness.VisualTestBase;
import test.util.Util;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.jupiter.api.Assertions.assertTrue;
import static test.util.Util.TIMEOUT;

class StageModalityTest extends VisualTestBase {
    private static final int WIDTH = 300;
    private static final int HEIGHT = 300;
    private Stage topStage;
    private Stage bottomStage;
    private static final Color TOP_COLOR = Color.RED;
    private static final Color BOTTOM_COLOR = Color.LIME;
    private static final Color COLOR1 = Color.RED;
    private static final Color COLOR2 = Color.ORANGE;
    private static final Color COLOR3 = Color.YELLOW;
    private static final Color COLOR4 = Color.GREEN;
    private static final Color COLOR5 = Color.BLUE;
    private static final Color COLOR6 = Color.INDIGO;
    private static final Color COLOR7 = Color.VIOLET;
    private static final double TOLERANCE = 0.00;

    private void setupBottomStage() throws InterruptedException {
        final CountDownLatch shownLatch = new CountDownLatch(1);

        runAndWait(() -> {
            bottomStage = getStage(false);
            bottomStage.initStyle(StageStyle.DECORATED);
            Scene bottomScene = new Scene(getFocusedLabel(BOTTOM_COLOR, bottomStage), WIDTH, HEIGHT);
            bottomScene.setFill(BOTTOM_COLOR);
            bottomStage.setScene(bottomScene);
            bottomStage.setX(0);
            bottomStage.setY(0);
            bottomStage.setOnShown(e -> Platform.runLater(shownLatch::countDown));
            bottomStage.show();
        });
        assertTrue(shownLatch.await(TIMEOUT, TimeUnit.MILLISECONDS),
                "Timeout waiting for bottom stage to be shown");

        sleep(500);
    }

    private void setupTopStage(Stage owner, Modality modality) {
        runAndWait(() -> {
            topStage = getStage(true);
            topStage.initStyle(StageStyle.DECORATED);
            Scene topScene = new Scene(getFocusedLabel(TOP_COLOR, topStage), WIDTH, HEIGHT);
            topScene.setFill(TOP_COLOR);
            topStage.setScene(topScene);
            if (owner != null) {
                topStage.initOwner(owner);
            }
            if (modality != null) {
                topStage.initModality(modality);
            }
            topStage.setWidth(WIDTH);
            topStage.setHeight(HEIGHT);
            topStage.setX(0);
            topStage.setY(0);
        });
    }

    @Test
    void testOpeningModalChildStageWhileMaximized() throws InterruptedException {
        setupBottomStage();
        setupTopStage(bottomStage, Modality.WINDOW_MODAL);

        Util.doTimeLine(500,
                () -> bottomStage.setMaximized(true),
                () -> topStage.show(),
                () -> {
                    assertTrue(bottomStage.isMaximized());

                    Color color = getColor(400, 400);
                    assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

                    color = getColor(100, 100);
                    assertColorEquals(TOP_COLOR, color, TOLERANCE);
                });
    }

    @Test
    void testOpeningModalChildStageWhileFullSceen() throws InterruptedException {
        setupBottomStage();
        setupTopStage(bottomStage, Modality.WINDOW_MODAL);

        Util.doTimeLine(500,
                () -> bottomStage.setFullScreen(true),
                () -> topStage.show(),
                () -> {
                    assertTrue(bottomStage.isFullScreen());

                    Color color = getColor(400, 400);
                    assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

                    color = getColor(100, 100);
                    assertColorEquals(TOP_COLOR, color, TOLERANCE);
                });
    }

    @Test
    void testOpeningAppModalStageWhileMaximized() throws InterruptedException {
        setupBottomStage();
        setupTopStage(null, Modality.APPLICATION_MODAL);

        Util.doTimeLine(500,
                () -> bottomStage.setMaximized(true),
                () -> topStage.show(),
                () -> {
                    assertTrue(bottomStage.isMaximized());
                    Color color = getColor(400, 400);
                    assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

                    color = getColor(100, 100);
                    assertColorEquals(TOP_COLOR, color, TOLERANCE);
                });
    }

    @Test
    void testOpeningAppModalStageWhileFullScreen() throws InterruptedException {
        setupBottomStage();
        setupTopStage(null, Modality.APPLICATION_MODAL);

        Util.doTimeLine(500,
                () -> bottomStage.setFullScreen(true),
                () -> topStage.show(),
                () -> {
                    assertTrue(bottomStage.isFullScreen());

                    Color color = getColor(400, 400);
                    assertColorEquals(BOTTOM_COLOR, color, TOLERANCE);

                    color = getColor(100, 100);
                    assertColorEquals(TOP_COLOR, color, TOLERANCE);
                });
    }

    private Stage createStage(Color color, Stage owner, int x, int y) {
        Stage stage = getStage(true);
        stage.initStyle(StageStyle.UNDECORATED);
        StackPane pane = getFocusedLabel(color, stage);
        Scene scene = new Scene(pane, WIDTH, HEIGHT);
        scene.setFill(color);
        stage.focusedProperty().addListener((obs, oldVal, newVal) -> {
            if (!newVal) {
                shakeStage(stage);
            }
        });
        stage.setScene(scene);
        stage.setWidth(WIDTH);
        stage.setHeight(HEIGHT);
        stage.setX(x);
        stage.setY(y);
        if (owner != null) {
            stage.initOwner(owner);
        }
        stage.initModality(Modality.WINDOW_MODAL);
        return stage;
    }

    private static StackPane getFocusedLabel(Color color, Stage stage) {
        Label label = new Label();
        label.textProperty().bind(Bindings.when(stage.focusedProperty())
                .then("Focused").otherwise("Unfocused"));
        StackPane pane = new StackPane(label);
        pane.setBackground(Background.EMPTY);

        double luminance = 0.2126 * color.getRed()
                + 0.7152 * color.getGreen()
                + 0.0722 * color.getBlue();

        Color textColor = luminance < 0.5 ? Color.WHITE : Color.BLACK;

        label.setTextFill(textColor);
        return pane;
    }

    private void shakeStage(Stage stage) {
        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(50), e -> stage.setX(stage.getX() + 10)),
                new KeyFrame(Duration.millis(100), e -> stage.setX(stage.getX() - 10)));
        timeline.setCycleCount(3);
        timeline.play();
    }

    private Stage stage0;
    private Stage stage1;
    private Stage stage2;
    private Stage stage3;
    private Stage stage4;
    private Stage stage5;
    private Stage stage6;

    @Test
    void testLayeredWindowModality() {
        Util.runAndWait(() -> {
                    stage0 = createStage(COLOR1, null, 100, 100);
                    stage1 = createStage(COLOR2, stage0, 150, 150);
                    stage2 = createStage(COLOR3, stage1, 200, 200);
                    stage3 = createStage(COLOR4, stage2, 250, 250);
                    stage4 = createStage(COLOR5, stage3, 300, 300);
                    stage5 = createStage(COLOR6, stage4, 350, 350);
                    stage6 = createStage(COLOR7, stage5, 400, 400);
                });

        Util.doTimeLine(500,
                stage0::show,
                stage1::show,
                stage2::show,
                stage3::show,
                stage4::show,
                stage5::show,
                stage6::show,
                () -> {
                    assertColor(COLOR1, stage0);
                    assertColor(COLOR2, stage1);
                    assertColor(COLOR3, stage2);
                    assertColor(COLOR4, stage3);
                    assertColor(COLOR5, stage4);
                    assertColor(COLOR6, stage5);
                    assertColor(COLOR7, stage6);
                },
                () -> clickStage(stage5),
                () -> assertTrue(stage6.isFocused()),
                stage5::close,
                () -> assertTrue(stage6.isFocused()),
                stage6::close,
                stage5::close,
                () -> assertColor(COLOR5, stage4));
    }

    private void clickStage(Stage stage) {
        getRobot().mouseMove(stage.getX() + 25, stage.getY() + 25);
        getRobot().mouseClick(javafx.scene.input.MouseButton.PRIMARY);
    }

    private void assertColor(Color expected, Stage stage) {
        Color color = getColor((int) stage.getX() + 25, (int) stage.getY() + 25);
        assertColorEquals(expected, color, TOLERANCE);
    }

    @Test
    void testMultiLayeredWindowModality() {
        Util.runAndWait(() -> {
                    stage0 = createStage(COLOR1, null, 100, 100);
                    stage1 = createStage(COLOR2, stage0, 150, 150);
                    stage2 = createStage(COLOR3, stage1, 200, 200);

                    stage3 = createStage(COLOR4, null, 600, 100);
                    stage4 = createStage(COLOR5, stage3, 650, 150);
                    stage5 = createStage(COLOR6, stage4, 700, 200);

                    stage6 = createStage(COLOR7, null, 0, 0);
                    stage6.centerOnScreen();
                    stage6.initModality(Modality.APPLICATION_MODAL);
                });

        Util.doTimeLine(500,
                stage0::show,
                stage1::show,
                stage2::show,
                stage3::show,
                stage4::show,
                stage5::show,
                stage6::show,
                stage6::close,
                () -> {
                    assertColor(COLOR1, stage0);
                    assertColor(COLOR2, stage1);
                    assertColor(COLOR3, stage2);
                    assertColor(COLOR4, stage3);
                    assertColor(COLOR5, stage4);
                    assertColor(COLOR6, stage5);
                },
                () -> assertTrue(stage5.isFocused()),
                stage5::close,
                () -> assertTrue(stage4.isFocused()),
                stage4::close,
                () -> assertTrue(stage3.isFocused()),
                stage3::close,
                () -> assertTrue(stage2.isFocused()),
                stage2::close,
                () -> assertTrue(stage1.isFocused()),
                stage1::close,
                () -> assertTrue(stage0.isFocused()));
    }
}
