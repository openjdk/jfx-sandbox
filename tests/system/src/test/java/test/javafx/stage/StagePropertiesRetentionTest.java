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
import javafx.application.Application;
import javafx.application.Platform;
import javafx.scene.Scene;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;
import javafx.stage.WindowEvent;
import javafx.util.Duration;
import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import test.util.Util;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.jupiter.api.Assertions.assertEquals;

public class StagePropertiesRetentionTest {
    private static CountDownLatch startupLatch = new CountDownLatch(1);
    private static final int POS_X = 100;
    private static final int POS_Y = 150;
    private static final int WIDTH = 100;
    private static final int HEIGHT = 150;

    private static Stage stage;

    public static class TestApp extends Application {

        @Override
        public void start(Stage primaryStage) throws Exception {
            primaryStage.setScene(new Scene(new VBox()));
            stage = primaryStage;
            stage.setWidth(WIDTH);
            stage.setHeight(HEIGHT);
            stage.setX(POS_X);
            stage.setY(POS_Y);
            stage.addEventHandler(WindowEvent.WINDOW_SHOWN, e ->
                                    Platform.runLater(startupLatch::countDown));
            stage.show();
        }
    }

    @BeforeAll
    public static void initFX() {
        Util.launch(startupLatch, TestApp.class);
    }

    @AfterAll
    public static void teardown() {
        Util.shutdown();
    }

    @Test
    public void testFullscreenShouldNotChangeStagesSize() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setFullScreen(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    assertEquals(WIDTH, stage.getWidth(), "Entering fullscreen mode changed the Stage's width");
                    assertEquals(HEIGHT, stage.getHeight(), "Entering fullscreen mode changed the Stage's height");
                }),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);
    }

    @Test
    public void testFullscreenShouldNotChangeStagesLocation() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setFullScreen(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    assertEquals(POS_X, stage.getX(), "Entering fullscreen mode changed the Stage's X position");
                    assertEquals(POS_Y, stage.getY(), "Entering fullscreen mode changed the Stage's Y position");
                }),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);
    }

    @Test
    public void testMaximizeShouldNotChangeStagesLocation() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setMaximized(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    assertEquals(POS_X, stage.getX(), "Maximized mode changed the Stage's X position");
                    assertEquals(POS_Y, stage.getY(), "Maximized mode changed the Stage's Y position");
                }),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);
    }

    @Test
    public void testMaximizeShouldNotChangeStagesSize() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setMaximized(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    assertEquals(WIDTH, stage.getWidth(), "Maximized mode changed the Stage's width");
                    assertEquals(HEIGHT, stage.getHeight(), "Maximized mode changed the Stage's height");
                }),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);
    }
}
