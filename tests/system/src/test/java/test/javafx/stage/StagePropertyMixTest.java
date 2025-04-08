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

import javafx.application.Platform;
import javafx.scene.Scene;
import javafx.scene.layout.StackPane;
import javafx.scene.paint.Color;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;
import test.util.Util;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

public class StagePropertyMixTest {
    private static CountDownLatch startupLatch = new CountDownLatch(1);
    private static final int WIDTH = 300;
    private static final int HEIGHT = 300;

    private Stage stage;

    private void setupStage(Consumer<Stage> pc) {
        CountDownLatch shownLatch = new CountDownLatch(1);
        Platform.runLater(() -> {
            stage = new Stage();
            stage.setScene(new Scene(new StackPane(), Color.HOTPINK));
            pc.accept(stage);
            stage.setOnShown(e -> shownLatch.countDown());
            stage.show();
        });

        Util.waitForLatch(shownLatch, 5, "Stage failed to show");
    }

    private void setupTransparentStage(Consumer<Stage> pc) {
        CountDownLatch shownLatch = new CountDownLatch(1);
        Platform.runLater(() -> {
            stage = new Stage();
            stage.initStyle(StageStyle.TRANSPARENT);
            StackPane root = new StackPane();
            root.setStyle("-fx-background-color: rgba(0, 0, 255, 0.5);");
            Scene scene = new Scene(root);
            scene.setFill(Color.TRANSPARENT);
            pc.accept(stage);
            stage.setOnShown(e -> shownLatch.countDown());
            stage.show();
        });

        Util.waitForLatch(shownLatch, 5, "Stage failed to show");
    }

    private void setupStageStyle(StageStyle stageStyle, Consumer<Stage> pc) {
        if (stageStyle == StageStyle.TRANSPARENT) {
            setupStage(pc);
        } else  {
            setupTransparentStage(pc);
        }
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

    @Test
    public void testMaximizeUndecorated() throws Exception {
        int pos = 100;

        setupStage(s -> {
            s.initStyle(StageStyle.UNDECORATED);
            s.setX(pos);
            s.setY(pos);
            s.setWidth(WIDTH);
            s.setHeight(HEIGHT);
        });

        CountDownLatch latch = new CountDownLatch(1);

        stage.maximizedProperty().addListener((observable, oldValue, newValue) -> {
            if (Boolean.TRUE.equals(newValue)) {
                latch.countDown();
            }
        });

        Util.runAndWait(() -> stage.setMaximized(true));
        latch.await(5, TimeUnit.SECONDS);

        Util.sleep(500);

        assertTrue(stage.isMaximized(), "Stage should be maximized");
        assertEquals(pos, stage.getX(), "Stage maximized position changed");
        assertEquals(pos, stage.getY(), "Stage maximized position changed");
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testMaximizeUnresizable(StageStyle stageStyle) throws Exception {
        setupStageStyle(stageStyle, s -> {
            s.initStyle(stageStyle);
            s.setWidth(WIDTH);
            s.setHeight(HEIGHT);
            s.setResizable(false);
        });

        CountDownLatch latch = new CountDownLatch(1);

        stage.maximizedProperty().addListener((observable, oldValue, newValue) -> {
            if (Boolean.TRUE.equals(newValue)) {
                latch.countDown();
            }
        });

        Util.runAndWait(() -> stage.setMaximized(true));

        latch.await(5, TimeUnit.SECONDS);
        assertTrue(stage.isMaximized(), "Unresizable stage should be maximized");
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testMaximizeMaxSize(StageStyle stageStyle) throws Exception {
        int maxSize = 500;

        setupStageStyle(stageStyle, s -> {
            s.initStyle(stageStyle);
            s.setWidth(WIDTH);
            s.setHeight(HEIGHT);
            s.setMaxWidth(maxSize);
            s.setMaxHeight(maxSize);
        });

        CountDownLatch maximizeLatch = new CountDownLatch(1);
        CountDownLatch restoreLatch = new CountDownLatch(1);

        stage.maximizedProperty().addListener((observable, oldValue, newValue) -> {
            if (Boolean.TRUE.equals(newValue)) {
                maximizeLatch.countDown();
            } else {
                restoreLatch.countDown();
            }
        });

        Util.runAndWait(() -> stage.setMaximized(true));

        maximizeLatch.await(5, TimeUnit.SECONDS);
        assertTrue(stage.isMaximized(), "Max size stage should be maximized");

        Util.runAndWait(() -> stage.setMaximized(false));
        restoreLatch.await(5, TimeUnit.SECONDS);

        Util.sleep(500);

        assertEquals(WIDTH, stage.getWidth(), "Stage width should have remained");
        assertEquals(HEIGHT, stage.getHeight(), "Stage height should have remained");
        assertEquals(maxSize, stage.getMaxWidth(), "Stage max width should have remained");
        assertEquals(maxSize, stage.getMaxHeight(), "Stage max height should have remained");
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testMaxSizeShouldBeProgramaticallyResized(StageStyle stageStyle) throws Exception {
        int maxSize = 300;
        int newSize = 500;

        setupStageStyle(stageStyle, s -> {
            s.initStyle(stageStyle);
            s.setMaxWidth(maxSize);
            s.setMaxHeight(maxSize);
        });

        CountDownLatch resizeLatch = new CountDownLatch(2);

        stage.widthProperty().addListener((observable, oldValue, newValue) -> resizeLatch.countDown());
        stage.heightProperty().addListener((observable, oldValue, newValue) -> resizeLatch.countDown());

        Util.runAndWait(() -> {
            stage.setWidth(newSize);
            stage.setHeight(newSize);
        });

        resizeLatch.await(5, TimeUnit.SECONDS);
        Util.sleep(500);

        assertEquals(newSize, stage.getWidth(), "Stage width should be programatically resized beyond max width");
        assertEquals(newSize, stage.getHeight(), "Stage height should be programatically resized beyond max height");
        assertEquals(maxSize, stage.getMaxWidth(), "Stage max width should have remained");
        assertEquals(maxSize, stage.getMaxHeight(), "Stage max height should have remained");
    }
}
