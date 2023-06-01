/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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

package renderperf;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import javafx.animation.AnimationTimer;
import javafx.application.Platform;
import javafx.geometry.Point3D;
import javafx.scene.control.Button;
import javafx.scene.effect.BlendMode;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;
import javafx.scene.paint.Color;
import javafx.scene.paint.CycleMethod;
import javafx.scene.paint.LinearGradient;
import javafx.scene.paint.PhongMaterial;
import javafx.scene.paint.RadialGradient;
import javafx.scene.paint.Stop;
import javafx.scene.shape.Arc;
import javafx.scene.shape.ArcType;
import javafx.scene.shape.Box;
import javafx.scene.shape.Circle;
import javafx.scene.shape.CubicCurve;
import javafx.scene.shape.Ellipse;
import javafx.scene.shape.Rectangle;
import javafx.scene.text.Font;
import javafx.scene.text.Text;
import javafx.scene.Group;
import javafx.scene.Scene;
import javafx.stage.Stage;

/**
 * {@link RenderPerfTest} is an application to measure graphic rendering performance
 * of JavaFX graphic rendering by calculatiing the Frames per second (FPS) value.
 * The application calculates the FPS by creating a JavaFX environment and rendering
 * objects such as Circle, Image, Text etc based on the test executed. Rendered objects
 * are animated by changing their coordinates every frame which creates animation.
 * Each test case is run for a duration of {@link #TEST_TIME} and number of frames rendered
 * are tracked to calculate FPS.
 *
 * <p>
 * Steps to run the application:
 * <ol>
 *  <li>cd RenderPerfTest/src</li>
 *  <li>Command to compile the program: javac {@literal @}{@literal <}path_to{@literal >}/compile.args renderperf/{@link RenderPerfTest}.java</li>
 *  <li>Command to execute the program: java {@literal @}{@literal <}path_to{@literal >}/run.args renderperf/{@link RenderPerfTest} -t {@literal <}test_name{@literal <} -n {@literal <}number_of_objects{@literal <} -h</li>
 *  Where:
 *  <ul>
 *      <li>test_name: Name of the test to be executed. If not specified, all tests are executed.</li>
 *      <li>number_of_objects: Number of objects to be rendered in the test. If not specified, default value is 1000.</li>
 *      <li>-h: help: prints application usage.</li>
 *  </ul>
 * NOTE: Set JVM command line parameter -Djavafx.animation.fullspeed=true to run animation at full speed
 * </ol>
 * <p>
 * Example - Command to execute the Circle test with 10000 circle objects: <br>
 * java -Djavafx.animation.fullspeed=true {@literal @}{@literal <}path_to{@literal >}/run.args renderperf/{@link RenderPerfTest} -t Circle -n 10000.
 *
 */

public class RenderPerfTest {
    private static final double WIDTH = 800;
    private static final double HEIGHT = 800;
    private static final double R = 25;
    private static final long SECOND_IN_NANOS = 1_000_000_000L;
    private static final long WARMUP_TIME = 5 * SECOND_IN_NANOS;
    private static final long TEST_TIME = 10 * SECOND_IN_NANOS;
    private static final Color[] marker = {Color.RED, Color.BLUE, Color.GREEN, Color.YELLOW, Color.ORANGE, Color.MAGENTA};

    private static Particles balls;
    private static Stage stage;
    private static Scene scene;
    private static Group group;
    private static Random random = new Random(100);
    private static int objectCount = 0;
    private static ArrayList<String> testList = null;

    interface Renderable {
        void addComponents(Group node);
        void updateCoordinates();
        void updateComponentCoordinates();
        void releaseResource();
    }

    /**
     * {@link Particles} class keeps track of the coordinates of the rendered components.
     * It also determines the delta by which the object coordinates shall be changed in each frame.
     *
     * The coordinates of the objects are generated randomly within
     * the width and height of the JavaFX window.
     * A random number between -2 and 2 determines the delta by which the object will be
     * moved in each frame.
     * These values are stored in arrays. These arrays are used as parameters to
     * {@link ParticleRenderer} methods.
     */

