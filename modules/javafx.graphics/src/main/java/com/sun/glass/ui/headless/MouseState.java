package com.sun.glass.ui.headless;

import com.sun.glass.events.KeyEvent;
import com.sun.glass.events.MouseEvent;

/**
 * MouseState is a snapshot of mouse coordinates and which buttons are pressed.
 * MouseState is used both to store the current state of mouse input and to
 * describe changes to that state.
 */
class MouseState {

    static final int WHEEL_NONE = 0;
    static final int WHEEL_UP = 1;
    static final int WHEEL_DOWN = -1 ;

    private int x;
    private int y;
    private int wheel;
    private HeadlessWindow window;

    private IntSet buttonsPressed = new IntSet();

    int getX() {
        return x;
    }

    void setX(int x) {
        this.x = x;
    }

    int getY() {
        return y;
    }

    void setY(int y) {
        this.y = y;
    }

    int getWheel() {
        return wheel;
    }

    void setWheel(int wheel) {
        this.wheel = wheel;
    }

    void pressButton(int button) {
        buttonsPressed.addInt(button);
    }

    void releaseButton(int button) {
        buttonsPressed.removeInt(button);
    }

    /**
     * Returns the Glass window on which the coordinates of this state are located.
     * @param recalculateCache true if the cached value for the target window
     *                         should be recalculated; false if the cached
     *                         value should be used to determine the result
     *                         of this method.
     * @param fallback if the original window is null, or if no window can
     * be found, return the fallback window
     * @return the MonocleWindow at the top of the stack at the coordinates
     * described by this state object, or the fallback window in case the
     * current window is null or no window can be found for the supplied coordinates.
     */
    HeadlessWindow getWindow(boolean recalculateCache, HeadlessWindow fallback) {
        return fallback;
    }

    /** Returns the Glass button ID used for this state. */
    int getButton() {
        return buttonsPressed.isEmpty()
               ? MouseEvent.BUTTON_NONE
               : buttonsPressed.get(0);
    }

    /** Returns the Glass event modifiers for this state */
    int getModifiers() {
        int modifiers = KeyEvent.MODIFIER_NONE;
        for (int i = 0; i < buttonsPressed.size(); i++) {
            switch(buttonsPressed.get(i)) {
                case MouseEvent.BUTTON_LEFT:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_PRIMARY;
                    break;
                case MouseEvent.BUTTON_OTHER:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_MIDDLE;
                    break;
                case MouseEvent.BUTTON_RIGHT:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_SECONDARY;
                    break;
                case MouseEvent.BUTTON_BACK:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_BACK;
                    break;
                case MouseEvent.BUTTON_FORWARD:
                    modifiers |= KeyEvent.MODIFIER_BUTTON_FORWARD;
                    break;
            }
        }
        return modifiers;
    }

    /** Copies the contents of this state object to another.
     *
     * @param target The MouseState to which to copy this state's data.
     */
    void copyTo(MouseState target) {
        target.x = x;
        target.y = y;
        target.wheel = wheel;
        buttonsPressed.copyTo(target.buttonsPressed);
        target.window = window;
    }

    IntSet getButtonsPressed() {
        return buttonsPressed;
    }

    @Override
    public String toString() {
        return "MouseState[x="
                + x + ",y=" + y
                + ",wheel=" + wheel
                + ",buttonsPressed=" + buttonsPressed + "]";
    }

    /** Finds out whether two non-null states are identical in everything but
     * their coordinates
     *
     * @param ms the MouseState to compare to
     */
    boolean canBeFoldedWith(MouseState ms) {
        return ms.buttonsPressed.equals(buttonsPressed) && ms.wheel == wheel;
    }

}
