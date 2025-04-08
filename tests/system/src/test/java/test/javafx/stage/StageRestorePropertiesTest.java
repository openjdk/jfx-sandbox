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

import javafx.animation.KeyFrame;
import javafx.animation.Timeline;
import javafx.application.Platform;
import javafx.scene.Scene;
import javafx.scene.layout.StackPane;
import javafx.scene.paint.Color;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import javafx.util.Duration;
import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import test.util.Util;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.jupiter.api.Assertions.assertEquals;

public class StageRestorePropertiesTest {
    private static CountDownLatch startupLatch = new CountDownLatch(1);
    private static final int POS_X = 100;
    private static final int POS_Y = 150;
    private static final int WIDTH = 100;
    private static final int HEIGHT = 150;

    private Stage stage;

    private void setupTest(StageStyle stageStyle) {
       CountDownLatch shownLatch = new CountDownLatch(1);

        Platform.runLater(() -> {
            stage = new Stage();
            stage.initStyle(stageStyle);
            stage.setScene(new Scene(new StackPane(), Color.CHOCOLATE));
            stage.setWidth(300);
            stage.setHeight(300);
            stage.setX(300);
            stage.setY(300);
            stage.setOnShown(e -> shownLatch.countDown());
            stage.show();
        });

        Util.waitForLatch(shownLatch, 5, "Stage failed to show");
    }

    @BeforeAll
    public static void initFX() {
        Platform.setImplicitExit(false);
        Util.startup(startupLatch, startupLatch::countDown);
    }

    @AfterAll
    public static void teardown() {
        Util.shutdown();
    }

    @AfterEach
    public void cleanup() {
        if (stage != null) {
            CountDownLatch hideLatch = new CountDownLatch(1);
            stage.setOnHidden(e -> hideLatch.countDown());
            Platform.runLater(stage::hide);
            Util.waitForLatch(hideLatch, 5, "Stage failed to hide");
        }
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED"})
    public void testUnFullscreenChangedPosition(StageStyle stageStyle) throws Exception {
        setupTest(stageStyle);
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setFullScreen(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    stage.setX(POS_X);
                    stage.setY(POS_Y);
                }),
                new KeyFrame(Duration.millis(900), e -> stage.setFullScreen(false)),
                new KeyFrame(Duration.millis(1200), e -> latch.countDown())
        );
        timeline.play();

        latch.await(5, TimeUnit.SECONDS);
        Util.sleep(300);

        assertEquals(POS_X, stage.getX(), "Window failed to restore position set while fullscreened");
        assertEquals(POS_Y, stage.getY(),  "Window failed to restore position set while fullscreened");
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED"})
    public void testUnFullscreenChangedSize(StageStyle stageStyle) throws Exception {
        setupTest(stageStyle);
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setFullScreen(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    stage.setWidth(WIDTH);
                    stage.setHeight(HEIGHT);
                }),
                new KeyFrame(Duration.millis(900), e -> stage.setFullScreen(false)),
                new KeyFrame(Duration.millis(1200), e -> latch.countDown())
        );
        timeline.play();

        latch.await(5, TimeUnit.SECONDS);
        Util.sleep(300);

        assertEquals(WIDTH, stage.getWidth(), "Window failed to restore size set while fullscreened");
        assertEquals(HEIGHT, stage.getHeight(),  "Window failed to restore size set while fullscreened");
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED"})
    public void testUnMaximzeChangedPosition(StageStyle stageStyle) throws Exception {
        setupTest(stageStyle);
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setMaximized(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    stage.setX(POS_X);
                    stage.setY(POS_Y);
                }),
                new KeyFrame(Duration.millis(900), e -> stage.setMaximized(false)),
                new KeyFrame(Duration.millis(1200), e -> latch.countDown())
        );
        timeline.play();

        latch.await(5, TimeUnit.SECONDS);
        Util.sleep(300);

        assertEquals(POS_X, stage.getX(), "Window failed to restore position set while maximized");
        assertEquals(POS_Y, stage.getY(),  "Window failed to restore position set while maximized");
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED"})
    public void testUnMaximizeChangedSize(StageStyle stageStyle) throws Exception {
        setupTest(stageStyle);
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setMaximized(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    stage.setWidth(WIDTH);
                    stage.setHeight(HEIGHT);
                }),
                new KeyFrame(Duration.millis(900), e -> stage.setMaximized(false)),
                new KeyFrame(Duration.millis(1200), e -> latch.countDown())
        );
        timeline.play();

        latch.await(5, TimeUnit.SECONDS);
        Util.sleep(300);

        assertEquals(WIDTH, stage.getWidth(), "Window failed to restore size set while maximized");
        assertEquals(HEIGHT, stage.getHeight(),  "Window failed to restore size set while maximized");
    }
}
