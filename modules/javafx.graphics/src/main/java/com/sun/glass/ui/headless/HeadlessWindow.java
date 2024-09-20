package com.sun.glass.ui.headless;

import com.sun.glass.events.WindowEvent;
import com.sun.glass.ui.Cursor;
import com.sun.glass.ui.Pixels;
import com.sun.glass.ui.Screen;
import com.sun.glass.ui.View;
import com.sun.glass.ui.Window;
import java.util.concurrent.atomic.AtomicInteger;

public class HeadlessWindow extends Window {

    private int minWidth;
    private int minHeight;
    private int maxWidth = -1;
    private int maxHeight = -1;
    private int originalX, originalY, originalWidth, originalHeight;
    private boolean closed = false;
    private boolean visible = false;

    private static final AtomicInteger ptrCount = new AtomicInteger(0);
    public HeadlessWindow(Window owner, Screen screen, int styleMask) {
        super(owner, screen, styleMask);
    }

    @Override
    protected long _createWindow(long ownerPtr, long screenPtr, int mask) {
        int ptr = ptrCount.incrementAndGet();
        return ptr;
    }

    @Override
    protected boolean _close(long ptr) {
        this.closed = true;
        this.notifyDestroy();
        return true;
    }

    @Override
    protected boolean _setView(long ptr, View view) {
        return true;
    }

    @Override
    protected void _updateViewSize(long ptr) {
    }

    @Override
    protected boolean _setMenubar(long ptr, long menubarPtr) {
        return true;
    }

    @Override
    protected boolean _minimize(long ptr, boolean minimize) {
        return true;
    }

    @Override
    protected boolean _maximize(long ptr, boolean maximize, boolean wasMaximized) {
        int newX = 0;
        int newY = 0;
        int newWidth = 0;
        int newHeight = 0;
        if (maximize && !wasMaximized) {
            this.originalHeight = this.height;
            this.originalWidth = this.width;
            this.originalX = this.x;
            this.originalY = this.y;
            newX = 0;
            newY = 0;
            newWidth = screen.getWidth();
            newHeight = screen.getHeight();
            setState(State.MAXIMIZED);
        } else if (!maximize && wasMaximized) {
            newHeight = this.originalHeight;
            newWidth = this.originalWidth;
            newX = this.originalX;
            newY = this.originalY;
            setState(State.NORMAL);
        }
        notifyResizeAndMove(newX, newY, newWidth, newHeight);
        if (maximize) {
            notifyResize(WindowEvent.MAXIMIZE, newWidth, newHeight);
        }

        return maximize;
    }

    boolean setFullscreen(boolean full) {
        int newX = 0;
        int newY = 0;
        int newWidth = 0;
        int newHeight = 0;
        if (full) {
            this.originalHeight = this.height;
            this.originalWidth = this.width;
            this.originalX = this.x;
            this.originalY = this.y;
            newX = 0;
            newY = 0;
            newWidth = screen.getWidth();
            newHeight = screen.getHeight();
        } else  {
            newHeight = this.originalHeight;
            newWidth = this.originalWidth;
            newX = this.originalX;
            newY = this.originalY;
        }
        notifyResizeAndMove(newX, newY, newWidth, newHeight);

        return full;
    }

    @Override
    protected void _setBounds(long ptr, int x, int y, boolean xSet, boolean ySet, int w, int h, int cw, int ch, float xGravity, float yGravity) {
        int newWidth = 0;
        int newHeight = 0;
        if (w > 0) {
            //window newWidth surpass window content newWidth (cw)
            newWidth = w;
        } else if (cw > 0) {
            //content newWidth changed
            newWidth = cw;
        } else {
            //no explicit request to change newWidth, get default
            newWidth = getWidth();
        }

        if (h > 0) {
            //window newHeight surpass window content newHeight(ch)
            newHeight = h;
        } else if (ch > 0) {
            //content newHeight changed
            newHeight = ch;
        } else {
            //no explicit request to change newHeight, get default
            newHeight = getHeight();
        }
        if (!xSet) {
            x = getX();
        }
        if (!ySet) {
            y = getY();
        }
        if (maxWidth >= 0) {
            newWidth = Math.min(newWidth, maxWidth);
        }
        if (maxHeight >= 0) {
            newHeight = Math.min(newHeight, maxHeight);
        }
        newWidth = Math.max(newWidth, minWidth);
        newHeight = Math.max(newHeight, minHeight);
        notifyResizeAndMove(x, y, newWidth, newHeight);
    }

    private void notifyResizeAndMove(int x, int y, int width, int height) {
        HeadlessView view = (HeadlessView) getView();
        if (getWidth() != width || getHeight() != height) {
            notifyResize(WindowEvent.RESIZE, width, height);
            if (view != null) {
                view.notifyResize(width, height);
            }
        }
        if (getX() != x || getY() != y) {
            notifyMove(x, y);
        }
    }

    @Override
    protected boolean _setVisible(long ptr, boolean visible) {
        this.visible = visible;
        return this.visible;
    }

    @Override
    protected boolean _setResizable(long ptr, boolean resizable) {
        return true;
    }

    @Override
    protected boolean _requestFocus(long ptr, int event) {
        this.notifyFocus(event);
        return this.isFocused();
    }

    @Override
    protected void _setFocusable(long ptr, boolean isFocusable) {
    }

    @Override
    protected boolean _grabFocus(long ptr) {
        return true;
    }

    @Override
    protected void _ungrabFocus(long ptr) {
    }

    @Override
    protected boolean _setTitle(long ptr, String title) {
        return true;
    }

    @Override
    protected void _setLevel(long ptr, int level) {
    }

    @Override
    protected void _setAlpha(long ptr, float alpha) {
    }

    @Override
    protected boolean _setBackground(long ptr, float r, float g, float b) {
        return true;
    }

    @Override
    protected void _setEnabled(long ptr, boolean enabled) {
    }

    @Override
    protected boolean _setMinimumSize(long ptr, int width, int height) {
        this.minWidth = width;
        this.minHeight = height;
        return true;
    }

    @Override
    protected boolean _setMaximumSize(long ptr, int width, int height) {
        this.maxWidth = width;
        this.maxHeight = height;
        return true;
    }

    @Override
    protected void _setIcon(long ptr, Pixels pixels) {
    }

    @Override
    protected void _setCursor(long ptr, Cursor cursor) {
    }

    @Override
    protected void _toFront(long ptr) {
    }

    @Override
    protected void _toBack(long ptr) {
    }

    @Override
    protected void _enterModal(long ptr) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    protected void _enterModalWithWindow(long dialog, long window) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    protected void _exitModal(long ptr) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    protected void _requestInput(long ptr, String text, int type, double width, double height, double Mxx, double Mxy, double Mxz, double Mxt, double Myx, double Myy, double Myz, double Myt, double Mzx, double Mzy, double Mzz, double Mzt) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    protected void _releaseInput(long ptr) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    public long getNativeWindow() {
        return 0;
    }

}
