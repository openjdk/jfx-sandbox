package com.sun.glass.ui.headless;

import com.sun.glass.events.MouseEvent;
import com.sun.glass.ui.Application;
import com.sun.glass.ui.Clipboard;

import java.util.BitSet;

/**
 * Processes mouse input events based on changes to mouse state. Not
 * thread-safe and can only be used on the JavaFX application thread.
 */
class MouseInput {

    private final HeadlessApplication application;
    private final HeadlessWindow window;
    private MouseState state = new MouseState();
    private IntSet buttons = new IntSet();

    /** Are we currently processing drag and drop? */
    private boolean dragInProgress = false;
    /** What button started the drag operation? */
    private int dragButton = MouseEvent.BUTTON_NONE;
    /** On what View is the drag operation currently over? */
    private HeadlessView dragView = null;
    /** What drag actions have been performed? */
    private BitSet dragActions = new BitSet();
    private static final int DRAG_ENTER = 1;
    private static final int DRAG_LEAVE = 2;
    private static final int DRAG_OVER = 3;
    private static final int DRAG_DROP = 4;

    MouseInput (HeadlessApplication application, HeadlessWindow window) {
        this.application = application;
        this.window = window;
    }

    /** Retrieves the current state of mouse buttons and of the cursor.
     *
     * @param result a MouseState to which to copy data on the current mouse
     *               buttons and coordinates.
     */
    void getState(MouseState result) {
        state.copyTo(result);
    }

    /**
     * Sets a new state for mouse buttons and coordinates, generating input
     * events where appropriate.
     *
     * @param newState    the new state
     * @param synthesized true if this state change is synthesized from a change
     *                    in touch state; false if this state change comes from
     *                    an actual relative pointing devices or from the Glass
     *                    robot.
     */
    void setState(MouseState newState, boolean synthesized) {
        int x = newState.getX();
        int y = newState.getY();
        boolean newAbsoluteLocation = state.getX() != x || state.getY() != y;
        HeadlessView view = (HeadlessView) window.getView();

        if (view == null) {
            newState.copyTo(state);
            return;
        }

        int relX = x - window.getX();
        int relY = y - window.getY();
        // send press events
        newState.getButtonsPressed().difference(buttons, state.getButtonsPressed());
        if (!buttons.isEmpty()) {
            MouseState pressState = new MouseState();
            state.copyTo(pressState);
            for (int i = 0; i < buttons.size(); i++) {
                int button = buttons.get(i);
                pressState.pressButton(button);
                KeyState keyState = new KeyState();
                int modifiers = pressState.getModifiers() | keyState.getModifiers();
                // send press event
                boolean isPopupTrigger = false; // TODO
                postMouseEvent(view, MouseEvent.DOWN, button,
                               relX, relY, x, y,
                               modifiers, isPopupTrigger,
                               synthesized);
            }
        }
        buttons.clear();
        // send release events
        state.getButtonsPressed().difference(buttons,
                                             newState.getButtonsPressed());
        if (!buttons.isEmpty()) {
            MouseState releaseState = new MouseState();
            state.copyTo(releaseState);
            for (int i = 0; i < buttons.size(); i++) {
                int button = buttons.get(i);
                releaseState.releaseButton(button);
                KeyState keyState = new KeyState();
                int modifiers = releaseState.getModifiers() | keyState.getModifiers();
                // send release event
                boolean isPopupTrigger = false; // TODO
                postMouseEvent(view, MouseEvent.UP, button,
                               relX, relY, x, y,
                               modifiers, isPopupTrigger,
                               synthesized);
            }
        }
        buttons.clear();
        // send scroll events
        if (newState.getWheel() != state.getWheel()) {
            double dY;
            switch (newState.getWheel()) {
                case MouseState.WHEEL_DOWN: dY = -1.0; break;
                case MouseState.WHEEL_UP: dY = 1.0; break;
                default: dY = 0.0; break;
            }
            if (dY != 0.0) {
                int modifiers = newState.getModifiers();
                application.getProcessor().runLater(() -> {
                    view.notifyScroll(relX, relY, x, y, 0.0, dY,
                                      modifiers, 1, 0, 0, 0, 1.0, 1.0);

                });
            }
            newState.setWheel(MouseState.WHEEL_NONE);
        }
        newState.copyTo(state);
    }

    private void postMouseEvent(HeadlessView view, int eventType, int button,
                                int relX, int relY, int x, int y,
                                int modifiers, boolean isPopupTrigger, boolean synthesized) {
        application.getProcessor().runLater(() -> {
            notifyMouse(view, eventType, button,
                        relX, relY, x, y,
                        modifiers, isPopupTrigger, synthesized);
        });
    }

    private void notifyMouse(HeadlessView view, int eventType, int button,
                             int relX, int relY, int x, int y,
                             int modifiers, boolean isPopupTrigger, boolean synthesized) {
        switch (eventType) {
            case MouseEvent.DOWN: {
                if (dragButton == MouseEvent.BUTTON_NONE) {
                    dragButton = button;
                }
                break;
            }
            case MouseEvent.UP: {
                if (dragButton == button) {
                    dragButton = MouseEvent.BUTTON_NONE;
                    if (dragInProgress) {
                        try {
                            view.notifyDragDrop(relX, relY, x, y,
                                                Clipboard.ACTION_MOVE);
                        } catch (RuntimeException e) {
                            Application.reportException(e);
                        }
                        try {
                            view.notifyDragEnd(Clipboard.ACTION_MOVE);
                        } catch (RuntimeException e) {
                            Application.reportException(e);
                        }
                        dragActions.clear();
                        dragView = null;
                        dragInProgress = false;
                    }
                }
                break;
            }
            case MouseEvent.DRAG: {
                if (dragButton != MouseEvent.BUTTON_NONE) {
                    if (dragInProgress) {
                        if (dragView == view && dragActions.isEmpty()) {
                            // first drag notification
                            try {
                                view.notifyDragEnter(relX, relY, x, y,
                                                     Clipboard.ACTION_MOVE);
                            } catch (RuntimeException e) {
                                Application.reportException(e);
                            }
                            dragActions.set(DRAG_ENTER);
                        } else if (dragView == view && dragActions.get(DRAG_ENTER)) {
                            try {
                                view.notifyDragOver(relX, relY, x, y,
                                                    Clipboard.ACTION_MOVE);
                            } catch (RuntimeException e) {
                                Application.reportException(e);
                            }
                            dragActions.set(DRAG_OVER);
                        } else if (dragView != view) {
                            if (dragView != null) {
                                try {
                                    dragView.notifyDragLeave();
                                } catch (RuntimeException e) {
                                    Application.reportException(e);
                                }
                            }
                            try {
                                view.notifyDragEnter(relX, relY, x, y,
                                                     Clipboard.ACTION_MOVE);
                            } catch (RuntimeException e) {
                                Application.reportException(e);
                            }
                            dragActions.clear();
                            dragActions.set(DRAG_ENTER);
                            dragView = view;
                        }
                        return; // consume event
                    } else {
                        if (dragView == null) {
                            dragView = view;
                        }
                    }
                }
                break;
            }
        }
        view.notifyMouse(eventType, button,
                         relX, relY, x, y,
                         modifiers, isPopupTrigger,
                         synthesized);
    }

    void notifyDragStart() {
        dragInProgress = true;
    }

}
