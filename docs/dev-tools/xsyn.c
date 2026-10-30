// Synthetic input helper for the nbody interactive validation pass.
// Uses XTEST (no dependencies beyond libXtst). Throwaway tool, kept in the
// SDD workspace for reuse in future validation runs. Not product code.
//
// Usage: xsyn <command> [args]
//   move abs X Y | move rel DX DY
//   click left|right|middle [abs X Y | rel DX DY]
//   key KEYSYM          (e.g. space, escape, r)
//   press KEYSYM        (key press only, no release)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
/* XTest prototypes (header absent on this host; ABI is stable): */
int XTestFakeKeyEvent(Display* dpy, unsigned kc, Bool is_press, unsigned delay);
int XTestFakeButtonEvent(Display* dpy, unsigned b, Bool is_press, unsigned delay);
int XTestFakeMotionEvent(Display* dpy, int screen, int x, int y, unsigned delay);
int XTestFakeRelativeMotionEvent(Display* dpy, int x, int y, unsigned delay);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Display* d;

static void fakeKey(const char* name) {
    KeySym ks = XStringToKeysym(name);
    if (ks == NoSymbol) { fprintf(stderr, "unknown keysym %s\n", name); exit(2); }
    KeyCode kc = XKeysymToKeycode(d, ks);
    XTestFakeKeyEvent(d, kc, True, 0);
    XTestFakeKeyEvent(d, kc, False, 0);
    XFlush(d);
}

int main(int argc, char** argv) {
    d = XOpenDisplay(NULL);
    if (!d) { fprintf(stderr, "cannot open display\n"); return 3; }
    if (argc < 2) { fprintf(stderr, "need a command\n"); return 2; }

    if (strcmp(argv[1], "move") == 0) {
        int rel = strcmp(argv[2], "rel") == 0;
        XTestFakeMotionEvent(d, -1, atoi(argv[3]), atoi(argv[4]), 0);
        (void)rel;
    } else if (strcmp(argv[1], "click") == 0) {
        unsigned b = Button1;
        if (strcmp(argv[2], "right") == 0) b = Button3;
        else if (strcmp(argv[2], "middle") == 0) b = Button2;
        int absx = 0, absy = 0, have = 0;
        if (argc >= 6) { absx = atoi(argv[4]); absy = atoi(argv[5]); have = 1; }
        if (have && strcmp(argv[3], "abs") == 0)
            XTestFakeMotionEvent(d, -1, absx, absy, 0);
        else if (have && strcmp(argv[3], "rel") == 0)
            XTestFakeRelativeMotionEvent(d, absx, absy, 0);
        XTestFakeButtonEvent(d, b, True, 0);
        XTestFakeButtonEvent(d, b, False, 0);
    } else if (strcmp(argv[1], "key") == 0) {
        fakeKey(argv[2]);
        return 0;
    } else if (strcmp(argv[1], "press") == 0) {
        KeySym ks = XStringToKeysym(argv[2]);
        XTestFakeKeyEvent(d, XKeysymToKeycode(d, ks), True, 0);
        XFlush(d);
        return 0;
    } else if (strcmp(argv[1], "release") == 0) {
        KeySym ks = XStringToKeysym(argv[2]);
        XTestFakeKeyEvent(d, XKeysymToKeycode(d, ks), False, 0);
        XFlush(d);
        return 0;
    } else if (strcmp(argv[1], "raise") == 0) {
        Window w = (Window)strtoul(argv[2], NULL, 0);
        XRaiseWindow(d, w);
        XSetInputFocus(d, w, RevertToParent, CurrentTime);
    } else if (strcmp(argv[1], "button") == 0) {  // button down|up left|right
        unsigned b = strcmp(argv[3], "right") == 0 ? Button3 : Button1;
        XTestFakeButtonEvent(d, b, strcmp(argv[2], "down") == 0, 0);
    } else {
        fprintf(stderr, "unknown command %s\n", argv[1]);
        return 2;
    }
    XFlush(d);
    return 0;
}
