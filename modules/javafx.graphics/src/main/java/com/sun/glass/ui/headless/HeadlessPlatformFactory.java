package com.sun.glass.ui.headless;

import com.sun.glass.ui.Application;
import com.sun.glass.ui.Clipboard;
import com.sun.glass.ui.Menu;
import com.sun.glass.ui.MenuBar;
import com.sun.glass.ui.MenuItem;
import com.sun.glass.ui.PlatformFactory;
import com.sun.glass.ui.SystemClipboard;
import com.sun.glass.ui.delegate.ClipboardDelegate;
import com.sun.glass.ui.delegate.MenuBarDelegate;
import com.sun.glass.ui.delegate.MenuDelegate;
import com.sun.glass.ui.delegate.MenuItemDelegate;
import java.util.HashMap;

public class HeadlessPlatformFactory extends PlatformFactory {

    @Override
    public Application createApplication() {
        return new HeadlessApplication();
    }

    @Override
    public MenuBarDelegate createMenuBarDelegate(MenuBar menubar) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    public MenuDelegate createMenuDelegate(Menu menu) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    public MenuItemDelegate createMenuItemDelegate(MenuItem menuItem) {
        throw new UnsupportedOperationException("Not supported yet."); // Generated from nbfs://nbhost/SystemFileSystem/Templates/Classes/Code/GeneratedMethodBody
    }

    @Override
    public ClipboardDelegate createClipboardDelegate() {
        return (String clipboardName) -> {
            if (Clipboard.SYSTEM.equals(clipboardName)) {
                return new HeadlessSystemClipboard();
            } else {
                throw new IllegalArgumentException("No support for " + clipboardName + " clipboard in headless");
            }
        };
    }

    class HeadlessSystemClipboard extends SystemClipboard {

        HashMap<String, Object> cacheData;
        int supportedActions;

        HeadlessSystemClipboard() {
            super(Clipboard.SYSTEM);
        }

        @Override
        protected boolean isOwner() {
            return true;
        }

        @Override
        protected void pushToSystem(HashMap<String, Object> cacheData, int supportedActions) {
            this.cacheData = cacheData;
            this.supportedActions = supportedActions;
        }

        @Override
        protected void pushTargetActionToSystem(int actionDone) {
        }

        @Override
        protected Object popFromSystem(String mimeType) {
            return null;
        }

        @Override
        protected int supportedSourceActionsFromSystem() {
            return Clipboard.ACTION_NONE;
        }

        @Override
        protected String[] mimesFromSystem() {
            return new String[0];
        }
    }
}
