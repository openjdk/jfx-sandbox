/*
 * Copyright (c) 2010, 2024, Oracle and/or its affiliates. All rights reserved.
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

package hello;

import javafx.animation.KeyFrame;
import javafx.animation.KeyValue;
import javafx.animation.Timeline;
import javafx.application.Application;
import javafx.scene.Group;
import javafx.scene.Scene;
import javafx.scene.control.ProgressBar;
import javafx.scene.paint.Color;
import javafx.stage.Stage;
import javafx.util.Duration;

public class HelloProgressBar extends Application {

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) {
        Application.launch(args);
    }

    @Override public void start(Stage stage) {
        stage.setTitle("Hello ProgressBar");
        Scene scene = new Scene(new Group(), 600, 450);
        scene.setFill(Color.CHOCOLATE);

        Group root = (Group)scene.getRoot();

        double y = 15;
        final double SPACING = 25;

        ProgressBar pbar = new ProgressBar();
        pbar.setLayoutX(15);
        pbar.setLayoutY(y);
        pbar.setVisible(true);
        root.getChildren().add(pbar);

        y += SPACING;
        pbar = new ProgressBar();
        pbar.setPrefWidth(150);
        pbar.setLayoutX(15);
        pbar.setLayoutY(y);
        pbar.setVisible(true);
        root.getChildren().add(pbar);

        y += SPACING;
        pbar = new ProgressBar();
        pbar.setPrefWidth(200);
        pbar.setLayoutX(15);
        pbar.setLayoutY(y);
        pbar.setVisible(true);
        root.getChildren().add(pbar);

        y = 15;
        pbar = new ProgressBar();
        pbar.setLayoutX(230);
        pbar.setLayoutY(y);
        pbar.setProgress(0.25);
        pbar.setVisible(true);
        root.getChildren().add(pbar);

        y += SPACING;
        pbar = new ProgressBar();
        pbar.setPrefWidth(150);
        pbar.setLayoutX(230);
        pbar.setLayoutY(y);
        pbar.setProgress(0.50);
        pbar.setVisible(true);
        root.getChildren().add(pbar);

        y += SPACING;
        pbar = new ProgressBar();
        pbar.setPrefWidth(200);
        pbar.setLayoutX(230);
        pbar.setLayoutY(y);
        pbar.setProgress(0);
        root.getChildren().add(pbar);

        final Timeline timeline = new Timeline();
        timeline.setCycleCount(Timeline.INDEFINITE);
        timeline.setAutoReverse(true);
        final KeyValue kv = new KeyValue(pbar.progressProperty(), 1);
        final KeyFrame kf1 = new KeyFrame(Duration.millis(3000), kv);
        timeline.getKeyFrames().add(kf1);
        timeline.play();

        stage.setScene(scene);
        stage.show();
    }
}
