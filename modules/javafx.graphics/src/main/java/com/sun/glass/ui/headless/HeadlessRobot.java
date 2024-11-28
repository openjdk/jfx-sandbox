package com.sun.glass.ui.headless;

import com.sun.glass.events.KeyEvent;
import com.sun.glass.events.MouseEvent;
import com.sun.glass.ui.Application;
import com.sun.glass.ui.GlassRobot;
import com.sun.glass.ui.Window;
import java.util.ArrayList;
import java.util.List;
import javafx.scene.input.KeyCode;
import javafx.scene.input.MouseButton;
import javafx.scene.paint.Color;

public class HeadlessRobot extends GlassRobot {

    private final HeadlessApplication application;
    private final MouseInput mouseInput;

    private NestedRunnableProcessor processor;

    private double mouseX, mouseY;
    private int modifiers;
    
    private boolean keyControl = false;
    private boolean keyShift = false;
    private boolean keyCommand = false;
    private boolean keyAlt = false;

    public HeadlessRobot(HeadlessApplication application, HeadlessWindow window) {
        this.application = application;
        this.mouseInput = new MouseInput(application, window);
    }

    @Override
    public void create() {
    }

    @Override
    public void destroy() {
    }

    @Override
    public void keyPress(KeyCode keyCode) {
        checkWindowFocused();
        if (activeWindow == null) {
            return;
        }
        HeadlessView view = (HeadlessView) activeWindow.getView();
        int code = keyCode.getCode();
        processSpecialKeys(code, true);
        char[] keyval = getKeyChars(code);
        int mods = getKeyModifiers();
        if (view != null) {
            view.notifyKey(KeyEvent.PRESS, code, keyval, mods);
            if (keyval.length > 0) {
                view.notifyKey(KeyEvent.TYPED, 0, keyval, mods);
            }
        }
    }

    @Override
    public void keyRelease(KeyCode keyCode) {
        checkWindowFocused();
        if (activeWindow == null) return;
        HeadlessView view = (HeadlessView)activeWindow.getView();
        int code = keyCode.getCode();
        processSpecialKeys(code, false);
        int mods = getKeyModifiers();
        char[] keyval = new char[0];
        if (view != null) {
            view.notifyKey(KeyEvent.RELEASE, code, keyval, mods);
        }
    }

    @Override
    public double getMouseX() {
        return this.mouseX;
    }

    @Override
    public double getMouseY() {
        return this.mouseY;
    }

    @Override
    public void mouseMove(double x, double y) {
        this.mouseX = x;
        this.mouseY = y;
        checkWindowEnterExit();
        if (activeWindow == null) return;
        HeadlessView view = (HeadlessView)activeWindow.getView();
        if (view == null) return;
        int wx = activeWindow.getX();
        int wy = activeWindow.getY();
        view.notifyMouse(MouseEvent.MOVE, MouseEvent.BUTTON_NONE, (int)mouseX-wx, (int)mouseY-wy, (int)mouseX, (int)mouseY, modifiers, false, false);
    }

    @Override
    public void mousePress(MouseButton... buttons) {
        Application.checkEventThread();
        HeadlessView view = (HeadlessView)activeWindow.getView();
        if (view == null) {
            view = (HeadlessView)activeWindow.getView();
            if (view == null) {
                System.err.println("no view for this window, return");
            }
        }
        int wx = activeWindow.getX();
        int wy = activeWindow.getY();
        view.notifyMouse(MouseEvent.DOWN, MouseEvent.BUTTON_LEFT, (int)mouseX-wx, (int)mouseY-wy, (int)mouseX, (int)mouseY, modifiers, true, true);
    }

    @Override
    public void mouseRelease(MouseButton... buttons) {
        Application.checkEventThread();
        HeadlessView view = (HeadlessView) activeWindow.getView();
        int wx = activeWindow.getX();
        int wy = activeWindow.getY();
        view.notifyMouse(MouseEvent.UP, MouseEvent.BUTTON_LEFT, (int) mouseX - wx, (int) mouseY - wy, (int) mouseX, (int) mouseY, modifiers, true, true);
    }

