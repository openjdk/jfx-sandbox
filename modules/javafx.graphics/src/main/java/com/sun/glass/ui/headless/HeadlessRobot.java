package com.sun.glass.ui.headless;

import com.sun.glass.events.KeyEvent;
import com.sun.glass.events.MouseEvent;
import com.sun.glass.ui.Application;
import com.sun.glass.ui.GlassRobot;
import javafx.scene.input.KeyCode;
import javafx.scene.input.MouseButton;
import javafx.scene.paint.Color;

public class HeadlessRobot extends GlassRobot {

    private final HeadlessApplication application;
    private final MouseInput mouseInput;

    private HeadlessWindow window;
    private NestedRunnableProcessor processor;

    private double mouseX, mouseY;
    private int modifiers;

    public HeadlessRobot(HeadlessApplication application, HeadlessWindow window) {
        this.application = application;
        this.mouseInput = new MouseInput(application, window);
        this.window = window;
    }

    @Override
    public void create() {
    }

    @Override
    public void destroy() {
    }

    @Override
    public void keyPress(KeyCode keyCode) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    public void keyRelease(KeyCode keyCode) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
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
        this.mouseX = mouseX + x;
        this.mouseY = mouseY + y;
        MouseState state = new MouseState();
        mouseInput.getState(state);
        state.setX((int) x);
        state.setY((int) y);
        mouseInput.setState(state, false);
    }

    @Override
    public void mousePress(MouseButton... buttons) {
        Application.checkEventThread();
        MouseState state = new MouseState();
        mouseInput.getState(state);
        mouseInput.setState(convertToMouseState(true, state, buttons), false);
    }

    @Override
    public void mouseRelease(MouseButton... buttons) {
        Application.checkEventThread();
        MouseState state = new MouseState();
        mouseInput.getState(state);
        mouseInput.setState(convertToMouseState(false, state, buttons), false);
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

}
