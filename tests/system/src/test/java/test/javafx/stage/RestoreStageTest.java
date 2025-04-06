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
import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import test.util.Util;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class RestoreStageTest {
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
            stage.setWidth(300);
            stage.setHeight(300);
            stage.setX(300);
            stage.setY(400);
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
    public void testUnFullscreenChangedPosition() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setFullScreen(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    stage.setX(POS_X);
                    stage.setY(POS_Y);
                }),
                new KeyFrame(Duration.millis(900), e -> stage.setFullScreen(false)),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();

        latch.await(5, TimeUnit.SECONDS);
        Thread.sleep(300);

        Assertions.assertEquals(POS_X, stage.getX(), "Window failed to restore position set while fullscreened");
        Assertions.assertEquals(POS_Y, stage.getY(),  "Window failed to restore position set while fullscreened");
    }

    @Test
    public void testUnFullscreenChangedSize() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setFullScreen(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    stage.setWidth(WIDTH);
                    stage.setHeight(HEIGHT);
                }),
                new KeyFrame(Duration.millis(900), e -> stage.setFullScreen(false)),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();

        latch.await(5, TimeUnit.SECONDS);
        Thread.sleep(300);

        Assertions.assertEquals(WIDTH, stage.getWidth(), "Window failed to restore size set while fullscreened");
        Assertions.assertEquals(HEIGHT, stage.getHeight(),  "Window failed to restore size set while fullscreened");
    }

    @Test
    public void testUnMaximzeChangedPosition() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setMaximized(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    stage.setX(POS_X);
                    stage.setY(POS_Y);
                }),
                new KeyFrame(Duration.millis(900), e -> stage.setMaximized(false)),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();

        latch.await(5, TimeUnit.SECONDS);
        Thread.sleep(300);

        Assertions.assertEquals(POS_X, stage.getX(), "Window failed to restore position set while maximized");
        Assertions.assertEquals(POS_Y, stage.getY(),  "Window failed to restore position set while maximized");
    }

    @Test
    public void testUnMaximizeChangedSize() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> stage.setMaximized(true)),
                new KeyFrame(Duration.millis(600), e -> {
                    stage.setWidth(WIDTH);
                    stage.setHeight(HEIGHT);
                }),
                new KeyFrame(Duration.millis(900), e -> stage.setMaximized(false)),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();

        latch.await(5, TimeUnit.SECONDS);
        Thread.sleep(300);

        Assertions.assertEquals(WIDTH, stage.getWidth(), "Window failed to restore size set while maximized");
        Assertions.assertEquals(HEIGHT, stage.getHeight(),  "Window failed to restore size set while maximized");
    }
}