    @Override
    public void mouseWheel(int wheelAmt) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    @Override
    public Color getPixelColor(double x, double y) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    int getModifiers(MouseButton... buttons) {
        int modifiers = KeyEvent.MODIFIER_NONE;
        for (int i = 0; i < buttons.length; i++) {
            switch (buttons[i]) {
                case PRIMARY:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_PRIMARY;
                    break;
                case MIDDLE:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_MIDDLE;
                    break;
                case SECONDARY:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_SECONDARY;
                    break;
                case BACK:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_BACK;
                    break;
                case FORWARD:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_FORWARD;
                    break;
            }
        }
        return modifiers;
    }

    private static MouseState convertToMouseState(boolean press, MouseState state, MouseButton... buttons) {
        for (MouseButton button : buttons) {
            switch (button) {
                case PRIMARY:
                    if (press) {
                        state.pressButton(MouseEvent.BUTTON_LEFT);
                    } else {
                        state.releaseButton(MouseEvent.BUTTON_LEFT);
                    }
                    break;
                case SECONDARY:
                    if (press) {
                        state.pressButton(MouseEvent.BUTTON_RIGHT);
                    } else {
                        state.releaseButton(MouseEvent.BUTTON_RIGHT);
                    }
                    break;
                case MIDDLE:
                    if (press) {
                        state.pressButton(MouseEvent.BUTTON_OTHER);
                    } else {
                        state.releaseButton(MouseEvent.BUTTON_OTHER);
                    }
                    break;
                case BACK:
                    if (press) {
                        state.pressButton(MouseEvent.BUTTON_BACK);
                    } else {
                        state.releaseButton(MouseEvent.BUTTON_BACK);
                    }
                    break;
                case FORWARD:
                    if (press) {
                        state.pressButton(MouseEvent.BUTTON_FORWARD);
                    } else {
                        state.releaseButton(MouseEvent.BUTTON_FORWARD);
                    }
                    break;
                default:
                    throw new IllegalArgumentException("MouseButton: " + button
                            + " not supported by Monocle Robot");
            }
        }
        return state;
    }

    private Window activeWindow = null;
    private void checkWindowFocused() {
        this.activeWindow = getFocusedWindow();
    }
    private void checkWindowEnterExit() {
        Window oldWindow = activeWindow;
        this.activeWindow = getTargetWindow(this.mouseX, this.mouseY);

        if (this.activeWindow == null) return;
        int wx = activeWindow.getX();
        int wy = activeWindow.getY();

        if (activeWindow != oldWindow) {
            HeadlessView view = (HeadlessView)activeWindow.getView();
            view.notifyMouse(MouseEvent.ENTER, MouseEvent.BUTTON_NONE, (int)mouseX-wx, (int)mouseY-wy, (int)mouseX, (int)mouseY, modifiers, true, true);
            if (oldWindow != null) {
                HeadlessView oldView = (HeadlessView)oldWindow.getView();
                if (oldView != null) {
                    int owx = oldWindow.getX();
                    int owy = oldWindow.getY();
                    oldView.notifyMouse(MouseEvent.EXIT, MouseEvent.BUTTON_NONE, (int)mouseX-owx, (int)mouseY-owy, (int)mouseX, (int)mouseY, modifiers, true, true);
                }
            }
        }
    }

    private HeadlessWindow getFocusedWindow() {
        List<Window> windows = Window.getWindows().stream()
                .filter(win -> win.getView()!= null)
                .filter(win -> !win.isClosed())
                .filter(win -> win.isFocused()).toList();
        if (windows.isEmpty()) return null;
        if (windows.size() == 1) return (HeadlessWindow)windows.get(0);
        return (HeadlessWindow)windows.get(windows.size() -1);
    }

    private HeadlessWindow getTargetWindow(double x, double y) {
        List<Window> windows = Window.getWindows().stream()
                .filter(win -> win.getView()!= null)
                .filter(win -> !win.isClosed())
                .filter(win -> (x >= win.getX() && x <= win.getX() + win.getWidth()
                        && y >= win.getY() && y <= win.getY()+ win.getHeight())).toList();
        if (windows.isEmpty()) return null;
        if (windows.size() == 1) return (HeadlessWindow)windows.get(0);
        return (HeadlessWindow)windows.get(windows.size() -1);
    }
    
        private boolean numLock = false;
    private boolean capsLock = false;
        private final char[] NO_CHAR = { };

