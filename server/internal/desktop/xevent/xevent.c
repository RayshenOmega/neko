#include "xevent.h"

static int XEventError(Display *display, XErrorEvent *event) {
  char message[100];

  int error;
  error = XGetErrorText(display, event->error_code, message, sizeof(message));
  if (error) {
    goXEventError(event, "Could not get error message.");
  } else {
    goXEventError(event, message);
  }

  return 1;
}

void XEventLoop(char *name) {
  Display *display = XOpenDisplay(name);
  Window root = RootWindow(display, 0);

  int xfixes_event_base, xfixes_error_base;
  if (!XFixesQueryExtension(display, &xfixes_event_base, &xfixes_error_base)) {
    return;
  }

  Atom WM_WINDOW_ROLE = XInternAtom(display, "WM_WINDOW_ROLE", 1);
  Atom XA_CLIPBOARD = XInternAtom(display, "CLIPBOARD", 0);
  XFixesSelectSelectionInput(display, root, XA_CLIPBOARD, XFixesSetSelectionOwnerNotifyMask);
  XFixesSelectCursorInput(display, root, XFixesDisplayCursorNotifyMask);
  XSelectInput(display, root, SubstructureNotifyMask);

  XSync(display, 0);
  XSetErrorHandler(XEventError);

  while (goXEventActive()) {
    XEvent event;
    XNextEvent(display, &event);

    // XFixesDisplayCursorNotify
    if (event.type == xfixes_event_base + 1) {
      XFixesCursorNotifyEvent notifyEvent = *((XFixesCursorNotifyEvent *) &event);
      if (notifyEvent.subtype == XFixesDisplayCursorNotify) {
        goXEventCursorChanged(notifyEvent);
        continue;
      }
    }

    // XFixesSelectionNotifyEvent
    if (event.type == xfixes_event_base + XFixesSelectionNotify) {
      XFixesSelectionNotifyEvent notifyEvent = *((XFixesSelectionNotifyEvent *) &event);
      if (notifyEvent.subtype == XFixesSetSelectionOwnerNotify && notifyEvent.selection == XA_CLIPBOARD) {
        goXEventClipboardUpdated();
        continue;
      }
    }

    // ConfigureNotify
    if (event.type == ConfigureNotify) {
      Window window = event.xconfigure.window;

      char *name;
      XFetchName(display, window, &name);

      XTextProperty role;
      XGetTextProperty(display, window, &role, WM_WINDOW_ROLE);

      goXEventConfigureNotify(display, window, name, role.value);
      XFree(name);
      continue;
    }

    // UnmapNotify
    if (event.type == UnmapNotify) {
      Window window = event.xunmap.window;
      goXEventUnmapNotify(window);
      continue;
    }
  }

  XCloseDisplay(display);
}

void XFileChooserHide(Display *display, Window window) {
  Window root = RootWindow(display, 0);

  // The WM_TRANSIENT_FOR property is defined by the [ICCCM] for managed windows.
  // This specification extends the use of the property to override-redirect windows.
  // If an override-redirect is a pop-up on behalf of another window, then the Client
  // SHOULD set WM_TRANSIENT_FOR on the override-redirect to this other window.
  //
  // As an example, a Client should set WM_TRANSIENT_FOR on dropdown menus to the
  // toplevel application window that contains the menubar.

  // Remove WM_TRANSIENT_FOR
  Atom WM_TRANSIENT_FOR = XInternAtom(display, "WM_TRANSIENT_FOR", 0);
  XDeleteProperty(display, window, WM_TRANSIENT_FOR);

  // Add _NET_WM_STATE_BELOW
  XClientMessageEvent clientMessageEvent;
  memset(&clientMessageEvent, 0, sizeof(clientMessageEvent));

  // window  = the respective client window
  // message_type = _NET_WM_STATE
  // format = 32
  // data.l[0] = the action, as listed below
  //   _NET_WM_STATE_REMOVE  0 // remove/unset property
  //   _NET_WM_STATE_ADD     1 // add/set property
  //   _NET_WM_STATE_TOGGLE  2 // toggle property 
  // data.l[1] = first property to alter
  // data.l[2] = second property to alter
  // data.l[3] = source indication
  // other data.l[] elements = 0

  clientMessageEvent.type         = ClientMessage;
  clientMessageEvent.window       = window;
  clientMessageEvent.message_type = XInternAtom(display, "_NET_WM_STATE", 0);
  clientMessageEvent.format       = 32;
  clientMessageEvent.data.l[0]    = 1;
  clientMessageEvent.data.l[1]    = XInternAtom(display, "_NET_WM_STATE_BELOW", 0);
  clientMessageEvent.data.l[3]    = 1;

  XSendEvent(display, root, 0, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&clientMessageEvent);
}
