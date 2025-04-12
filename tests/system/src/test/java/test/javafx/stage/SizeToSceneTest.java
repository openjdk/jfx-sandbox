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

import javafx.animation.KeyFrame;
import javafx.animation.Timeline;
import javafx.scene.Parent;
import javafx.scene.layout.StackPane;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import javafx.stage.Window;
import javafx.util.Duration;
import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

class SizeToSceneTest extends StageTestBase {
    private static final double STAGE_WIDTH = 300;
    private static final double STAGE_HEIGHT = 300;
    private static final double SCENE_WIDTH = 130;
    private static final double SCENE_HEIGHT = 120;
    private static final double DECORATION_DELTA = 50;

    @Override
    protected Parent createRoot() {
        StackPane root = new StackPane();
        root.setPrefSize(SCENE_WIDTH, SCENE_HEIGHT);
        return root;
    }

    @Override
    protected void setupStageStyle(StageStyle stageStyle, Consumer<Stage> pc) {
        Consumer<Stage> cs = stage -> {
            stage.setWidth(STAGE_WIDTH);
            stage.setHeight(STAGE_HEIGHT);
        };

        if (pc != null) {
            cs = cs.andThen(pc);
        }

        super.setupStageStyle(stageStyle, cs);
    }

    private double getDelta(StageStyle stageStyle) {
        if (stageStyle != StageStyle.DECORATED && stageStyle != StageStyle.UTILITY) {
            return DECORATION_DELTA;
        }

        return 0;
    }

    private void assertWindowSizeMatchesToScene(StageStyle stageStyle) {
        Assertions.assertEquals(SCENE_WIDTH, getStage().getWidth(), getDelta(stageStyle));
        Assertions.assertEquals(SCENE_HEIGHT, getStage().getHeight(), getDelta(stageStyle));
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnMaximizedThenSizeToScene(StageStyle stageStyle) {
        setupStageStyle(stageStyle, s -> {
            s.setMaximized(true);
            s.sizeToScene();
        });

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnFullscreenThenSizeToScene(StageStyle stageStyle) {
        setupStageStyle(stageStyle, s -> {
            s.setFullScreen(true);
            s.sizeToScene();
        });

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnSizeToSceneThenMaximized(StageStyle stageStyle) throws InterruptedException {
        setupStageStyle(stageStyle, Window::sizeToScene);

        CountDownLatch latch = new CountDownLatch(1);
        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> getStage().setMaximized(true)),
                new KeyFrame(Duration.millis(600), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnSizeToSceneThenFullscreen(StageStyle stageStyle) throws InterruptedException {
        setupStageStyle(stageStyle, Window::sizeToScene);

        CountDownLatch latch = new CountDownLatch(1);
        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> getStage().setFullScreen(true)),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeAfterShowSizeToSceneThenFullscreen(StageStyle stageStyle) throws InterruptedException {
        setupStageStyle(stageStyle, null);

        CountDownLatch latch = new CountDownLatch(1);
        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> getStage().sizeToScene()),
                new KeyFrame(Duration.millis(600), e -> getStage().setFullScreen(true)),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeAfterShowSizeToSceneThenMaximized(StageStyle stageStyle) throws InterruptedException {
        setupStageStyle(stageStyle, null);

        CountDownLatch latch = new CountDownLatch(1);
        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> getStage().sizeToScene()),
                new KeyFrame(Duration.millis(600), e -> getStage().setMaximized(true)),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeAfterShowFullscreenThenSizeToScene(StageStyle stageStyle) throws InterruptedException {
        setupStageStyle(stageStyle, null);

        CountDownLatch latch = new CountDownLatch(1);
        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> getStage().setFullScreen(true)),
                new KeyFrame(Duration.millis(600), e -> getStage().sizeToScene()),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeAfterShowMaximizedThenSizeToScene(StageStyle stageStyle) throws InterruptedException {
        setupStageStyle(stageStyle, null);

        CountDownLatch latch = new CountDownLatch(1);
        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> getStage().setMaximized(true)),
                new KeyFrame(Duration.millis(600), e -> getStage().sizeToScene()),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeOnSizeToScene(StageStyle stageStyle) {
        setupStageStyle(stageStyle, Window::sizeToScene);

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeFullscreenOnOffSizeToScene(StageStyle stageStyle) throws InterruptedException {
        setupStageStyle(stageStyle, null);

        CountDownLatch latch = new CountDownLatch(1);
        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> getStage().setFullScreen(true)),
                new KeyFrame(Duration.millis(600), e -> getStage().sizeToScene()),
                new KeyFrame(Duration.millis(900), e -> getStage().setFullScreen(false)),
                new KeyFrame(Duration.millis(1900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeSizeToSceneFullscreenOnOff(StageStyle stageStyle) {
        setupStageStyle(stageStyle, s -> {
            s.sizeToScene();
            s.setFullScreen(true);
            s.setFullScreen(false);
        });

        assertWindowSizeMatchesToScene(stageStyle);
    }


    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeMaximizedOnOffSizeToScene(StageStyle stageStyle) {
        setupStageStyle(stageStyle, s -> {
            s.setMaximized(true);
            s.sizeToScene();
            s.setMaximized(false);
        });

        assertWindowSizeMatchesToScene(stageStyle);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    void testInitialSizeSizeToSceneMaximizedOnOff(StageStyle stageStyle) {
        setupStageStyle(stageStyle, s -> {
            s.sizeToScene();
            s.setMaximized(true);
            s.setMaximized(false);
        });

        assertWindowSizeMatchesToScene(stageStyle);
    }
}
