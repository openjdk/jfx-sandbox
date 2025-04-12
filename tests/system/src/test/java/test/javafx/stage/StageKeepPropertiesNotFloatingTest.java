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
import javafx.stage.StageStyle;
import javafx.util.Duration;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.EnumSource;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.jupiter.api.Assertions.assertEquals;

public class StageKeepPropertiesNotFloatingTest extends StageTestBase {
    private static final int POS_X = 100;
    private static final int POS_Y = 150;
    private static final int WIDTH = 100;
    private static final int HEIGHT = 150;

    private void doTimeLine(Runnable setNonFloatingRunnable,
                            Runnable assertRunnable) throws InterruptedException {
        CountDownLatch latch = new CountDownLatch(1);
        Timeline timeline = new Timeline(
                new KeyFrame(Duration.millis(300), e -> setNonFloatingRunnable.run()),
                new KeyFrame(Duration.millis(600), e -> assertRunnable.run()),
                new KeyFrame(Duration.millis(900), e -> latch.countDown())
        );
        timeline.play();
        latch.await(5, TimeUnit.SECONDS);
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testFullscreenShouldKeepProperties(StageStyle stageStyle) throws InterruptedException {
        setupStageStyle(stageStyle, null);

        doTimeLine(() -> getStage().setFullScreen(true),
           () -> {
               assertEquals(WIDTH, getStage().getWidth(), "Entering fullscreen mode changed the Stage's width");
               assertEquals(HEIGHT, getStage().getHeight(), "Entering fullscreen mode changed the Stage's height");
               assertEquals(POS_X, getStage().getX(), "Entering fullscreen mode changed the Stage's X position");
               assertEquals(POS_Y, getStage().getY(), "Entering fullscreen mode changed the Stage's Y position");
           });
    }

    @ParameterizedTest
    @EnumSource(names = {"DECORATED", "UNDECORATED", "TRANSPARENT"})
    public void testMaximizeShouldKeepProperties(StageStyle stageStyle) throws InterruptedException {
        setupStageStyle(stageStyle, null);

        doTimeLine(() -> getStage().setMaximized(true),
            () -> {
                assertEquals(WIDTH, getStage().getWidth(), "Maximized mode changed the Stage's width");
                assertEquals(HEIGHT, getStage().getHeight(), "Maximized mode changed the Stage's height");
                assertEquals(POS_X, getStage().getX(), "Maximized mode changed the Stage's X position");
                assertEquals(POS_Y, getStage().getY(), "Maximized mode changed the Stage's Y position");
            });
    }
}
