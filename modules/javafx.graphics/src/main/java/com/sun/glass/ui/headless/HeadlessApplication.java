package com.sun.glass.ui.headless;

import com.sun.glass.ui.Application;
import com.sun.glass.ui.CommonDialogs;
import com.sun.glass.ui.Cursor;
import com.sun.glass.ui.GlassRobot;
import com.sun.glass.ui.Pixels;
import com.sun.glass.ui.Screen;
import com.sun.glass.ui.Size;
import com.sun.glass.ui.Timer;
import com.sun.glass.ui.View;
import com.sun.glass.ui.Window;
import java.io.File;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Objects;

public class HeadlessApplication extends Application {

    private final NestedRunnableProcessor processor = new NestedRunnableProcessor();
    private HeadlessWindow window;
    private HeadlessCursor cursor;

    private final int MULTICLICK_MAX_X = 20;
    private final int MULTICLICK_MAX_Y = 20;
    private final long MULTICLICK_TIME = 500;
    private HeadlessRobot robot;
    private Screen[] screens = null;

    protected HeadlessApplication() {
        super();
    }

    @Override
    protected void runLoop(Runnable launchable) {
        processor.invokeLater(launchable);
        Thread eventThread = new Thread(processor);
        setEventThread(eventThread);
        eventThread.start();
    }

    NestedRunnableProcessor getProcessor() {
        return this.processor;
    }

    @Override
    protected void finishTerminating() {
        if (processor != null) {
            processor.stopProcessing();
        }
        final Thread eventThread = getEventThread();
        if (eventThread != null) {
            setEventThread(null);
        }
        super.finishTerminating();
    }
    @Override
    protected void _invokeAndWait(Runnable runnable) {
        processor.invokeAndWait(runnable);
    }

    @Override
    protected void _invokeLater(Runnable runnable) {
        processor.invokeLater(runnable);
    }

    @Override
    protected Object _enterNestedEventLoop() {
        return processor.newRunLoop();
    }

    @Override
    protected void _leaveNestedEventLoop(Object retValue) {
        processor.leaveCurrentLoop(retValue);
    }

    @Override
    public Window createWindow(Window owner, Screen screen, int styleMask) {
        return new HeadlessWindow(owner, screen, styleMask);
    }

    @Override
    public View createView() {
        return new HeadlessView();
    }

    @Override
    public Cursor createCursor(int type) {
        this.cursor = new HeadlessCursor(type);
        return this.cursor;
    }

    public Cursor getCursor() {
        return this.cursor;
    }

    @Override
    public Cursor createCursor(int x, int y, Pixels pixels) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    protected void staticCursor_setVisible(boolean visible) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    protected Size staticCursor_getBestSize(int width, int height) {
        return new Size(16,16);
    }

    @Override
    public Pixels createPixels(int width, int height, ByteBuffer data) {
        return new HeadlessPixels(width, height, data);
    }

    @Override
    public Pixels createPixels(int width, int height, ByteBuffer data, float scalex, float scaley) {
        return new HeadlessPixels(width, height, data, scalex, scaley);
    }

    @Override
    public Pixels createPixels(int width, int height, IntBuffer data) {
        return new HeadlessPixels(width, height, data);
    }

    @Override
    public Pixels createPixels(int width, int height, IntBuffer data, float scalex, float scaley) {
        return new HeadlessPixels(width, height, data, scalex, scaley);
    }

    @Override
    protected int staticPixels_getNativeFormat() {
        return Pixels.Format.BYTE_BGRA_PRE;
    }

    @Override
    public GlassRobot createRobot() {
        this.robot = new HeadlessRobot(this, (HeadlessWindow) this.window);
        return this.robot;
    }

    @Override
    protected double staticScreen_getVideoRefreshPeriod() {
        return 0.;
    }

    @Override
    protected Screen[] staticScreen_getScreens() {
        if (this.screens == null) {
            float scaleX = 1.f;
            float scaleY = 1.f;
            String scale = AccessController.doPrivileged((PrivilegedAction<String>) ()
                    -> System.getProperty("glass.gtk.uiScale"));
            if (scale != null && !scale.isBlank()) {
                float scaleFloat = Float.parseFloat(scale);
                scaleX = scaleFloat;
                scaleY = scaleFloat;
            }
            Screen screen = new Screen(0, 32, 0, 0, 1000, 1000, 0, 0, 1000, 1000, 0, 0, 1000, 1000, 100, 100, 1f, 1f, scaleX, scaleY);
            this.screens = new Screen[1];
            this.screens[0] = screen;
        }
        return this.screens;
    }

    @Override
    public Timer createTimer(Runnable runnable) {
        return new HeadlessTimer(runnable);
    }

    @Override
    protected int staticTimer_getMinPeriod() {
        return 0;
    }

    @Override
    protected int staticTimer_getMaxPeriod() {
        return 1_000_000;
    }

    @Override
    protected CommonDialogs.FileChooserResult staticCommonDialogs_showFileChooser(Window owner, String folder, String filename, String title, int type, boolean multipleMode, CommonDialogs.ExtensionFilter[] extensionFilters, int defaultFilterIndex) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    protected File staticCommonDialogs_showFolderChooser(Window owner, String folder, String title) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    protected long staticView_getMultiClickTime() {
        return MULTICLICK_TIME;
    }

    @Override
    protected int staticView_getMultiClickMaxX() {
        return MULTICLICK_MAX_X;
    }

    @Override
    protected int staticView_getMultiClickMaxY() {
        return MULTICLICK_MAX_Y;
    }

    @Override
    protected boolean _supportsTransparentWindows() {
        return false;
    }

    @Override
    protected boolean _supportsUnifiedWindows() {
        return false;
    }

    @Override
    protected int _getKeyCodeForChar(char c, int hint) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    public boolean hasWindowManager() {
        return false;
    }
}