    private char[] getKeyChars(int key) {
        char c = '\000';
        boolean shifted = this.keyShift;
        // TODO: implement configurable keyboard mappings.
        // The following is only for US keyboards
        if (key >= KeyEvent.VK_A && key <= KeyEvent.VK_Z) {
            shifted ^= capsLock;
            if (shifted) {
                c = (char) (key - KeyEvent.VK_A + 'A');
            } else {
                c = (char) (key - KeyEvent.VK_A + 'a');
            }
        } else if (key >= KeyEvent.VK_NUMPAD0 && key <= KeyEvent.VK_NUMPAD9) {
            if (numLock) {
                c = (char) (key - KeyEvent.VK_NUMPAD0 + '0');
            }
        } else if (key >= KeyEvent.VK_0 && key <= KeyEvent.VK_9) {
            if (shifted) {
                switch (key) {
                    case KeyEvent.VK_0: c = ')'; break;
                    case KeyEvent.VK_1: c = '!'; break;
                    case KeyEvent.VK_2: c = '@'; break;
                    case KeyEvent.VK_3: c = '#'; break;
                    case KeyEvent.VK_4: c = '$'; break;
                    case KeyEvent.VK_5: c = '%'; break;
                    case KeyEvent.VK_6: c = '^'; break;
                    case KeyEvent.VK_7: c = '&'; break;
                    case KeyEvent.VK_8: c = '*'; break;
                    case KeyEvent.VK_9: c = '('; break;
                }
            } else {
                c = (char) (key - KeyEvent.VK_0 + '0');
            }
        } else if (key == KeyEvent.VK_SPACE) {
            c = ' ';
        } else if (key == KeyEvent.VK_TAB) {
            c = '\t';
        } else if (key == KeyEvent.VK_ENTER) {
            c = (char)13;
        } else if (key == KeyEvent.VK_MULTIPLY) {
            c = '*';
        } else if (key == KeyEvent.VK_DIVIDE) {
            c = '/';
        } else if (shifted) {
            switch (key) {
                case KeyEvent.VK_BACK_QUOTE: c = '~'; break;
                case KeyEvent.VK_COMMA: c = '<'; break;
                case KeyEvent.VK_PERIOD: c = '>'; break;
                case KeyEvent.VK_SLASH: c = '?'; break;
                case KeyEvent.VK_SEMICOLON: c = ':'; break;
                case KeyEvent.VK_QUOTE: c = '\"'; break;
                case KeyEvent.VK_BRACELEFT: c = '{'; break;
                case KeyEvent.VK_BRACERIGHT: c = '}'; break;
                case KeyEvent.VK_BACK_SLASH: c = '|'; break;
                case KeyEvent.VK_MINUS: c = '_'; break;
                case KeyEvent.VK_EQUALS: c = '+'; break;
            }        } else {
            switch (key) {
                case KeyEvent.VK_BACK_QUOTE: c = '`'; break;
                case KeyEvent.VK_COMMA: c = ','; break;
                case KeyEvent.VK_PERIOD: c = '.'; break;
                case KeyEvent.VK_SLASH: c = '/'; break;
                case KeyEvent.VK_SEMICOLON: c = ';'; break;
                case KeyEvent.VK_QUOTE: c = '\''; break;
                case KeyEvent.VK_BRACELEFT: c = '['; break;
                case KeyEvent.VK_BRACERIGHT: c = ']'; break;
                case KeyEvent.VK_BACK_SLASH: c = '\\'; break;
                case KeyEvent.VK_MINUS: c = '-'; break;
                case KeyEvent.VK_EQUALS: c = '='; break;
            }
        }
        return c == '\000' ? NO_CHAR : new char[] { c };
    }

    private void processSpecialKeys(int c, boolean on) {
        if (c == KeyEvent.VK_CONTROL) {
            this.keyControl = on;
        } 
        if (c == KeyEvent.VK_SHIFT) {
            this.keyShift = on;
        }
        if (c == KeyEvent.VK_COMMAND) {
            this.keyCommand = on;
        }
        if (c == KeyEvent.VK_ALT) {
            this.keyAlt = on;
        }
    }

    private int getKeyModifiers() {
        int answer = 0;
        if (this.keyControl) answer = answer | KeyEvent.MODIFIER_CONTROL;
        if (this.keyShift) answer = answer | KeyEvent.MODIFIER_SHIFT;
        if (this.keyCommand) answer = answer | KeyEvent.MODIFIER_COMMAND;
        if (this.keyAlt) answer = answer | KeyEvent.MODIFIER_ALT;
        return answer;
    }
}