    static class Particles {
        private double[] bx;
        private double[] by;
        private double[] vx;
        private double[] vy;
        private double r;
        private int n;

        private double width;
        private double height;

        Particles(int n, double r, double width, double height) {
            bx = new double[n];
            by = new double[n];
            vx = new double[n];
            vy = new double[n];
            this.n = n;
            this.r = r;
            this.width = width;
            this.height = height;

            for (int i = 0; i < n; i++) {
                bx[i] = random.nextDouble(r, (width - r));
                by[i] = random.nextDouble(r, (height - r));
                vx[i] = random.nextDouble(-2, 2);
                vy[i] = random.nextDouble(-2, 2);
            }
        }

        void addComponents(Group node, ParticleRenderer renderer) {
            renderer.addComponents(node, n, bx, by, vx, vy);
        }

        void updateCoordinates() {
            for (int i = 0; i < n; i++) {
                bx[i] += vx[i];
                if (bx[i] + r > width || bx[i] - r < 0) vx[i] = -vx[i];
                by[i] += vy[i];
                if (by[i] + r > height || by[i] - r < 0) vy[i] = -vy[i];
            }
        }

        void updateComponentCoordinates(ParticleRenderer renderer) {
            renderer.updateComponentCoordinates(n, bx, by, vx, vy);
        }
    }

    /**
     * This method is used to create object of {@link ParticleRenderable}
     * which will be used to call the {@link PerfMeter#exec} method of
     * {@link PerfMeter} to render the component the components or update coordinates.
     * This method shall be called for each teast case.
     *
     * @param renderer object of a particle renderer which inherits from {@link ParticleRenderer}
     *
     * @return object of {@link ParticleRenderable}
     */
    ParticleRenderable createPR(ParticleRenderer renderer) {
        return new ParticleRenderable(renderer);
    }

    /**
     * Primary function of {@link ParticleRenderable} class is to invoke
     * the methods of {@link Particles} class which in turn invokes
     * {@link ParticleRenderable} methods to render components on JavaFX application.
     * Object of {@link ParticleRenderable} is created for every {@link ParticleRenderable}
     * child class i.e for each test case which makes it easy to reuse the
     * coordinates generated for rendering the components.
     * This class helps in separating the individual test case component rendering code
     * independant of the test execution.
     */
    static class ParticleRenderable implements Renderable {
        ParticleRenderer renderer;

        ParticleRenderable(ParticleRenderer renderer) {
            this.renderer = renderer;
        }

        @Override
        public void addComponents(Group node) {
            balls.addComponents(node, renderer);
        }

        @Override
        public void updateCoordinates() {
            balls.updateCoordinates();
        }

        @Override
        public void updateComponentCoordinates() {
            balls.updateComponentCoordinates(renderer);
        }

        @Override
        public void releaseResource() {
            renderer.releaseResource();
        }
    }

