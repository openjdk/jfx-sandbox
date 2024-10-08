/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates. All rights reserved.
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

package test.javafx.scene.control;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import javafx.application.Platform;
import javafx.scene.Group;
import javafx.scene.Node;
import javafx.scene.Scene;
import javafx.scene.control.ProgressIndicator;
import javafx.scene.control.skin.ProgressIndicatorSkin;
import javafx.stage.Stage;
import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import test.util.Util;
import test.util.memory.JMemoryBuddy;

public class ProgressIndicatorLeakTest {

    @BeforeAll
    public static void initFX() throws Exception {
        CountDownLatch startupLatch = new CountDownLatch(1);
        Platform.setImplicitExit(false);
        Util.startup(startupLatch, startupLatch::countDown);
    }

    @AfterAll
    public static void teardownOnce() {
        Util.shutdown();
    }

    @Test
    public void leakDeterminationIndicator() throws Exception {
        JMemoryBuddy.memoryTest((checker) -> {
            CountDownLatch showingLatch = new CountDownLatch(1);
            Util.runAndWait(() -> {
                Stage stage = new Stage();
                ProgressIndicator indicator = new ProgressIndicator(-1);
                indicator.setSkin(new ProgressIndicatorSkin(indicator));
                Scene scene = new Scene(indicator);
                stage.setScene(scene);
                indicator.setProgress(1.0);
                Assertions.assertEquals(1, indicator.getChildrenUnmodifiable().size(), "size is wrong");
                Node detIndicator = indicator.getChildrenUnmodifiable().get(0);
                indicator.setProgress(-1.0);
                indicator.setProgress(1.0);
                checker.assertCollectable(detIndicator);
                stage.setOnShown(l -> {
                    Platform.runLater(() -> showingLatch.countDown());
                });
                stage.show();
            });
            try {
                Assertions.assertTrue(showingLatch.await(15, TimeUnit.SECONDS), "Timeout waiting for setOnShown");
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        });
    }

    @Test
    public void stageLeakWhenTreeNotShowing() throws Exception {
        JMemoryBuddy.memoryTest((checker) -> {
            CountDownLatch showingLatch = new CountDownLatch(1);
            AtomicReference<Stage> stage = new AtomicReference<>();

            Util.runAndWait(() -> {
                stage.set(new Stage());
                Group root = new Group();
                root.setVisible(false);
                root.getChildren().add(new ProgressIndicator());
                stage.get().setScene(new Scene(root));
                stage.get().setOnShown(l -> {
                    Platform.runLater(() -> showingLatch.countDown());
                });
                stage.get().show();
            });

            try {
                Assertions.assertTrue(showingLatch.await(15, TimeUnit.SECONDS), "Timeout waiting test stage");
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }

            Util.runAndWait(() -> {
                stage.get().close();
            });

            checker.assertCollectable(stage.get());
        });
    }
}
