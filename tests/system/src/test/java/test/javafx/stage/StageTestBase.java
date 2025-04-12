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
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.layout.StackPane;
import javafx.scene.paint.Color;
import javafx.stage.Stage;
import javafx.stage.StageStyle;
import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeAll;
import test.util.Util;

import java.util.concurrent.CountDownLatch;
import java.util.function.Consumer;

public abstract class StageTestBase {
    private static final CountDownLatch startupLatch = new CountDownLatch(1);
    private Stage stage;

    /**
     * Creates a Scene for the test stage acoording to the {@link StageStyle}
     * @param stageStyle {@link StageStyle} of the Stage
     * @return a {@link Scene}
     */
    protected Scene createScene(StageStyle stageStyle) {
        if (stageStyle == StageStyle.TRANSPARENT) {
            Parent root = createRoot();
            root.setStyle("-fx-background-color: rgba(255, 105, 180, 0.5);");
            Scene scene = new Scene(root);
            scene.setFill(Color.TRANSPARENT);

            return scene;
        }

        return new Scene(createRoot(), Color.HOTPINK);
    }

    /**
     * Gets the Scene root
     */
    protected Parent createRoot() {
        return new StackPane();
    }

    /**
     * Utility method to setup test Stages according to {@link StageStyle}
     * @param stageStyle The Stage Style.
     * @param pc A consumer to set state properties
     */
    protected void setupStageStyle(StageStyle stageStyle, Consumer<Stage> pc) {
        CountDownLatch shownLatch = new CountDownLatch(1);
        Platform.runLater(() -> {
            stage = new Stage();
            stage.setAlwaysOnTop(true);
            if (pc != null) {
                pc.accept(stage);
            }
            stage.initStyle(StageStyle.TRANSPARENT);
            stage.setScene(createScene(stageStyle));
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

    /**
     * Hides the test stage after each test
     */
    @AfterEach
    public void cleanup() {
        if (stage != null) {
            CountDownLatch hideLatch = new CountDownLatch(1);
            stage.setOnHidden(e -> hideLatch.countDown());
            Platform.runLater(stage::hide);
            Util.waitForLatch(hideLatch, 5, "Stage failed to hide");
        }
    }

    /**
     * @return The stage that is created for each test
     */
    protected Stage getStage() {
        return stage;
    }
}