    /**
     * Interface which shall be implemented by all the particle renderer sub-classes
     * used in different test cases.
     * Methods for adding components, changing location of components and
     * releasing resources used for rendering the components are defined.
     */
    interface ParticleRenderer {
        void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy);
        void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy);
        void releaseResource();
    }

    static abstract class FlatParticleRenderer implements ParticleRenderer {
        Color[] colors;
        double r;

        FlatParticleRenderer(int n, double r) {
            colors = new Color[n];
            this.r = r;

            for (int i = 0; i < n; i++) {
                colors[i] = Color.rgb(random.nextInt(256), random.nextInt(256), random.nextInt(256));
            }
        }

        public void releaseResource() {
            colors = null;
        }
    }

    static class ArcRenderer extends FlatParticleRenderer {
        Arc[] arc;

        ArcRenderer(int n, double r) {
            super(n, r);
            arc = new Arc[n];
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                arc[id] = new Arc();

                arc[id].setCenterX(x[id]);
                arc[id].setCenterY(y[id]);
                arc[id].setRadiusX(r);
                arc[id].setRadiusY(r);
                arc[id].setStartAngle(random.nextDouble(100));
                arc[id].setLength(random.nextDouble(360));
                arc[id].setType(ArcType.ROUND);
                arc[id].setFill(colors[id % colors.length]);
                node.getChildren().add(arc[id]);
            }
        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                arc[id].setCenterX(x[id]);
                arc[id].setCenterY(y[id]);
            }
        }

        public void releaseResource() {
            super.releaseResource();
            arc = null;
        }
    }

    static class CubicCurveRenderer extends FlatParticleRenderer {
        CubicCurve[] cubicCurve;

        CubicCurveRenderer(int n, double r) {
            super(n, r);
            cubicCurve = new CubicCurve[n];
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                cubicCurve[id] = new CubicCurve();

                cubicCurve[id].setStartX(0);
                cubicCurve[id].setStartY(50);
                cubicCurve[id].setControlX1(25);
                cubicCurve[id].setControlY1(0);
                cubicCurve[id].setControlX2(75);
                cubicCurve[id].setControlY2(100);
                cubicCurve[id].setEndX(100);
                cubicCurve[id].setEndY(50);
                cubicCurve[id].setFill(colors[id % colors.length]);
                node.getChildren().add(cubicCurve[id]);
            }

        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                cubicCurve[id].setTranslateX(x[id] - r);
                cubicCurve[id].setTranslateY(y[id] - (2 * r));
            }
        }

        public void releaseResource() {
            super.releaseResource();
            cubicCurve = null;
        }
    }

    static class CircleRenderer extends FlatParticleRenderer {
        Circle[] circle;

        CircleRenderer(int n, double r) {
            super(n, r);
            circle = new Circle[n];
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                circle[id] = new Circle();

                circle[id].setCenterX(x[id]);
                circle[id].setCenterY(y[id]);
                circle[id].setRadius(r);
                circle[id].setFill(colors[id % colors.length]);
                node.getChildren().add(circle[id]);
            }
        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                circle[id].setCenterX(x[id]);
                circle[id].setCenterY(y[id]);
            }
        }

        public void releaseResource() {
            super.releaseResource();
            circle = null;
        }
    }

    static class CircleRendererRH extends CircleRenderer {

        CircleRendererRH(int n, double r) {
            super(n, r);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                circle[id].setSmooth(false);
            }
        }
    }

    static class CircleRendererBlendMultiply extends CircleRenderer {

        CircleRendererBlendMultiply(int n, double r) {
            super(n, r);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id += 10) {
                circle[id].setBlendMode(BlendMode.MULTIPLY);
            }
        }
    }

    static class CircleRendererBlendAdd extends CircleRenderer {

        CircleRendererBlendAdd(int n, double r) {
            super(n, r);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id += 10) {
                circle[id].setBlendMode(BlendMode.ADD);
            }
        }
    }

    static class CircleRendererBlendDarken extends CircleRenderer {

        CircleRendererBlendDarken(int n, double r) {
            super(n, r);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id += 10) {
                circle[id].setBlendMode(BlendMode.DARKEN);
            }
        }
    }

    static class StrokedCircleRenderer extends CircleRenderer {

        StrokedCircleRenderer(int n, double r) {
            super(n, r);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                circle[id].setFill(null);
                circle[id].setStroke(colors[id % colors.length]);
            }
        }
    }

    static class LinGradCircleRenderer extends CircleRenderer {
        Stop[] stops;
        LinearGradient linGradient;

        LinGradCircleRenderer(int n, double r) {
            super(n, r);
            stops = new Stop[] { new Stop(0, Color.BLACK), new Stop(1, Color.RED)};
            linGradient = new LinearGradient(0, 0, 1, 0, true, CycleMethod.NO_CYCLE, stops);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                circle[id].setFill(linGradient);
            }
        }
    }

    static class RadGradCircleRenderer extends CircleRenderer {
        Stop[] stops;
        RadialGradient radGradient;

        RadGradCircleRenderer(int n, double r) {
            super(n, r);
            stops = new Stop[] { new Stop(0.0, Color.WHITE), new Stop(0.1, Color.RED), new Stop(1.0, Color.DARKRED)};
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                radGradient = new RadialGradient(0, 0, x[id], y[id], 60, false, CycleMethod.NO_CYCLE, stops);
                circle[id].setFill(radGradient);
            }
        }
    }

    static class EllipseRenderer extends FlatParticleRenderer {
        Ellipse[] ellipse;

        EllipseRenderer(int n, double r) {
            super(n, r);
            ellipse = new Ellipse[n];
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                ellipse[id] = new Ellipse();

                ellipse[id].setCenterX(x[id]);
                ellipse[id].setCenterY(y[id]);
                ellipse[id].setRadiusX(2 * r);
                ellipse[id].setRadiusY(r);
                ellipse[id].setFill(colors[id % colors.length]);
                node.getChildren().add(ellipse[id]);
            }
        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                ellipse[id].setCenterX(x[id]);
                ellipse[id].setCenterY(y[id]);
            }
        }

        public void releaseResource() {
            super.releaseResource();
            ellipse = null;
        }
    }

    static class RectangleRenderer extends FlatParticleRenderer {
        Rectangle[] rectangle;

        RectangleRenderer(int n, double r) {
            super(n, r);
            rectangle = new Rectangle[n];
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                rectangle[id] = new Rectangle();

                rectangle[id].setX(x[id] - r);
                rectangle[id].setY(y[id] - r);
                rectangle[id].setWidth(2 * r);
                rectangle[id].setHeight(2 * r);
                rectangle[id].setFill(colors[id % colors.length]);
                node.getChildren().add(rectangle[id]);
            }
        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                rectangle[id].setX(x[id] - r);
                rectangle[id].setY(y[id] - r);
            }
        }

        public void releaseResource() {
            super.releaseResource();
            rectangle = null;
        }
    }

    static class RectangleRendererRH extends RectangleRenderer {

        RectangleRendererRH(int n, double r) {
            super(n, r);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                rectangle[id].setSmooth(false);
                rectangle[id].setRotate(45);
            }
        }
    }

    static class StrokedRectangleRenderer extends RectangleRenderer {

        StrokedRectangleRenderer(int n, double r) {
            super(n, r);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                rectangle[id].setFill(null);
                rectangle[id].setStroke(colors[id % colors.length]);
            }
        }
    }

    static class Box3DRenderer extends FlatParticleRenderer {
        Box[] box;

        Box3DRenderer(int n, double r) {
            super(n, r);
            box = new Box[n];
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                box[id] = new Box(2 * r, 2 * r, 2 * r);

                box[id].setTranslateX(x[id]);
                box[id].setTranslateY(y[id]);

                PhongMaterial material = new PhongMaterial();
                material.setDiffuseColor(colors[id % colors.length]);
                box[id].setMaterial(material);

                box[id].setRotationAxis(new Point3D(1, 1, 1));
                box[id].setRotate(45);
                node.getChildren().add(box[id]);
            }
        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                box[id].setTranslateX(x[id]);
                box[id].setTranslateY(y[id]);
            }
        }

        public void releaseResource() {
            super.releaseResource();
            box = null;
        }
    }


    static class WhiteTextRenderer implements ParticleRenderer {
        double r;
        Text[] text;

        WhiteTextRenderer(int n, double r) {
            this.r = r;
            text = new Text[n];
        }

        void setPaint(Text t, int id) {
            t.setFill(Color.WHITE);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                text[id] = new Text(x[id], y[id], "The quick brown fox jumps over the lazy dog");
                setPaint(text[id], id);
                node.getChildren().add(text[id]);
            }
        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                text[id].setX(x[id]);
                text[id].setY(y[id]);
            }
        }

        public void releaseResource() {
            text = null;
        }
    }

    static class ColorTextRenderer extends WhiteTextRenderer {
        Color[] colors;

        ColorTextRenderer(int n, double r) {
            super(n, r);
            colors = new Color[n];

            for (int i = 0; i < n; i++) {
                colors[i] = Color.rgb(random.nextInt(256), random.nextInt(256), random.nextInt(256));
            }
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                text[id].setFill(colors[id % colors.length]);
            }
        }
    }

    static class LargeTextRenderer extends WhiteTextRenderer {
        Font font;

        LargeTextRenderer(int n, double r) {
            super(n, r);
            font = new Font(48);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                text[id].setFont(font);
            }
        }
    }

    static class LargeColorTextRenderer extends ColorTextRenderer {
        Font font;

        LargeColorTextRenderer(int n, double r) {
            super(n, r);
            font = new Font(48);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                text[id].setFont(font);
            }
        }
    }

    static class ImageRenderer implements ParticleRenderer {
        ImageView[] dukeImg;
        Image image;
        double r;

        ImageRenderer(int n, double r) {
            this.r = r;
            try {
                String url = RenderPerfTest.class.getResource("duke.png").toString();
                image = new Image(url);
                dukeImg = new ImageView[n];
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                dukeImg[id] = new ImageView();
                dukeImg[id].setImage(image);
                dukeImg[id].setX(x[id] - r);
                dukeImg[id].setY(y[id] - 2 * r);

                node.getChildren().add(dukeImg[id]);
            }
        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                dukeImg[id].setX(x[id] - r);
                dukeImg[id].setY(y[id] - 2 * r);
            }
        }

        public void releaseResource() {
            image = null;
            dukeImg = null;
        }
    }

    static class ImageRendererRH extends ImageRenderer {

        ImageRendererRH(int n, double r) {
            super(n, r);
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            super.addComponents(node, n, x, y, vx, vy);
            for (int id = 0; id < n; id++) {
                dukeImg[id].setSmooth(false);
            }
        }
    }

    static class MultiShapeRenderer extends FlatParticleRenderer {
        Circle[] circle;
        Rectangle[] rectangle;
        Arc[] arc;
        Ellipse[] ellipse;

        MultiShapeRenderer(int n, double r) {
            super(n, r);
            circle = new Circle[n / 4];
            rectangle = new Rectangle[n / 4];
            arc = new Arc[n / 4];
            ellipse = new Ellipse[n / 4];
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            int index =0;
             for (int id = 0; id < n / 4; id++) {
                circle[id] = new Circle();

                circle[id].setCenterX(x[index]);
                circle[id].setCenterY(y[index]);
                circle[id].setRadius(r);
                circle[id].setFill(colors[index % colors.length]);
                node.getChildren().add(circle[id]);
                index++;

                rectangle[id] = new Rectangle();

                rectangle[id].setX(x[index] - r);
                rectangle[id].setY(y[index] - r);
                rectangle[id].setWidth(2 * r);
                rectangle[id].setHeight(2 * r);
                rectangle[id].setFill(colors[index % colors.length]);
                node.getChildren().add(rectangle[id]);
                index++;

                arc[id] = new Arc();

                arc[id].setCenterX(x[index]);
                arc[id].setCenterY(y[index]);
                arc[id].setRadiusX(r);
                arc[id].setRadiusY(r);
                arc[id].setStartAngle(random.nextDouble(100));
                arc[id].setLength(random.nextDouble(360));
                arc[id].setType(ArcType.ROUND);
                arc[id].setFill(colors[index % colors.length]);
                node.getChildren().add(arc[id]);
                index++;

                ellipse[id] = new Ellipse();

                ellipse[id].setCenterX(x[index]);
                ellipse[id].setCenterY(y[index]);
                ellipse[id].setRadiusX(2 * r);
                ellipse[id].setRadiusY(r);
                ellipse[id].setFill(colors[index % colors.length]);
                node.getChildren().add(ellipse[id]);
                index++;
            }
        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            int index = 0;
            for (int id = 0; id < n / 4; id++) {
                circle[id].setCenterX(x[index]);
                circle[id].setCenterY(y[index]);
                index++;

                rectangle[id].setX(x[index] - r);
                rectangle[id].setY(y[index] - r);
                index++;

                arc[id].setCenterX(x[index]);
                arc[id].setCenterY(y[index]);
                index++;

                ellipse[id].setCenterX(x[index]);
                ellipse[id].setCenterY(y[index]);
                index++;
            }
        }

        public void releaseResource() {
            circle = null;
            rectangle = null;
            arc = null;
            ellipse = null;
        }
    }

    static class ButtonRenderer implements ParticleRenderer {
        double r;
        Button[] button;

        ButtonRenderer(int n, double r) {
            this.r = r;
            button = new Button[n];
        }

        @Override
        public void addComponents(Group node, int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                button[id] = new Button();
                button[id].setText(String.valueOf(id));
                button[id].setLayoutX(x[id]);
                button[id].setLayoutY(y[id]);
                node.getChildren().add(button[id]);
            }
        }

        public void updateComponentCoordinates(int n, double[] x, double[] y, double[] vx, double[] vy) {
            for (int id = 0; id < n; id++) {
                button[id].setLayoutX(x[id]);
                button[id].setLayoutY(y[id]);
            }
        }

        public void releaseResource() {
            button = null;
        }
    }

    /**
     * {@link PerfMeter} is the class which runs each test.
     * This uses the JavaFX applcation environment created and invokes {@link ParticleRenderable}
     * methods to render component and animate. The values details required to calculate
     * FPS is also tracked in this class.
     */
    static class PerfMeter {
        private String name;

        private int frames = 0;
        AnimationTimer frameRateMeter;

        long startTime = 0;
        long endTime = 0;
        boolean warmUp = true;

        PerfMeter(String name) {
            this.name = name;
        }

        /**
         * The method which invokes {@link ParticleRenderable} methods for rendering and
         * moving the components on the JavaFX application. Same JavaFX application
         * environment is used for all the test cases.
         * This method warms up the test environment and then runs animation to calculate FPS.
         * The overridden {@link AnimationTimer#handle} method, gets invoked for each frame
         * which keeps track of the number of frames rendered, duration of the test case
         * to calculate FPS value.
         *
         * @params  renderable
         *          object of {@link Renderable}
         */
        void exec(final Renderable renderable) throws Exception {
            final CountDownLatch startupLatch = new CountDownLatch(1);
            final CountDownLatch stopLatch = new CountDownLatch(1);
            final CountDownLatch stageHiddenLatch = new CountDownLatch(1);

            Platform.runLater(() -> {
                group = new Group();
                renderable.addComponents(group);
                scene = new Scene(group, WIDTH, HEIGHT);
                scene.setFill(Color.BLACK);

                stage.setScene(scene);
                stage.setAlwaysOnTop(true);
                stage.setOnShown(event -> Platform.runLater(startupLatch::countDown));
                stage.setOnHidden(event -> Platform.runLater(stageHiddenLatch::countDown));

                stage.show();

                frameRateMeter = new AnimationTimer() {
                    @Override
                    public void handle(long now) {
                        if (startTime == 0) {
                            startTime = now;
                        }

                        if (warmUp && (now >= startTime + WARMUP_TIME)) {
                            startTime = now;
                            frames = 0;
                            warmUp = false;
                        }

                        moveComponents(renderable);

                        if (!warmUp) {
                            frames++;
                            if (now >= startTime + TEST_TIME) {
                                endTime = now;
                                stopLatch.countDown();
                            }
                        }
                    }
                };
            });

            if (!startupLatch.await(20, TimeUnit.SECONDS)) {
                throw new RuntimeException("Timeout waiting for stage to load.");
            }
            frameRateMeter.start();

            if (!stopLatch.await(20, TimeUnit.SECONDS)) {
                throw new RuntimeException("Timeout waiting for test execution completion.");
            }
            frameRateMeter.stop();

            reportFPS(startTime, endTime);
            Platform.runLater(() -> {
                renderable.releaseResource();
                stage.hide();
            });

            if (!stageHiddenLatch.await(20, TimeUnit.SECONDS)) {
                throw new RuntimeException("Timeout waiting for stage to get hidden.");
            }
            startTime = 0;
            endTime = 0;
            warmUp = true;
        }

        void moveComponents(final Renderable renderable){
            Platform.runLater(() -> {
                renderable.updateCoordinates();
                renderable.updateComponentCoordinates();
            });
        }

        void reportFPS(long startTime, long endTime) {
            long elapsedNanos = endTime - startTime;
            double elapsedNanosPerFrame = (double) elapsedNanos / frames;
            double frameRate = SECOND_IN_NANOS / elapsedNanosPerFrame;
            System.out.println(String.format("%s (Objects Frames FPS), %d, %d, %.3f", name, objectCount, frames, frameRate));
        }
    }



    public void testArc() throws Exception {
        (new PerfMeter("Arc")).exec(createPR(new ArcRenderer(objectCount, R)));
    }

    public void testCubicCurve() throws Exception {
        (new PerfMeter("CubicCurve")).exec(createPR(new CubicCurveRenderer(objectCount, R)));
    }

    public void testCircle() throws Exception {
        (new PerfMeter("Circle")).exec(createPR(new CircleRenderer(objectCount, R)));
    }

    public void testCircleRH() throws Exception {
        (new PerfMeter("CircleRH")).exec(createPR(new CircleRendererRH(objectCount, R)));
    }

    public void testCircleBlendMultiply() throws Exception {
        (new PerfMeter("CircleBlendMultiply")).exec(createPR(new CircleRendererBlendMultiply(objectCount, R)));
    }

    public void testCircleBlendAdd() throws Exception {
        (new PerfMeter("CircleBlendAdd")).exec(createPR(new CircleRendererBlendAdd(objectCount, R)));
    }

    public void testCircleBlendDarken() throws Exception {
        (new PerfMeter("CircleBlendDarken")).exec(createPR(new CircleRendererBlendDarken(objectCount, R)));
    }

    public void testStrokedCircle() throws Exception {
        (new PerfMeter("StrokedCircle")).exec(createPR(new StrokedCircleRenderer(objectCount, R)));
    }

    public void testLinGradCircle() throws Exception {
        (new PerfMeter("LinGradCircle")).exec(createPR(new LinGradCircleRenderer(objectCount, R)));
    }

    public void testRadGradCircle() throws Exception {
        (new PerfMeter("RadGradCircle")).exec(createPR(new RadGradCircleRenderer(objectCount, R)));
    }

    public void testEllipse() throws Exception {
        (new PerfMeter("Ellipse")).exec(createPR(new EllipseRenderer(objectCount, R)));
    }

    public void testRectangle() throws Exception {
        (new PerfMeter("Rectangle")).exec(createPR(new RectangleRenderer(objectCount, R)));
    }

    public void testRotatedRectangleRH() throws Exception {
        (new PerfMeter("RotatedRectangleRH")).exec(createPR(new RectangleRendererRH(objectCount, R)));
    }

    public void testStrokedRectangle() throws Exception {
        (new PerfMeter("StrokedRectangle")).exec(createPR(new StrokedRectangleRenderer(objectCount, R)));
    }

    public void testWhiteText() throws Exception {
        (new PerfMeter("WhiteText")).exec(createPR(new WhiteTextRenderer(objectCount, R)));
    }

    public void testColorText() throws Exception {
        (new PerfMeter("ColorText")).exec(createPR(new ColorTextRenderer(objectCount, R)));
    }

    public void testLargeText() throws Exception {
        (new PerfMeter("LargeText")).exec(createPR(new LargeTextRenderer(objectCount, R)));
    }

    public void testLargeColorText() throws Exception {
        (new PerfMeter("LargeColorText")).exec(createPR(new LargeColorTextRenderer(objectCount, R)));
    }

    public void testImage() throws Exception {
        (new PerfMeter("Image")).exec(createPR(new ImageRenderer(objectCount, R)));
    }

    public void testImageRH() throws Exception {
        (new PerfMeter("ImageRH")).exec(createPR(new ImageRendererRH(objectCount, R)));
    }

    public void test3DBox() throws Exception {
        (new PerfMeter("3DBox")).exec(createPR(new Box3DRenderer(objectCount, R)));
    }

    public void testMultiShape() throws Exception {
        (new PerfMeter("MultiShape")).exec(createPR(new MultiShapeRenderer(objectCount, R)));
    }

    public void testButton() throws Exception {
        (new PerfMeter("Button")).exec(createPR(new ButtonRenderer(objectCount, R)));
    }

    /**
     * Initialize all the {@link ParticleRenderer} objects with
     * child classes to render different components.
     */
    public void initializeRenderers(int n) {
        balls = new Particles(n, R, WIDTH, HEIGHT);
    }

    /**
     * Initialize the JavaFX application environment.
     * Once the stage is initialized, all tests use
     * same environment for execution.
     */
    public void intializeFxEnvironment() {
        Platform.startup(() -> {
            stage = new Stage();
            Platform.setImplicitExit(false);
        });
    }

    public void exitFxEnvironment() {
        Platform.exit();
    }

    public boolean parseCmdOptions(String[] args) {
        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            switch(arg) {
            case "-t":
                while ((i+1) < args.length && args[i + 1].charAt(0) != '-') {
                    testList.add(args[++i]);
                }
                if (testList.size() == 0) return false;
                break;
            case "-n":
                try {
                    objectCount = Integer.parseInt(args[++i]);
                }
                catch (ArrayIndexOutOfBoundsException e) {
                    System.out.println("\nnumber_of_objects not provided.");
                    return false;
                }
                break;
            case "-h":
            case "--help":
            default:
                return false;
            }
        }
        return true;
    }

    public static void printTests() {
        Method[] methods = RenderPerfTest.class.getDeclaredMethods();
        System.out.println("\nSupported tests:");
        for (Method m : methods) {
            if (m.getName().startsWith("test")) {
                System.out.println(m.getName().replaceFirst("test", ""));
            }
        }
    }

    public static void printUsage() {
        System.out.println("Usage: java @<path_to>/run.args RenderPerfTest -t <test_name> -n <number_of_objects> -h");
        System.out.println("       Where test_name: Name of the test to be executed");
        System.out.println("             number_of_objects: Number of objects to be rendered in the test");
        System.out.println("             -h: help: print application usage");
        System.out.println("NOTE: Set JVM command line parameter -Djavafx.animation.fullspeed=true to run animation at full speed");

        printTests();
    }

    public static void main(String[] args) throws Exception {
        RenderPerfTest test = new RenderPerfTest();

        test.intializeFxEnvironment();
        testList = new ArrayList<String>();

        if (!test.parseCmdOptions(args)) {
            printUsage();
            test.exitFxEnvironment();
            return;
        }

        if (test.objectCount == 0) {
            test.objectCount = 1000;
        }
        test.initializeRenderers(test.objectCount);

        try {
            if (testList.size() != 0) {
                for(String testName: testList) {
                    Method m = RenderPerfTest.class.getDeclaredMethod("test" + testName);
                    m.invoke(test);
                }
            } else {
                Method[] methods = RenderPerfTest.class.getDeclaredMethods();
                for (Method m : methods) {
                    if (m.getName().startsWith("test")) {
                        m.invoke(test);
                    }
                }
            }
        } catch (NoSuchMethodException e) {
            System.out.println("\nIncorrect Test Name!");
            printTests();
        } catch (Exception e) {
            System.out.println("\nUnexpected error occurred");
            e.printStackTrace();
        }
        test.exitFxEnvironment();
    }
}
