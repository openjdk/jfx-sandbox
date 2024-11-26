package com.sun.glass.ui.headless;

import com.sun.glass.ui.Pixels;
import com.sun.glass.ui.View;
import com.sun.glass.events.ViewEvent;
import com.sun.glass.ui.Window;
import java.util.Map;

public class HeadlessView extends View {

    @Override
    protected void _enableInputMethodEvents(long ptr, boolean enable) {
    }

    @Override
    protected long _create(Map capabilities) {
        return 1;
    }

    @Override
    protected long _getNativeView(long ptr) {
        return ptr;
    }

    @Override
    protected int _getX(long ptr) {
        return 0;
    }

    @Override
    protected int _getY(long ptr) {
        return 0;
    }

    @Override
    protected void notifyResize(int width, int height) {
        super.notifyResize(width, height);
    }

    @Override
    protected void _setParent(long ptr, long parentPtr) {
    }

    @Override
    protected boolean _close(long ptr) {
        return true;
    }

    @Override
    protected void _scheduleRepaint(long ptr) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    protected void _begin(long ptr) {
    }

    @Override
    protected void _end(long ptr) {
    }

    @Override
    protected int _getNativeFrameBuffer(long ptr) {
        return 0;
    }

    @Override
    protected void _uploadPixels(long ptr, Pixels pixels) {
    }

    @Override
    protected boolean _enterFullscreen(long ptr, boolean animate, boolean keepRatio, boolean hideCursor) {
        HeadlessWindow window = (HeadlessWindow)this.getWindow();
        window.setFullscreen(true);
        notifyView(ViewEvent.FULLSCREEN_ENTER);
        return true;
    }

    @Override
    protected void _exitFullscreen(long ptr, boolean animate) {
        HeadlessWindow window = (HeadlessWindow)this.getWindow();
        if (window != null) {
            window.setFullscreen(false);
        }
        notifyView(ViewEvent.FULLSCREEN_EXIT);
    }

    @Override
    protected void notifyKey(int type, int keyCode, char[] keyChars, int modifiers) {
        super.notifyKey(type, keyCode, keyChars, modifiers);
    }

    @Override
    protected void notifyMouse(int type, int button,
            int x, int y, int xAbs, int yAbs, int modifiers,
            boolean isPopupTrigger, boolean isSynthesized) {
        super.notifyMouse(type, button, x, y, xAbs, yAbs, modifiers,
                isPopupTrigger,
                isSynthesized);
    }

    @Override
    protected void notifyScroll(int x, int y, int xAbs, int yAbs,
            double deltaX, double deltaY, int modifiers,
            int lines, int chars,
            int defaultLines, int defaultChars,
            double xMultiplier, double yMultiplier) {
        super.notifyScroll(x, y, xAbs, yAbs, deltaX, deltaY,
                modifiers, lines, chars,
                defaultLines, defaultChars, xMultiplier,
                yMultiplier);
    }

    @Override
    protected int notifyDragEnter(int x, int y, int absx, int absy, int recommendedDropAction) {
        return super.notifyDragEnter(x, y, absx, absy, recommendedDropAction);
    }

    @Override
    protected void notifyDragLeave() {
        super.notifyDragLeave();
    }

    @Override
    protected int notifyDragDrop(int x, int y, int absx, int absy, int recommendedDropAction) {
        return super.notifyDragDrop(x, y, absx, absy, recommendedDropAction);
    }

    @Override
    protected int notifyDragOver(int x, int y, int absx, int absy, int recommendedDropAction) {
        return super.notifyDragOver(x, y, absx, absy, recommendedDropAction);
    }

    @Override
    protected void notifyDragEnd(int performedAction) {
        super.notifyDragEnd(performedAction);
    }

    @Override
    public void uploadPixels(Pixels pixels) {
    }

}
