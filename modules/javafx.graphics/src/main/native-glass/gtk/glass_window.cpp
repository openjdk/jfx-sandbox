/*
 * Copyright (c) 2011, 2025, Oracle and/or its affiliates. All rights reserved.
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
#include "glass_window.h"
#include "glass_general.h"
#include "glass_key.h"
#include "glass_screen.h"
#include "glass_dnd.h"
#include "glass_evloop.h"

#include <com_sun_glass_events_WindowEvent.h>
#include <com_sun_glass_events_ViewEvent.h>
#include <com_sun_glass_events_MouseEvent.h>
#include <com_sun_glass_events_KeyEvent.h>

#include <com_sun_glass_ui_Window_Level.h>

#include <cairo.h>
#include <cairo-xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <gtk/gtkx.h>

#include <string.h>
#include <algorithm>

#define MOUSE_BACK_BTN 8
#define MOUSE_FORWARD_BTN 9

static gboolean event_draw_background(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    WindowContext *ctx = USER_PTR_TO_CTX(user_data);
    LOG0("event_draw_background\n");
    ctx->paint_background(cr);

    return FALSE;
}

static gboolean event_realize(GtkWidget *widget, gpointer user_data) {
    WindowContext *ctx = USER_PTR_TO_CTX(user_data);
    ctx->process_realize();

    return FALSE;
}

static gboolean enforce_requested_state_later(gpointer user_data) {
    WindowContext *ctx = USER_PTR_TO_CTX(user_data);
    ctx->enforce_requested_state();

    return G_SOURCE_REMOVE;
}

static gboolean update_window_size_location_later(gpointer user_data) {
    WindowContext *ctx = USER_PTR_TO_CTX(user_data);
    ctx->update_window_size_location();

    return G_SOURCE_REMOVE;
}

static int geometry_get_window_width(const WindowGeometry *windowGeometry) {
     return (windowGeometry->final_width.type == BOUNDSTYPE_WINDOW)
                   ? windowGeometry->final_width.value
                   : windowGeometry->final_width.value
                         + windowGeometry->extents.width;
}

static int geometry_get_window_height(const WindowGeometry *windowGeometry) {
    return (windowGeometry->final_height.type == BOUNDSTYPE_WINDOW)
                   ? windowGeometry->final_height.value
                   : windowGeometry->final_height.value
                         + windowGeometry->extents.height;
}

static int geometry_get_content_width(WindowGeometry *windowGeometry) {
    return (windowGeometry->final_width.type == BOUNDSTYPE_CONTENT)
                   ? windowGeometry->final_width.value
                   : windowGeometry->final_width.value
                         - windowGeometry->extents.width;
}

static int geometry_get_content_height(WindowGeometry *windowGeometry) {
    return (windowGeometry->final_height.type == BOUNDSTYPE_CONTENT)
                   ? windowGeometry->final_height.value
                   : windowGeometry->final_height.value
                         - windowGeometry->extents.height;
}

void destroy_and_delete_ctx(WindowContext* ctx) {
    LOG0("destroy_and_delete_ctx\n");
    if (ctx) {
        ctx->process_destroy();

        if (!ctx->get_events_count()) {
            LOG0("delete ctx\n");
            delete ctx;
        }
        // else: ctx will be deleted in EventsCounterHelper after completing
        // an event processing
    }
}

static gboolean is_window_floating(GdkWindow *gdk_window) {
    if (!GDK_IS_WINDOW(gdk_window))
        return FALSE;

    GdkWindowState state = gdk_window_get_state(gdk_window);

    return !(state & GDK_WINDOW_STATE_MAXIMIZED)
        && !(state & GDK_WINDOW_STATE_FULLSCREEN)
        && !(state & GDK_WINDOW_STATE_ICONIFIED);
}

static inline jint gtk_button_number_to_mouse_button(guint button) {
    switch (button) {
        case 1:
            return com_sun_glass_events_MouseEvent_BUTTON_LEFT;
        case 2:
            return com_sun_glass_events_MouseEvent_BUTTON_OTHER;
        case 3:
            return com_sun_glass_events_MouseEvent_BUTTON_RIGHT;
        case MOUSE_BACK_BTN:
            return com_sun_glass_events_MouseEvent_BUTTON_BACK;
        case MOUSE_FORWARD_BTN:
            return com_sun_glass_events_MouseEvent_BUTTON_FORWARD;
        default:
            // Other buttons are not supported by quantum and are not reported by other platforms
            return com_sun_glass_events_MouseEvent_BUTTON_NONE;
    }
}

WindowContext * WindowContext::sm_grab_window = NULL;
WindowContext * WindowContext::sm_mouse_drag_window = NULL;

// Work-around because frame extents are only obtained after window is shown.
// This is used to know the total window size (content + decoration)
// The first window will have a duplicated resize event, subsequent windows will use the cached value.
GdkRectangle WindowContext::normal_extents = {0, 0, 0, 0};
GdkRectangle WindowContext::utility_extents = {0, 0, 0, 0};

WindowContext::WindowContext(jobject _jwindow, WindowContext* _owner, long _screen,
        WindowFrameType _frame_type, WindowType type, GdkWMFunction wmf) :
            screen(_screen),
            frame_type(_frame_type),
            window_type(type),
            owner(_owner),
            geometry(),
            resizable(),
            im_ctx(),
            background_color() {
    jwindow = mainEnv->NewGlobalRef(_jwindow);
    initial_wmf = wmf;
    current_wmf = wmf;
    // Default to white
    background_color = { 1.0, 1.0, 1.0, 1.0 };
    is_mouse_entered = false;
    is_disabled = false;
    on_top = false;
    can_be_deleted = false;
    was_mapped = false;
    initial_state_mask = 0;

    gtk_widget = gtk_window_new(type == POPUP ? GTK_WINDOW_POPUP : GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(gtk_widget), "realize", G_CALLBACK(event_realize), this);
    g_signal_connect(G_OBJECT(gtk_widget), "draw", G_CALLBACK(event_draw_background), this);

    if (gchar* app_name = get_application_name()) {
        gtk_window_set_wmclass(GTK_WINDOW(gtk_widget), app_name, app_name);
        g_free(app_name);
    }

    if (owner) {
        owner->add_child(this);
        if (on_top_inherited()) {
            gtk_window_set_keep_above(GTK_WINDOW(gtk_widget), TRUE);
        }
    }

    if (type == UTILITY) {
        gtk_window_set_type_hint(GTK_WINDOW(gtk_widget), GDK_WINDOW_TYPE_HINT_UTILITY);
    }

    glong xvisualID = (glong)mainEnv->GetStaticLongField(jApplicationCls, jApplicationVisualID);

    if (xvisualID != 0) {
        GdkVisual *visual = gdk_x11_screen_lookup_visual(gdk_screen_get_default(), xvisualID);
        glass_gtk_window_configure_from_visual(gtk_widget, visual);
    }

    gtk_widget_set_app_paintable(gtk_widget, TRUE);

    glass_configure_window_transparency(gtk_widget, frame_type == TRANSPARENT);
    gtk_window_set_title(GTK_WINDOW(gtk_widget), "");

    if (frame_type != TITLED) {
        gtk_window_set_decorated(GTK_WINDOW(gtk_widget), FALSE);
    } else {
        geometry.extents = get_cached_extents();
    }
}

GdkWindow* WindowContext::get_gdk_window() {
    if (GDK_IS_WINDOW(gdk_window)) {
        return gdk_window;
    }

    return NULL;
}

jobject WindowContext::get_jview() {
    return jview;
}

jobject WindowContext::get_jwindow() {
    return jwindow;
}

bool WindowContext::isEnabled() {
    if (jwindow) {
        bool result = (JNI_TRUE == mainEnv->CallBooleanMethod(jwindow, jWindowIsEnabled));
        LOG_EXCEPTION(mainEnv)
        return result;
    } else {
        return false;
    }
}

void WindowContext::process_map() {
    // We need only first map
    if (was_mapped || window_type == POPUP) return;

    was_mapped = true;
    LOG1("%lu: mapped\n", XID());

    // Work around JDK-8337400 (Initial window position is not centered on Xorg)
    if (geometry.x_set || geometry.y_set) {
        int x = (geometry.x_set) ? geometry.x_set_value : geometry.x;
        int y = (geometry.y_set) ? geometry.y_set_value : geometry.y;

        LOG2("move (initial position work-around) -> %d,%d\n", x, y);
        gtk_window_move(GTK_WINDOW(gtk_widget), x, y);
    }

    // Must be later on Xorg for the initial state before show to work
    if (initial_state_mask != 0) {
        gdk_threads_add_idle((GSourceFunc) enforce_requested_state_later, this);
    }
}

void WindowContext::process_focus(GdkEventFocus *event) {
    if (!event->in && WindowContext::sm_grab_window == this) {
        ungrab_focus();
    }

    if (im_ctx.enabled && im_ctx.ctx) {
        if (event->in) {
            gtk_im_context_focus_in(im_ctx.ctx);
        } else {
            gtk_im_context_focus_out(im_ctx.ctx);
        }
    }

    if (jwindow) {
        if (!event->in || isEnabled()) {
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocus,
                    event->in ? com_sun_glass_events_WindowEvent_FOCUS_GAINED
                              : com_sun_glass_events_WindowEvent_FOCUS_LOST);
            CHECK_JNI_EXCEPTION(mainEnv)
        } else {
            // when the user tries to activate a disabled window, send FOCUS_DISABLED
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocusDisabled);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

void WindowContext::increment_events_counter() {
    ++events_processing_cnt;
}

void WindowContext::decrement_events_counter() {
    --events_processing_cnt;
}

size_t WindowContext::get_events_count() {
    return events_processing_cnt;
}

bool WindowContext::is_dead() {
    return can_be_deleted;
}

void WindowContext::process_destroy() {
    LOG1("%lu: process_destroy\n", XID());

    if (owner) {
        owner->remove_child(this);
    }

    if (WindowContext::sm_mouse_drag_window == this) {
        ungrab_mouse_drag_focus();
    }

    if (WindowContext::sm_grab_window == this) {
        ungrab_focus();
    }

    std::set<WindowContext*>::iterator it;
    for (it = children.begin(); it != children.end(); ++it) {
        // FIX JDK-8226537: this method calls set_owner(NULL) which prevents
        // WindowContext::process_destroy() to call remove_child() (because children
        // is being iterated here) but also prevents gtk_window_set_transient_for from
        // being called - this causes the crash on gnome.
        gtk_window_set_transient_for((*it)->get_gtk_window(), NULL);
        (*it)->set_owner(NULL);
        destroy_and_delete_ctx(*it);
    }
    children.clear();

    if (jwindow) {
        mainEnv->CallVoidMethod(jwindow, jWindowNotifyDestroy);
        EXCEPTION_OCCURED(mainEnv);
    }

    if (jview) {
        mainEnv->DeleteGlobalRef(jview);
        jview = NULL;
    }

    if (jwindow) {
        mainEnv->DeleteGlobalRef(jwindow);
        jwindow = NULL;
    }

    can_be_deleted = true;
}

void WindowContext::process_delete() {
    LOG1("%lu: process_delete\n", XID());
    if (jwindow && isEnabled()) {
        LOG1("%lu: jWindowNotifyClose\n", XID());
        mainEnv->CallVoidMethod(jwindow, jWindowNotifyClose);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

// Returns false to not interrupt the EXPOSE event to Gtk, so we can paint the background (as there's no view)
bool WindowContext::notify_repaint(GdkRectangle *rect) {
    if (jview) {
        mainEnv->CallVoidMethod(jview, jViewNotifyRepaint, rect->x, rect->y, rect->width, rect->height);
        CHECK_JNI_EXCEPTION_RET(mainEnv, true)

        return true;
    }

    return false;
}

void WindowContext::process_mouse_button(GdkEventButton *event) {
    bool press = event->type == GDK_BUTTON_PRESS;
    guint state = event->state;
    guint mask = 0;

    // We need to add/remove current mouse button from the modifier flags
    // as X lib state represents the state just prior to the event and
    // glass needs the state just after the event
    switch (event->button) {
        case 1:
            mask = GDK_BUTTON1_MASK;
            break;
        case 2:
            mask = GDK_BUTTON2_MASK;
            break;
        case 3:
            mask = GDK_BUTTON3_MASK;
            break;
        case MOUSE_BACK_BTN:
            mask = GDK_BUTTON4_MASK;
            break;
        case MOUSE_FORWARD_BTN:
            mask = GDK_BUTTON5_MASK;
            break;
    }

    if (press) {
        state |= mask;
    } else {
        state &= ~mask;
    }

    if (press) {
        GdkDevice* device = event->device;

        if (glass_gdk_device_is_grabbed(device)
                && (glass_gdk_device_get_window_at_position(device, NULL, NULL)
                == NULL)) {
            ungrab_focus();
            return;
        }
    }

    if (!press) {
        if ((event->state & MOUSE_BUTTONS_MASK) && !(state & MOUSE_BUTTONS_MASK)) { // all buttons released
            ungrab_mouse_drag_focus();
        } else if (event->button == 8 || event->button == 9) {
            // GDK X backend interprets button press events for buttons 4-7 as
            // scroll events so GDK_BUTTON4_MASK and GDK_BUTTON5_MASK will never
            // be set on the event->state from GDK. Thus we cannot check if all
            // buttons have been released in the usual way (as above).
            ungrab_mouse_drag_focus();
        }
    }

    jint button = gtk_button_number_to_mouse_button(event->button);

    if (jview && button != com_sun_glass_events_MouseEvent_BUTTON_NONE) {
        mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                press ? com_sun_glass_events_MouseEvent_DOWN : com_sun_glass_events_MouseEvent_UP,
                button,
                (jint) event->x, (jint) event->y,
                (jint) event->x_root, (jint) event->y_root,
                gdk_modifier_mask_to_glass(state),
                (event->button == 3 && press) ? JNI_TRUE : JNI_FALSE,
                JNI_FALSE);
        CHECK_JNI_EXCEPTION(mainEnv)

        if (jview && event->button == 3 && press) {
            mainEnv->CallVoidMethod(jview, jViewNotifyMenu,
                    (jint)event->x, (jint)event->y,
                    (jint)event->x_root, (jint)event->y_root,
                    JNI_FALSE);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

void WindowContext::process_mouse_motion(GdkEventMotion *event) {
    jint glass_modifier = gdk_modifier_mask_to_glass(event->state);
    jint isDrag = glass_modifier & (
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_PRIMARY |
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_MIDDLE |
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_SECONDARY |
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_BACK |
            com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_FORWARD);
    jint button = com_sun_glass_events_MouseEvent_BUTTON_NONE;

    if (isDrag && WindowContext::sm_mouse_drag_window == NULL) {
        // Upper layers expects from us Windows behavior:
        // all mouse events should be delivered to window where drag begins
        // and no exit/enter event should be reported during this drag.
        // We can grab mouse pointer for these needs.
        grab_mouse_drag_focus();
    }

    if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_PRIMARY) {
        button = com_sun_glass_events_MouseEvent_BUTTON_LEFT;
    } else if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_MIDDLE) {
        button = com_sun_glass_events_MouseEvent_BUTTON_OTHER;
    } else if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_SECONDARY) {
        button = com_sun_glass_events_MouseEvent_BUTTON_RIGHT;
    } else if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_BACK) {
        button = com_sun_glass_events_MouseEvent_BUTTON_BACK;
    } else if (glass_modifier & com_sun_glass_events_KeyEvent_MODIFIER_BUTTON_FORWARD) {
        button = com_sun_glass_events_MouseEvent_BUTTON_FORWARD;
    }

    if (jview) {
        mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                isDrag ? com_sun_glass_events_MouseEvent_DRAG : com_sun_glass_events_MouseEvent_MOVE,
                button,
                (jint) event->x, (jint) event->y,
                (jint) event->x_root, (jint) event->y_root,
                glass_modifier,
                JNI_FALSE,
                JNI_FALSE);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContext::process_mouse_scroll(GdkEventScroll *event) {
    jdouble dx = 0;
    jdouble dy = 0;

    // converting direction to change in pixels
    switch (event->direction) {
#if GTK_CHECK_VERSION(3, 4, 0)
        case GDK_SCROLL_SMOOTH:
            //FIXME 3.4 ???
            break;
#endif
        case GDK_SCROLL_UP:
            dy = 1;
            break;
        case GDK_SCROLL_DOWN:
            dy = -1;
            break;
        case GDK_SCROLL_LEFT:
            dx = 1;
            break;
        case GDK_SCROLL_RIGHT:
            dx = -1;
            break;
    }
    if (event->state & GDK_SHIFT_MASK) {
        jdouble t = dy;
        dy = dx;
        dx = t;
    }
    if (jview) {
        mainEnv->CallVoidMethod(jview, jViewNotifyScroll,
                (jint) event->x, (jint) event->y,
                (jint) event->x_root, (jint) event->y_root,
                dx, dy,
                gdk_modifier_mask_to_glass(event->state),
                (jint) 0, (jint) 0,
                (jint) 0, (jint) 0,
                (jdouble) 40.0, (jdouble) 40.0);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContext::process_mouse_cross(GdkEventCrossing *event) {
    bool enter = event->type == GDK_ENTER_NOTIFY;
    if (jview) {
        guint state = event->state;
        if (enter) { // workaround for JDK-8126843
            state &= ~MOUSE_BUTTONS_MASK;
        }

        if (enter != is_mouse_entered) {
            is_mouse_entered = enter;
            mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                    enter ? com_sun_glass_events_MouseEvent_ENTER : com_sun_glass_events_MouseEvent_EXIT,
                    com_sun_glass_events_MouseEvent_BUTTON_NONE,
                    (jint) event->x, (jint) event->y,
                    (jint) event->x_root, (jint) event->y_root,
                    gdk_modifier_mask_to_glass(state),
                    JNI_FALSE,
                    JNI_FALSE);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

void WindowContext::process_key(GdkEventKey *event) {
    bool press = event->type == GDK_KEY_PRESS;
    jint glassKey = get_glass_key(event);
    jint glassModifier = gdk_modifier_mask_to_glass(event->state);
    if (press) {
        glassModifier |= glass_key_to_modifier(glassKey);
    } else {
        glassModifier &= ~glass_key_to_modifier(glassKey);
    }
    jcharArray jChars = NULL;
    jchar key = gdk_keyval_to_unicode(event->keyval);
    if (key >= 'a' && key <= 'z' && (event->state & GDK_CONTROL_MASK)) {
        key = key - 'a' + 1; // map 'a' to ctrl-a, and so on.
    }

    if (key > 0) {
        jChars = mainEnv->NewCharArray(1);
        if (jChars) {
            mainEnv->SetCharArrayRegion(jChars, 0, 1, &key);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    } else {
        jChars = mainEnv->NewCharArray(0);
    }

    if (!jview) {
        return;
    }

    mainEnv->CallVoidMethod(jview, jViewNotifyKey,
            (press) ? com_sun_glass_events_KeyEvent_PRESS
                    : com_sun_glass_events_KeyEvent_RELEASE,
            glassKey,
            jChars,
            glassModifier);
    CHECK_JNI_EXCEPTION(mainEnv)

    // TYPED events should only be sent for printable characters.
    // jview is checked again because previous call might be an exit key
    if (press && key > 0 && jview) {
        mainEnv->CallVoidMethod(jview, jViewNotifyKey,
                com_sun_glass_events_KeyEvent_TYPED,
                com_sun_glass_events_KeyEvent_VK_UNDEFINED,
                jChars,
                glassModifier);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContext::paint(void* data, jint width, jint height) {
    cairo_rectangle_int_t rect = {0, 0, width, height};
    cairo_region_t *region = cairo_region_create_rectangle(&rect);
    gdk_window_begin_paint_region(gdk_window, region);

    cairo_t* context = gdk_cairo_create(gdk_window);

    cairo_surface_t* cairo_surface =
        cairo_image_surface_create_for_data(
            (unsigned char*)data,
            CAIRO_FORMAT_ARGB32,
            width, height, width * 4);

    applyShapeMask(data, width, height);

    cairo_set_source_surface(context, cairo_surface, 0, 0);
    cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
    cairo_paint(context);

    gdk_window_end_paint(gdk_window);
    cairo_region_destroy(region);

    cairo_destroy(context);
    cairo_surface_destroy(cairo_surface);
}

void WindowContext::paint_background(cairo_t *cr) {
    if (frame_type == TRANSPARENT) return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(gtk_widget, &allocation);
    gdk_cairo_set_source_rgba(cr, &background_color);
    cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
    cairo_fill(cr);
}

void WindowContext::add_child(WindowContext* child) {
    children.insert(child);
    gtk_window_set_transient_for(child->get_gtk_window(), this->get_gtk_window());
}

void WindowContext::remove_child(WindowContext* child) {
    children.erase(child);
    gtk_window_set_transient_for(child->get_gtk_window(), NULL);
}

bool WindowContext::is_visible() {
    return gtk_widget_get_visible(gtk_widget);
}

bool WindowContext::set_view(jobject view) {
    if (jview) {
        mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                com_sun_glass_events_MouseEvent_EXIT,
                com_sun_glass_events_MouseEvent_BUTTON_NONE,
                0, 0,
                0, 0,
                0,
                JNI_FALSE,
                JNI_FALSE);
        mainEnv->DeleteGlobalRef(jview);
    }

    if (view) {
        jview = mainEnv->NewGlobalRef(view);
    } else {
        jview = NULL;
    }
    return TRUE;
}

bool WindowContext::grab_mouse_drag_focus() {
    if (glass_gdk_mouse_devices_grab_with_cursor(
            gdk_window, gdk_window_get_cursor(gdk_window), FALSE)) {
        WindowContext::sm_mouse_drag_window = this;
        return true;
    } else {
        return false;
    }
}

void WindowContext::ungrab_mouse_drag_focus() {
    WindowContext::sm_mouse_drag_window = NULL;
    glass_gdk_mouse_devices_ungrab();
    if (WindowContext::sm_grab_window) {
        WindowContext::sm_grab_window->grab_focus();
    }
}

bool WindowContext::grab_focus() {
    if (WindowContext::sm_mouse_drag_window
            || glass_gdk_mouse_devices_grab(gdk_window)) {
        WindowContext::sm_grab_window = this;
        return true;
    } else {
        return false;
    }
}

void WindowContext::ungrab_focus() {
    if (!WindowContext::sm_mouse_drag_window) {
        glass_gdk_mouse_devices_ungrab();
    }
    WindowContext::sm_grab_window = NULL;

    if (jwindow) {
        mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocusUngrab);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContext::set_cursor(GdkCursor* cursor) {
    if (!is_in_drag()) {
        if (WindowContext::sm_mouse_drag_window) {
            glass_gdk_mouse_devices_grab_with_cursor(
                    WindowContext::sm_mouse_drag_window->get_gdk_window(), cursor, FALSE);
        } else if (WindowContext::sm_grab_window) {
            glass_gdk_mouse_devices_grab_with_cursor(
                    WindowContext::sm_grab_window->get_gdk_window(), cursor, TRUE);
        }
    }
    gdk_window_set_cursor(gdk_window, cursor);
}

void WindowContext::set_background(float r, float g, float b) {
    LOG3("set_background %.2f, %.2f, %.2f\n", r, g, b);
    background_color.red = r;
    background_color.green = g;
    background_color.blue = b;
}

GdkAtom WindowContext::get_net_frame_extents_atom() {
    static GdkAtom atom = NULL;
    if (atom == NULL) {
        atom = gdk_atom_intern_static_string("_NET_FRAME_EXTENTS");
    }
    return atom;
}

void WindowContext::request_frame_extents() {
    Display *display = GDK_DISPLAY_XDISPLAY(gdk_window_get_display(gdk_window));
    static Atom rfeAtom = XInternAtom(display, "_NET_REQUEST_FRAME_EXTENTS", False);

    if (rfeAtom != None) {
        XClientMessageEvent clientMessage;
        memset(&clientMessage, 0, sizeof(clientMessage));

        clientMessage.type = ClientMessage;
        clientMessage.window = GDK_WINDOW_XID(gdk_window);
        clientMessage.message_type = rfeAtom;
        clientMessage.format = 32;

        XSendEvent(display, XDefaultRootWindow(display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   (XEvent *) &clientMessage);
        XFlush(display);
    }
}

void WindowContext::update_window_size_location() {
    if (!geometry.needs_to_restore_size
        || (gdk_window_get_state(gdk_window) & (GDK_WINDOW_STATE_FULLSCREEN | GDK_WINDOW_STATE_MAXIMIZED))) {
        return;
    }

    geometry.needs_to_restore_size = false;
    int cw = geometry_get_content_width(&geometry);
    int ch = geometry_get_content_height(&geometry);

    gtk_window_move(GTK_WINDOW(gtk_widget), geometry.x, geometry.y);
    update_window_constraints();

    LOG2("update size/location -> size %d, %d\n", cw, ch);
    gtk_window_resize(GTK_WINDOW(gtk_widget), cw, ch);
}

void WindowContext::enforce_requested_state() {
    if (initial_state_mask & GDK_WINDOW_STATE_MAXIMIZED) {
        gtk_window_maximize(GTK_WINDOW(gtk_widget));
    }

    if (initial_state_mask & GDK_WINDOW_STATE_FULLSCREEN) {
        gtk_window_fullscreen(GTK_WINDOW(gtk_widget));
    }

    if (initial_state_mask & GDK_WINDOW_STATE_ICONIFIED) {
        gtk_window_iconify(GTK_WINDOW(gtk_widget));
    }
}

void WindowContext::update_frame_extents() {
    int top, left, bottom, right;

    if (get_frame_extents_property(&top, &left, &bottom, &right)) {
        if (top > 0 || right > 0 || bottom > 0 || left > 0) {
            bool changed = geometry.extents.x != left
                        || geometry.extents.y != top
                        || geometry.extents.width != (left + right)
                        || geometry.extents.height != (top + bottom);

            if (!changed) {
                return;
            }

            GdkRectangle rect = { left, top, (left + right), (top + bottom) };

            int newW, newH;
            gdk_window_get_geometry(gdk_window, NULL, NULL, &newW, &newH);

            // Here the user might change the desktop theme and in consequence
            // change decoration sizes. Note that width / height might have
            // different bound types
            if (geometry.final_width.type == BOUNDSTYPE_WINDOW) {
                // Re-add the extents and then subtract the new
                newW = newW
                    + ((geometry.frame_extents_received) ? geometry.extents.width : 0)
                    - rect.width;
            }

            if (geometry.final_height.type == BOUNDSTYPE_WINDOW) {
                // Re-add the extents and then subtract the new
                newH = newH
                    + ((geometry.frame_extents_received) ? geometry.extents.height : 0)
                    - rect.height;
            }

            geometry.extents = rect;
            geometry.frame_extents_received = true;

            LOG2("extents received -> new view size: %d, %d\n", newW, newH);
            gtk_window_resize(GTK_WINDOW(gtk_widget), newW, newH);

            int x = geometry.x;
            int y = geometry.y;

            // Gravity x, y are used in centerOnScreen(). Here it's used to adjust the position
            // accounting decorations
            if (geometry.gravity_x != 0) {
                x -= geometry.gravity_x * (float) (geometry.extents.width);

                if (geometry.x_set) {
                    geometry.x_set_value -= geometry.gravity_x * (float) (geometry.extents.width);
                }
            }

            if (geometry.gravity_y != 0) {
                y -= geometry.gravity_y  * (float) (geometry.extents.height);

                if (geometry.y_set) {
                    geometry.y_set_value -= geometry.gravity_y  * (float) (geometry.extents.height);
                }
            }

            if ((gdk_window_get_state(gdk_window) & GDK_WINDOW_STATE_FULLSCREEN) == 0) {
                gtk_window_move(GTK_WINDOW(gtk_widget), x, y);
            } else {
                geometry.x = x;
                geometry.y = y;
            }
        }
    }
}

bool WindowContext::get_frame_extents_property(int *top, int *left,
        int *bottom, int *right) {
    unsigned long *extents;

    if (gdk_property_get(gdk_window,
            get_net_frame_extents_atom(),
            gdk_atom_intern("CARDINAL", FALSE),
            0,
            sizeof (unsigned long) * 4,
            FALSE,
            NULL,
            NULL,
            NULL,
            (guchar**) & extents)) {
        *left = extents [0];
        *right = extents [1];
        *top = extents [2];
        *bottom = extents [3];

        g_free(extents);
        return true;
    }

    return false;
}

GdkRectangle WindowContext::get_cached_extents() {
    return window_type == UTILITY ? utility_extents : normal_extents;
}

void WindowContext::process_property_notify(GdkEventProperty *event) {
//    LOG1("process_property_notify: %s\n", gdk_atom_name(event->atom));
    if (event->atom == get_net_frame_extents_atom()) {
        update_frame_extents();
    }
}

void WindowContext::process_state(GdkEventWindowState *event) {
    if (!(event->changed_mask & (GDK_WINDOW_STATE_ICONIFIED
                                | GDK_WINDOW_STATE_MAXIMIZED
                                | GDK_WINDOW_STATE_FULLSCREEN
                                | GDK_WINDOW_STATE_ABOVE))) {
        return;
    }

    if (event->changed_mask & GDK_WINDOW_STATE_ABOVE) {
        notify_on_top(event->new_window_state & GDK_WINDOW_STATE_ABOVE);
    }

    // Those represent the real current size in the state
    int cw = gdk_window_get_width(gdk_window);
    int ch = gdk_window_get_height(gdk_window);

    int ww = cw + geometry.extents.width;
    int wh = ch + geometry.extents.height;

    if ((event->changed_mask & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_ICONIFIED))
        && ((event->new_window_state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_ICONIFIED)) == 0)) {
        LOG0("com_sun_glass_events_WindowEvent_RESTORE\n");
        notify_window_resize(com_sun_glass_events_WindowEvent_RESTORE, ww, wh);
    } else if (event->new_window_state & (GDK_WINDOW_STATE_ICONIFIED)) {
        LOG0("com_sun_glass_events_WindowEvent_MINIMIZE\n");
        notify_window_resize(com_sun_glass_events_WindowEvent_MINIMIZE, ww, wh);
    } else if (event->new_window_state & (GDK_WINDOW_STATE_MAXIMIZED)) {
        LOG0("com_sun_glass_events_WindowEvent_MAXIMIZE\n");
        notify_window_resize(com_sun_glass_events_WindowEvent_MAXIMIZE, ww, wh);
    }

    if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED
        && (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) == 0) {
        remove_wmf(GDK_FUNC_MINIMIZE);
    }

    if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED
        && (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) == 0) {
        remove_wmf(GDK_FUNC_MINIMIZE);
    }

    notify_view_resize(cw, ch);
    // Since FullScreen (or custom modes of maximized) can undecorate the
    // window, request view position change
    notify_view_move();

    if (jview && event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
        if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
            LOG0("com_sun_glass_events_ViewEvent_FULLSCREEN_ENTER\n");
            mainEnv->CallVoidMethod(jview, jViewNotifyView, com_sun_glass_events_ViewEvent_FULLSCREEN_ENTER);
            CHECK_JNI_EXCEPTION(mainEnv)
        } else {
            LOG0("com_sun_glass_events_ViewEvent_FULLSCREEN_EXIT\n");
            mainEnv->CallVoidMethod(jview, jViewNotifyView, com_sun_glass_events_ViewEvent_FULLSCREEN_EXIT);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }

    gdk_threads_add_idle((GSourceFunc) enforce_requested_state_later, this);

    // This only accounts MAXIMIZED and FULLSCREEN
    bool restored = (event->changed_mask & (GDK_WINDOW_STATE_MAXIMIZED
                                            | GDK_WINDOW_STATE_FULLSCREEN))
                    && ((event->new_window_state & (GDK_WINDOW_STATE_MAXIMIZED
                                            | GDK_WINDOW_STATE_FULLSCREEN)) == 0);

    // In case the size or location changed while maximized of fullscreened
    if (restored && geometry.needs_to_restore_size) {
        //Call if later because restore properties will still arrive
        g_print("update_window_size_location_later\n");
        gdk_threads_add_idle((GSourceFunc) update_window_size_location_later, this);
    }
}

void WindowContext::process_realize() {
    LOG0("realized\n");
    gdk_window = gtk_widget_get_window(gtk_widget);

    if (frame_type == TITLED) {
        request_frame_extents();
    }

    gdk_window_set_events(gdk_window, GDK_FILTERED_EVENTS_MASK);
    g_object_set_data_full(G_OBJECT(gdk_window), GDK_WINDOW_DATA_CONTEXT, this, NULL);
    gdk_window_register_dnd(gdk_window);

    if (frame_type != TITLED) {
        initial_wmf = GDK_FUNC_ALL;
    }

    if (initial_wmf) {
        gdk_window_set_functions(gdk_window, initial_wmf);
    }
}

void WindowContext::notify_window_resize(int state, int width, int height) {
    if (jwindow) {
        LOG3("jWindowNotifyResize: %d -> %d, %d\n", state, width, height);
        mainEnv->CallVoidMethod(jwindow, jWindowNotifyResize, state, width, height);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContext::notify_window_move(int x, int y) {
    if (jwindow) {
        mainEnv->CallVoidMethod(jwindow, jWindowNotifyMove, x, y);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContext::notify_view_resize(int width, int height) {
    if (jview) {
        LOG2("jViewNotifyResize: %d, %d\n", width, height);
        mainEnv->CallVoidMethod(jview, jViewNotifyResize, width, height);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContext::notify_view_move() {
    if (jview) {
        LOG0("com_sun_glass_events_ViewEvent_MOVE\n");
        mainEnv->CallVoidMethod(jview, jViewNotifyView,
                com_sun_glass_events_ViewEvent_MOVE);
        CHECK_JNI_EXCEPTION(mainEnv)
    }
}

void WindowContext::process_configure(GdkEventConfigure *event) {
    LOG5("Configure Event - send_event: %d, x: %d, y: %d, width: %d, height: %d\n",
            event->send_event, event->x, event->y, event->width, event->height);

    GdkWindowState state = gdk_window_get_state(gdk_window);

    if (state & GDK_WINDOW_STATE_ICONIFIED) {
        return;
    }

    gint root_x, root_y, origin_x, origin_y;
    gdk_window_get_root_origin(gdk_window, &root_x, &root_y);
    gdk_window_get_origin(gdk_window, &origin_x, &origin_y);

    // view_x and view_y represent the position of the content relative to the top-left corner of the window,
    // taking into account window decorations (such as title bars and borders) applied by the window manager
    // and might vary by window state.
    geometry.view_x = origin_x - root_x;
    geometry.view_y = origin_y - root_y;
    LOG2("view x, y: %d, %d\n", geometry.view_x, geometry.view_y);

    notify_view_resize(event->width, event->height);
    notify_view_move();

    // The returned values might be inaccurate if _NET_FRAME_EXTENTS has not been received yet.
    // They will be corrected later if the property is updated. However, since there is no guarantee
    // that _NET_FRAME_EXTENTS will ever be available, we set the best guess for now.
    int ww = event->width + geometry.extents.width;
    int wh = event->height + geometry.extents.height;

    notify_window_resize((state & GDK_WINDOW_STATE_MAXIMIZED)
                            ? com_sun_glass_events_WindowEvent_MAXIMIZE
                            : com_sun_glass_events_WindowEvent_RESIZE,
                            ww, wh);

    notify_window_move(root_x, root_y);

    geometry.final_width.value = (geometry.final_width.type == BOUNDSTYPE_CONTENT)
            ? event->width : ww;

    geometry.final_height.value = (geometry.final_height.type == BOUNDSTYPE_CONTENT)
            ? event->height : wh;

    // x and y represent the position of the top-left corner of the window relative to the desktop area
    geometry.x = root_x;
    geometry.y = root_y;

    glong to_screen = getScreenPtrForLocation(event->x, event->y);
    if (to_screen != -1 && to_screen != screen) {
        if (jwindow) {
            LOG0("jWindowNotifyMoveToAnotherScreen\n");
            //notify screen changed
            jobject jScreen = createJavaScreen(mainEnv, to_screen);
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyMoveToAnotherScreen, jScreen);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
        screen = to_screen;
    }
}

void WindowContext::update_window_constraints() {
    // Prevent setFullScreen / setMaximized from updating geometry
    if (!is_window_floating(gdk_window)) {
          return;
    }

    GdkGeometry hints;

    if (resizable.value && !is_disabled) {
        int min_w = (resizable.minw == -1) ? 1
                      : resizable.minw - geometry.extents.width;
        int min_h =  (resizable.minh == -1) ? 1
                      : resizable.minh - geometry.extents.height;

        hints.min_width = (min_w < 1) ? 1 : min_w;
        hints.min_height = (min_h < 1) ? 1 : min_h;

        hints.max_width = (resizable.maxw == -1) ? G_MAXINT
                            : resizable.maxw - geometry.extents.width;

        hints.max_height = (resizable.maxh == -1) ? G_MAXINT
                           : resizable.maxh - geometry.extents.height;
    } else {
        int w = geometry_get_content_width(&geometry);
        int h = geometry_get_content_height(&geometry);

        hints.min_width = w;
        hints.min_height = h;
        hints.max_width = w;
        hints.max_height = h;
    }

    gtk_window_set_geometry_hints(GTK_WINDOW(gtk_widget), NULL, &hints,
                                  (GdkWindowHints)(GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE));
}

void WindowContext::set_resizable(bool res) {
    resizable.value = res;
    update_window_constraints();
}

void WindowContext::set_visible(bool visible) {
    if (visible) {
        gtk_widget_show(gtk_widget);

        if (!geometry.size_assigned) {
            set_bounds(0, 0, false, false, 320, 200, -1, -1, 0, 0);
        }

        // JDK-8220272 - fire event first because GDK_FOCUS_CHANGE is not always in order
        if (jwindow && isEnabled()) {
            mainEnv->CallVoidMethod(jwindow, jWindowNotifyFocus, com_sun_glass_events_WindowEvent_FOCUS_GAINED);
            CHECK_JNI_EXCEPTION(mainEnv);
        }
    } else {
        gtk_widget_hide(gtk_widget);
        if (jview && is_mouse_entered) {
            is_mouse_entered = false;
            mainEnv->CallVoidMethod(jview, jViewNotifyMouse,
                    com_sun_glass_events_MouseEvent_EXIT,
                    com_sun_glass_events_MouseEvent_BUTTON_NONE,
                    0, 0,
                    0, 0,
                    0,
                    JNI_FALSE,
                    JNI_FALSE);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

void WindowContext::set_bounds(int x, int y, bool xSet, bool ySet, int w, int h, int cw, int ch,
                               float gravity_x, float gravity_y) {
    LOG10("set_bounds -> x = %d, y = %d, xset = %d, yset = %d, w = %d, h = %d, cw = %d, ch = %d, gx = %f, gy = %f\n",
            x, y, xSet, ySet, w, h, cw, ch, gravity_x, gravity_y);
    // newW / newH are view/content sizes
    int newW = 0;
    int newH = 0;

    geometry.gravity_x = gravity_x;
    geometry.gravity_y = gravity_y;

    if (w > 0) {
        geometry.final_width.type = BOUNDSTYPE_WINDOW;
        geometry.final_width.value = w;
        newW = w - geometry.extents.width;
    } else if (cw > 0) {
        geometry.final_width.type = BOUNDSTYPE_CONTENT;
        geometry.final_width.value = cw;
        newW = cw;
    } else {
        newW = geometry_get_content_width(&geometry);
    }

    if (h > 0) {
        geometry.final_height.type = BOUNDSTYPE_WINDOW;
        geometry.final_height.value = h;
        newH = h - geometry.extents.height;
    } else if (ch > 0) {
        geometry.final_height.type = BOUNDSTYPE_CONTENT;
        geometry.final_height.value = ch;
        newH = ch;
    } else {
        newH = geometry_get_content_height(&geometry);
    }

    if (xSet) {
        geometry.x = x;
        geometry.x_set = true;
        geometry.x_set_value = x;
    }

    if (ySet) {
        geometry.y = y;
        geometry.y_set = true;
        geometry.y_set_value = y;
    }

    if (GDK_IS_WINDOW(gdk_window)) {
        GdkWindowState state = gdk_window_get_state(gdk_window);

        // If it was requested to be or currently is fullscreen/maximized, just save the requested
        // dimensions / location and set them later when restored
        if (state & (GDK_WINDOW_STATE_FULLSCREEN | GDK_WINDOW_STATE_MAXIMIZED)) {
            LOG0("set_bounds -> needs_to_restore_size = true\n");
            geometry.needs_to_restore_size = true;
            return;
        }
    }

    if (newW > 0 || newH > 0) {
        // call update_window_constraints() to let gtk_window_resize succeed, because it's bound to geometry constraints
        update_window_constraints();

        if (GDK_IS_WINDOW(gdk_window)) {
            gtk_window_resize(GTK_WINDOW(gtk_widget), newW, newH);
        } else {
            gtk_window_set_default_size(GTK_WINDOW(gtk_widget), newW, newH);

            // If the GdkWindow is not yet created, report back to Java, because the configure event
            // won't happen
            int w = geometry_get_window_width(&geometry);
            int h = geometry_get_window_height(&geometry);

            notify_window_resize(com_sun_glass_events_WindowEvent_RESIZE, w, h);
            notify_view_resize(newW, newH);
        }

        geometry.size_assigned = true;
    }

    if (xSet || ySet) {
        gtk_window_move(GTK_WINDOW(gtk_widget), geometry.x, geometry.y);
    }
}

void WindowContext::applyShapeMask(void* data, uint width, uint height) {
    if (frame_type != TRANSPARENT) {
        return;
    }

    glass_window_apply_shape_mask(gtk_widget_get_window(gtk_widget), data, width, height);
}

void WindowContext::set_minimized(bool minimize) {
    LOG1("set_minimized = %d\n", minimize);
    if (minimize) {
        initial_state_mask |= GDK_WINDOW_STATE_ICONIFIED;
        add_wmf(GDK_FUNC_MINIMIZE);
        gtk_window_iconify(GTK_WINDOW(gtk_widget));
    } else {
        initial_state_mask &= ~GDK_WINDOW_STATE_ICONIFIED;
        gtk_window_deiconify(GTK_WINDOW(gtk_widget));
        gdk_window_focus(gdk_window, GDK_CURRENT_TIME);
    }
}

void WindowContext::set_maximized(bool maximize) {
    LOG1("set_maximized = %d\n", maximize);
    if (maximize) {
        initial_state_mask |= GDK_WINDOW_STATE_MAXIMIZED;
        add_wmf(GDK_FUNC_MAXIMIZE);
        gtk_window_maximize(GTK_WINDOW(gtk_widget));
    } else {
        initial_state_mask &= ~GDK_WINDOW_STATE_MAXIMIZED;
        gtk_window_unmaximize(GTK_WINDOW(gtk_widget));
    }
}

void WindowContext::enter_fullscreen() {
    LOG0("enter_fullscreen\n");
    initial_state_mask |= GDK_WINDOW_STATE_FULLSCREEN;
    gtk_window_fullscreen(GTK_WINDOW(gtk_widget));
}

void WindowContext::exit_fullscreen() {
    LOG0("exit_fullscreen\n");
    initial_state_mask &= ~GDK_WINDOW_STATE_FULLSCREEN;
    gtk_window_unfullscreen(GTK_WINDOW(gtk_widget));
}

void WindowContext::request_focus() {
    if (is_visible()) {
        gtk_window_present(GTK_WINDOW(gtk_widget));
    }
}

void WindowContext::set_focusable(bool focusable) {
    gtk_window_set_accept_focus(GTK_WINDOW(gtk_widget), focusable ? TRUE : FALSE);
}

void WindowContext::set_title(const char* title) {
    gtk_window_set_title(GTK_WINDOW(gtk_widget), title);
}

void WindowContext::set_alpha(double alpha) {
    gtk_window_set_opacity(GTK_WINDOW(gtk_widget), (gdouble)alpha);
}

void WindowContext::set_enabled(bool enabled) {
    is_disabled = !enabled;
    update_window_constraints();
}

void WindowContext::set_minimum_size(int w, int h) {
    resizable.minw = (w <= 0) ? 1 : w;
    resizable.minh = (h <= 0) ? 1 : h;
    update_window_constraints();
}

void WindowContext::set_maximum_size(int w, int h) {
    resizable.maxw = w;
    resizable.maxh = h;
    update_window_constraints();
}

void WindowContext::set_icon(GdkPixbuf* pixbuf) {
    gtk_window_set_icon(GTK_WINDOW(gtk_widget), pixbuf);
}

void WindowContext::to_front() {
    gdk_window_raise(gdk_window);
}

void WindowContext::to_back() {
    gdk_window_lower(gdk_window);
}

void WindowContext::set_modal(bool modal, WindowContext* parent) {
    if (modal) {
        //gtk_window_set_type_hint(GTK_WINDOW(gtk_widget), GDK_WINDOW_TYPE_HINT_DIALOG);
        if (parent) {
            gtk_window_set_transient_for(GTK_WINDOW(gtk_widget), parent->get_gtk_window());
        }
    }
    gtk_window_set_modal(GTK_WINDOW(gtk_widget), modal ? TRUE : FALSE);
}

GtkWindow *WindowContext::get_gtk_window() {
    return GTK_WINDOW(gtk_widget);
}

WindowGeometry WindowContext::get_geometry() {
    return geometry;
}

void WindowContext::update_ontop_tree(bool on_top) {
    bool effective_on_top = on_top || this->on_top;
    gtk_window_set_keep_above(GTK_WINDOW(gtk_widget), effective_on_top ? TRUE : FALSE);
    for (std::set<WindowContext*>::iterator it = children.begin(); it != children.end(); ++it) {
        (*it)->update_ontop_tree(effective_on_top);
    }
}

bool WindowContext::on_top_inherited() {
    WindowContext* o = owner;
    while (o) {
        WindowContext* topO = dynamic_cast<WindowContext*>(o);
        if (!topO) break;
        if (topO->on_top) {
            return true;
        }
        o = topO->owner;
    }
    return false;
}

bool WindowContext::effective_on_top() {
    if (owner) {
        WindowContext* topO = dynamic_cast<WindowContext*>(owner);
        return (topO && topO->effective_on_top()) || on_top;
    }
    return on_top;
}

void WindowContext::add_wmf(GdkWMFunction wmf) {
    if ((initial_wmf & wmf) == 0) {
        current_wmf = (GdkWMFunction)((int)current_wmf | (int)wmf);
        gdk_window_set_functions(gdk_window, current_wmf);
    }
}

void WindowContext::remove_wmf(GdkWMFunction wmf) {
    if ((initial_wmf & wmf) == 0) {
        current_wmf = (GdkWMFunction)((int)current_wmf & ~(int)wmf);
        gdk_window_set_functions(gdk_window, current_wmf);
    }
}

void WindowContext::notify_on_top(bool top) {
    // Do not report effective (i.e. native) values to the FX, only if the user sets it manually
    if (top != effective_on_top() && jwindow) {
        if (on_top_inherited() && !top) {
            // Disallow user's "on top" handling on windows that inherited the property
            gtk_window_set_keep_above(GTK_WINDOW(gtk_widget), TRUE);
        } else {
            on_top = top;
            update_ontop_tree(top);
            mainEnv->CallVoidMethod(jwindow,
                    jWindowNotifyLevelChanged,
                    top ? com_sun_glass_ui_Window_Level_FLOATING :  com_sun_glass_ui_Window_Level_NORMAL);
            CHECK_JNI_EXCEPTION(mainEnv);
        }
    }
}

void WindowContext::set_level(int level) {
    if (level == com_sun_glass_ui_Window_Level_NORMAL) {
        on_top = false;
    } else if (level == com_sun_glass_ui_Window_Level_FLOATING
            || level == com_sun_glass_ui_Window_Level_TOPMOST) {
        on_top = true;
    }
    // We need to emulate always on top behaviour on child windows

    if (!on_top_inherited()) {
        update_ontop_tree(on_top);
    }
}

void WindowContext::set_owner(WindowContext * owner_ctx) {
    owner = owner_ctx;
}

void WindowContext::update_view_size() {
    // Notify the view size only if size is oriented by WINDOW, otherwise it knows its own size
    if (geometry.final_width.type == BOUNDSTYPE_WINDOW
        || geometry.final_height.type == BOUNDSTYPE_WINDOW) {
        if (jview) {
            int cw = gdk_window_get_width(gdk_window);
            int ch = gdk_window_get_height(gdk_window);

            mainEnv->CallVoidMethod(jview, jViewNotifyResize, cw, ch);
            CHECK_JNI_EXCEPTION(mainEnv)
        }
    }
}

gulong WindowContext::XID() {
    return GDK_WINDOW_XID(gdk_window);
}

WindowContext::~WindowContext() {
    LOG1("%lu: ~WindowContext\n", XID());
    disableIME();
    gtk_widget_destroy(gtk_widget);
}

