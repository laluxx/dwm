/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>
#ifdef __OpenBSD__
#include <sys/sysctl.h>
#include <kvm.h>
#endif /* __OpenBSD */

#include "drw.h"
#include "util.h"
#include "math.h"
#include <unistd.h> // sleep
#include <X11/extensions/Xrender.h>


static int movement_direction = 0;  // 0: no movement, 1: right, -1: left

int clientsInTag(unsigned int tagmask);


/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->mx+(m)->mw) - MAX((x),(m)->mx)) \
                               * MAX(0, MIN((y)+(h),(m)->my+(m)->mh) - MAX((y),(m)->my)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TAGSLENGTH              (LENGTH(tags))
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)
// ORGINAL
#define XRDB_LOAD_COLOR(R,V)    if (XrmGetResource(xrdb, R, NULL, &type, &value) == True) { \
                                  if (value.addr != NULL && strnlen(value.addr, 8) == 7 && value.addr[0] == '#') { \
                                    int i = 1; \
                                    for (; i <= 6; i++) { \
                                      if (value.addr[i] < 48) break; \
                                      if (value.addr[i] > 57 && value.addr[i] < 65) break; \
                                      if (value.addr[i] > 70 && value.addr[i] < 97) break; \
                                      if (value.addr[i] > 102) break; \
                                    } \
                                    if (i == 7) { \
                                      strncpy(V, value.addr, 7); \
                                      V[7] = '\0'; \
                                    } \
                                  } \
                                }



/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel }; /* color schemes */
enum { NetSupported, NetWMName, NetWMIcon, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetDesktopNames, NetDesktopViewport, NetNumberOfDesktops, NetCurrentDesktop, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef struct TagState TagState;
struct TagState {
	int selected;
	int occupied;
	int urgent;
};

typedef struct ClientState ClientState;
struct ClientState {
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
};

typedef union {
	long i;
	unsigned long ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;

struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	unsigned int icw, ich; Picture icon;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, istruefullscreen, isterminal, noswallow, isalwaysontop, iscentered;
	char scratchkey;
	pid_t pid;
	Client *next;
	Client *snext;
	Client *swallowing;
	Monitor *mon;
	Window win;
	ClientState prevstate;
};





typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct Pertag Pertag;
struct Monitor {
	char ltsymbol[16];
    char lastltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by, bh;           /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	int gappx;            /* gaps between windows */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	TagState tagstate; //ipc
	int showbar;
	int topbar;
	Client *clients;
	Client *sel;
	/* Client *lastsel; //ipc */
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
	const Layout *lastlt;  //ipc

	Pertag *pertag;
	Client *lastsel; // last window selected
};





typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int isterminal;
	int noswallow;
	int monitor;
	const char scratchkey;
    int x, y, w, h;  // window position and size
} Rule;


typedef struct {
	int monitor;
	int tag;
	int layout;
	float mfact;
	int nmaster;
	int showbar;
	int topbar;
} MonitorRule;

/* function declarations */
static void setcurrentdesktop(void);
static void setdesktopnames(void);
static void setnumdesktops(void);
static void setviewport(void);
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void deck(Monitor *m);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Picture geticonprop(Window w, unsigned int *icw, unsigned int *ich);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static int handlexevent(struct epoll_event *ev);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void loadxrdb(void);
static void manage(Window w, XWindowAttributes *wa);
static void managealtbar(Window win, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setgaps(const Arg *arg);
static void setlayout(const Arg *arg);
static void setlayoutsafe(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void setupepoll(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void spawnbar();
static void spawnscratch(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglescratch(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void freeicon(Client *c);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmanagealtbar(Window w);
static void unmapnotify(XEvent *e);
static void updatecurrentdesktop(void);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updateicon(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int wmclasscontains(Window win, const char *class, const char *name);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xrdb(const Arg *arg);
static void zoom(const Arg *arg);
static void autostart_exec(void);
static void togglealwaysontop(const Arg *arg);


// PERSONAL FUNCTIONS
int numberOfFloatingClientsOnTag(unsigned int tag);
static void cyclelayout(const Arg *arg);
void toggletruefullscreen(const Arg *arg);
int numberOfVisibleClientsOnTag(unsigned int tag);
static void toggleborder(const Arg *arg);
void togglepeekmode(const Arg *arg);
static void masterstack(Monitor *);
static void checkedgeswitch(void);
void tilefloating(const Arg *arg);
void truefullscreen(Monitor *m);
void togglestack(const Arg *arg);











static pid_t getparentprocess(pid_t p);
static int isdescprocess(pid_t p, pid_t c);
static Client *swallowingclient(Window w);
static Client *termforwin(const Client *c);
static pid_t winpid(Window w);

/* variables */
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static int epoll_fd; //ipc
static int dpy_fd; //ipc
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
/* static Monitor *mons, *selmon; */
static Monitor *mons, *selmon, *lastselmon;
static Window root, wmcheckwin;

static xcb_connection_t *xcon;

/* personal variables */
const Layout *lastlayout = NULL;
static unsigned int previousTag = 0;  // Assuming 0 is an invalid/non-existent tag
static int globalBorderToggled = 1;
// MONOCLE
Client *prevclient = NULL;
int prevclientidx = -1;
Client *dragging = NULL;


#include "ipc.h"

/* configuration, allows nested code to access above variables */
#include "config.h"

#ifdef VERSION
#include "IPCClient.c"
#include "yajl_dumps.c"
#include "ipc.c"
#endif


struct Pertag {
	unsigned int curtag, prevtag; /* current and previous tag */
	int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
	float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
	unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
	int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* dwm will keep pid's of processes from autostart array and kill them at quit */
static pid_t *autostart_pids;
static size_t autostart_len;


// PERSONAL FUNCTIONS




/* void togglestack(const Arg *arg) { */
/*     static Client *hidden = NULL; // To keep track of hidden clients */
/*     Client *c, *t, *first = NULL, *lastHidden = NULL; */

/*     if(!hidden) { // If no hidden windows, hide the stack */
/*         for(c = selmon->clients; c; c = c->next) { */
/*             if(ISVISIBLE(c) && !c->isfloating && !c->isfullscreen) { */
/*                 if(!first) { // Skip the first client (master) */
/*                     first = c; */
/*                     continue; */
/*                 } */

/*                 detach(c); // Unmanage clients in the stack */

/*                 if(lastHidden) { */
/*                     lastHidden->next = c; // Append to the hidden list */
/*                 } else { */
/*                     hidden = c; // Start the hidden list */
/*                 } */
/*                 lastHidden = c; // Update the last hidden pointer */
/*                 // Move windows to the right outside of the screen */
/*                 XMoveWindow(dpy, c->win, selmon->wx + selmon->ww, c->y); */
/*             } */
/*         } */
/*         if(lastHidden) lastHidden->next = NULL; // Ensure the end of the list is NULL */
/*         arrange(selmon); // Rearrange all windows to make the master occupy the whole space */
/*     } else { // If there are hidden windows, show the stack */
/*         for(c = hidden; c; c = t) { */
/*             t = c->next; // Store the next pointer as it will be overwritten by attach */
/*             attach(c); // Reattach the window to the client list */
/*             // Move windows back to their original position */
/*             XMoveWindow(dpy, c->win, c->x - selmon->ww, c->y); */
/*         } */
/*         hidden = NULL; // Clear the hidden list */
/*         arrange(selmon); // Rearrange to restore the original layout */
/*     } */
/* } */




void togglestack(const Arg *arg) {
    static Client *hidden = NULL;
    Client *c, *firstVisible = NULL, *lastHidden = NULL;

    if (!hidden) { // If stack is not hidden
        for (c = selmon->clients; c; c = c->next) {
            if (ISVISIBLE(c) && !c->isfloating && !c->isfullscreen) {
                if (!firstVisible) { // skip the first window
                    firstVisible = c;
                    continue;
                }

                int newPositionX = selmon->mx + selmon->mw; // Move window to the right off-screen

                // Move window
                XMoveWindow(dpy, c->win, newPositionX, c->y);

                // detach from stack
                detachstack(c);

                // detach from client list and add to hidden list
                if (lastHidden) lastHidden->next = c;
                else hidden = c;

                lastHidden = c;
            }
        }
        if (lastHidden) lastHidden->next = NULL;
    } else { // If stack is hidden
        for (c = hidden; c; c = hidden) {
            hidden = c->next;
            attach(c); // reattach
            attachstack(c); // you may need a function like this to reattach to the stack properly.
            // You might need to restore their original position if needed.
            XMapWindow(dpy, c->win);  // Map back to the original position
        }
        lastHidden = NULL; // Reset last hidden
    }
    arrange(selmon);
}





void tilefloating(const Arg *arg) {
    // If there's no selected window, return
    if (!selmon->sel)
        return;

    // If the window is floating, tile it
    if (selmon->sel->isfloating && !selmon->sel->isfixed) {
        selmon->sel->isfloating = 0;

    }

    // Rearrange windows on the current monitor
    arrange(selmon);
}

void setWindowTransparency(Window win, double opacity) {
    unsigned long opacity_value = (unsigned long)(opacity * (double)0xffffffff);
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacity_value, 1);
}

void togglepeekmode(const Arg *arg) {
    static int inPeekMode = 0; // 0 for off, 1 for on
    Client *c;
    int count = 0;

    inPeekMode = !inPeekMode; // Toggle peek mode

    for (c = selmon->clients; c; c = c->next) {
        if (!ISVISIBLE(c)) continue;
        count++;
    }

    if (inPeekMode) {
        for (c = selmon->clients; c; c = c->next) {
            if (!ISVISIBLE(c)) continue;

            // Determine position of the window
            if (c == selmon->clients) { // The first client is usually the master
                if (count == 1) {
                    XMoveWindow(dpy, c->win, c->x, selmon->mh + 10); // Single window, move down smoothly
                } else {
                    XMoveWindow(dpy, c->win, c->x - selmon->ww, c->y); // Master window, move left
                }
            } else if (count == 2) { // If there are two windows, the second one moves down
                XMoveWindow(dpy, c->win, c->x, 2 * selmon->mh); // Move down faster
            } else if (c->y < selmon->mh / 2) { // If the window is above the middle of the screen, it's top right
                XMoveWindow(dpy, c->win, c->x + selmon->ww, c->y); // Move to the right
            } else { // Otherwise, it's bottom left
                XMoveWindow(dpy, c->win, c->x, 2 * selmon->mh); // Move down faster
            }
        }
    } else {
        // Restore the windows to their original positions
        for (c = selmon->clients; c; c = c->next) {
            if (!ISVISIBLE(c)) continue;
            XMoveWindow(dpy, c->win, c->x, c->y);
        }
    }
}

void writewindowcount() {
    FILE *f = fopen("/tmp/dwm-window-count", "w+");
    if (f == NULL) {
        fprintf(stderr, "Error opening file!\n");
        return;
    }

    int count = 0;
    for (Client *c = selmon->clients; c; c = c->next) {
        if (!ISVISIBLE(c)) continue;
        count++;
    }

    fprintf(f, "%d", count);
    fflush(f);  // Flush the file buffer
    ftruncate(fileno(f), ftell(f));  // Truncate the file to the current write position
    fclose(f);
}

int getTagMovementDirection(unsigned int newTag) {
    if (previousTag < newTag) {
        return 1;  // Moving forward
    } else if (previousTag > newTag) {
        return -1; // Moving backward
    } else {
        return 0;  // No movement or same tag
    }
}

void cyclelayout(const Arg *arg) {
    Layout *l;
    int step = arg->i;  // +1 for forwards, -1 for backwards
    for (l = (Layout *)layouts; l != selmon->lt[selmon->sellt]; l++)
        ;

    // Calculate the offset based on the current layout's position and the step
    int offset = (l - layouts) + step;

    // Use modular arithmetic for wrap-around
    offset = (offset + LENGTH(layouts)) % LENGTH(layouts);

    // Set the layout to the newly calculated offset
    setlayout(&((Arg){.v = &layouts[offset]}));
}

void toggletruefullscreen(const Arg *arg) {
    if (selmon->lt[selmon->sellt] != &layouts[4]) {
        lastlayout = selmon->lt[selmon->sellt];
        setlayout(&((Arg) { .v = &layouts[4] }));
    } else if (lastlayout) {
        // Set layout first
        setlayout(&((Arg) { .v = lastlayout }));
        selmon->showbar = 1;
        updatebarpos(selmon);

        // Reset the master factor to its default or desired value
        selmon->mfact = mfact;

        // Iterate over all clients and tile them if they are floating
        Client *c;
        for(c = selmon->clients; c; c = c->next) {
            if(c->isfloating && !c->isfixed)
                c->isfloating = 0;
        }

        // Manually call the tiling arrangement function of the desired layout
        if(lastlayout->arrange == tile)
            tile(selmon);

        // Finally, call arrange to reflect the changes
        arrange(selmon);
    }
}

void toggleborder(const Arg *arg) {
    if (!selmon->sel) return;

    // Toggle the global border state
    if (arg->ui & ShiftMask) {
        globalBorderToggled = !globalBorderToggled;
        for (Client *c = selmon->clients; c; c = c->next) {
            c->bw = globalBorderToggled ? 0 : borderpx;
            configure(c);  // Apply changes immediately
        }
    } else {
        // Toggle border for the focused window only
        selmon->sel->bw = selmon->sel->bw == 0 ? borderpx : 0;
    }

    arrange(selmon);
}



// ADDED
/* void */
/* checkedgeswitch(void) */
/* { */
/*     int x; */
/*     static int wasAtEdge = 0; */
/*     static time_t last_switch_time = 0; */
/*     time_t current_time; */
/*     int isDragging = 0; */

/*     XQueryPointer(dpy, root, &(Window){0}, &(Window){0}, &x, &(int){0}, &(int){0}, &(int){0}, &(unsigned int){0}); */
/*     if (selmon->sel && selmon->sel->isfloating) */
/*         isDragging = 1; // Checking if a window is currently being dragged */

/*     time(&current_time); */

/*     if (x < EDGETHRESHOLD) { */
/*         if (!wasAtEdge || (current_time - last_switch_time) > 2) {  // 2 seconds delay */
/*             if (selmon->tagset[selmon->seltags] > 1) { */
/*                 if ((!isDragging && MOUSEEDGESWITCH) || (isDragging && DRAGGEDGESWITCH)) { */
/*                     view(&((Arg) {.ui = selmon->tagset[selmon->seltags] >> 1})); */
/*                     wasAtEdge = 1; */
/*                     last_switch_time = current_time; */
/*                 } */
/*             } */
/*         } */
/*     } else if (x > (selmon->mx + selmon->mw - EDGETHRESHOLD)) { */
/*         if (!wasAtEdge || (current_time - last_switch_time) > 2) { */
/*             if (selmon->tagset[selmon->seltags] < (1 << (LENGTH(tags) - 1))) { */
/*                 if ((!isDragging && MOUSEEDGESWITCH) || (isDragging && DRAGGEDGESWITCH)) { */
/*                     view(&((Arg) {.ui = selmon->tagset[selmon->seltags] << 1})); */
/*                     wasAtEdge = 1; */
/*                     last_switch_time = current_time; */
/*                 } */
/*             } */
/*         } */
/*     } else { */
/*         wasAtEdge = 0; */
/*     } */
/* } */



// ADDED
/* int cursorOverClient(int x, int y) { */
/*     Client *c; */
/*     for (c = selmon->clients; c; c = c->next) { */
/*         if (ISVISIBLE(c) && */
/*             x > c->x && x < c->x + c->w && */
/*             y > c->y && y < c->y + c->h) { */
/*             return 1; // cursor is over this client window */
/*         } */
/*     } */
/*     return 0; // cursor isn't over any client window */
/* } */


int cursorOverClient(int x, int y) {
    Client *c;
    for (c = selmon->clients; c; c = c->next) {
        if (ISVISIBLE(c)) {
            // Consider left boundary as inside if window width matches monitor width
            int leftBoundary = (c->w == selmon->mw) ? c->x : c->x + 1;

            if (x >= leftBoundary && x < c->x + c->w &&
                y > c->y && y < c->y + c->h) {
                return 1; // cursor is over this client window
            }
        }
    }
    return 0; // cursor isn't over any client window
}




void checkedgeswitch(void) {
    int x, y;
    static int wasAtEdge = 0;
    static time_t last_switch_time = 0;
    time_t current_time;
    int isDragging = 0;

    XQueryPointer(dpy, root, &(Window){0}, &(Window){0}, &x, &y, &(int){0}, &(int){0}, &(unsigned int){0});

    if (cursorOverClient(x, y)) {
        return; // Exit the function if the cursor is over any client window
    }

    if (selmon->sel && selmon->sel->isfloating)
        isDragging = 1; // Checking if a window is currently being dragged

    time(&current_time);

    if (x < EDGETHRESHOLD) {
        if (!wasAtEdge || (current_time - last_switch_time) > 2) {  // 2 seconds delay
            if (selmon->tagset[selmon->seltags] > 1) {
                if ((!isDragging && MOUSEEDGESWITCH) || (isDragging && DRAGGEDGESWITCH)) {
                    view(&((Arg) {.ui = selmon->tagset[selmon->seltags] >> 1}));
                    wasAtEdge = 1;
                    last_switch_time = current_time;
                }
            }
        }
    } else if (x > (selmon->mx + selmon->mw - EDGETHRESHOLD)) {
        if (!wasAtEdge || (current_time - last_switch_time) > 2) {
            if (selmon->tagset[selmon->seltags] < (1 << (LENGTH(tags) - 1))) {
                if ((!isDragging && MOUSEEDGESWITCH) || (isDragging && DRAGGEDGESWITCH)) {
                    view(&((Arg) {.ui = selmon->tagset[selmon->seltags] << 1}));
                    wasAtEdge = 1;
                    last_switch_time = current_time;
                }
            }
        }
    } else {
        wasAtEdge = 0;
    }
}











int clientsInTag(unsigned int tagmask) {
    Client *c;
    for (c = selmon->clients; c; c = c->next)
        if (c->tags & tagmask)
            return 1;
    return 0;
}

int numberOfFloatingClientsOnTag(unsigned int tag) {
    int count = 0;
    for (Client *c = selmon->clients; c; c = c->next) {
        if (c->isfloating && (c->tags & tag)) {
            count++;
        }
    }
    return count;
}

int numberOfVisibleClientsOnTag(unsigned int tagmask) {
    int count = 0;
    Client *c;
    for (c = selmon->clients; c; c = c->next)
        if (c->tags & tagmask)
            count++;
    return count;
}



/* execute command from autostart array */
static void
autostart_exec() {
	size_t i = 0;
	const char *const *args;

	/* count entries */
	for (i = 0; autostart[i][0]; i++);
	autostart_len = i;

	autostart_pids = malloc(autostart_len * sizeof(pid_t));
	for (i = 0; autostart[i][0]; i++) {
		args = autostart[i];
		if ((autostart_pids[i] = fork()) == 0) {
			setsid();
			execvp(args[0], (char *const *)args);
			fprintf(stderr, "dwm: execvp %s\n", args[0]);
			perror(" failed");
			_exit(EXIT_FAILURE);
		}
	}
}



/* function implementations */
/* void */
/* applyrules(Client *c) */
/* { */
/* 	const char *class, *instance; */
/* 	unsigned int i; */
/* 	const Rule *r; */
/* 	Monitor *m; */
/* 	XClassHint ch = { NULL, NULL }; */

/* 	/\* rule matching *\/ */
/* 	c->isfloating = 0; */
/* 	c->tags = 0; */
/* 	c->scratchkey = 0; */
/* 	XGetClassHint(dpy, c->win, &ch); */
/* 	class    = ch.res_class ? ch.res_class : broken; */
/* 	instance = ch.res_name  ? ch.res_name  : broken; */

/* 	for (i = 0; i < LENGTH(rules); i++) { */
/* 		r = &rules[i]; */
/* 		if ((!r->title || strstr(c->name, r->title)) */
/* 		&& (!r->class || strstr(class, r->class)) */
/* 		&& (!r->instance || strstr(instance, r->instance))) */
/* 		{ */
/* 			c->isterminal = r->isterminal; */
/* 			c->noswallow  = r->noswallow; */
/* 			c->isfloating = r->isfloating; */
/* 			c->tags |= r->tags; */
/* 			c->scratchkey = r->scratchkey; */
/* 			for (m = mons; m && m->num != r->monitor; m = m->next); */
/* 			if (m) */
/* 				c->mon = m; */
/* 		} */
/* 	} */
/* 	if (ch.res_class) */
/* 		XFree(ch.res_class); */
/* 	if (ch.res_name) */
/* 		XFree(ch.res_name); */

/* 	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags]; */
/* } */

void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	c->scratchkey = 0;
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isterminal = r->isterminal;
			c->noswallow  = r->noswallow;
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			c->scratchkey = r->scratchkey;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
			if (r->x != -1) c->x = r->x;
			if (r->y != -1) c->y = r->y;
			if (r->w != -1) c->w = r->w;
			if (r->h != -1) c->h = r->h;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);

	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}







int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
}

// alpha
void deck(Monitor *m) {
    unsigned int i, n, h, mw, my, ww;
    int shiftAmount;
    Client *c;
    int currentIdx = -1;
    int selectedIdx = -1;

    // Count visible clients
    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
    if (n == 0) return;

    mw = (n > m->nmaster) ? (m->nmaster ? m->ww * m->mfact : 0) : m->ww;
    ww = m->ww - mw;

    // Determine index of current and selected windows
    for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (c == m->sel) {
            selectedIdx = i;
        }
        if (c == m->clients) {
            currentIdx = i;
        }
    }

    // Determine how much we need to shift the windows. Invert the direction.
    if (selectedIdx != -1 && currentIdx != -1) {
        shiftAmount = (currentIdx - selectedIdx) * ww;
    } else {
        shiftAmount = 0;
    }

    for (i = my = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (i < m->nmaster) {
            h = (m->wh - my) / (MIN(n, m->nmaster) - i);
            resize(c, m->wx + shiftAmount, m->wy + my, mw - (2*c->bw), h - (2*c->bw), False);
            my += HEIGHT(c);
        } else {
            // Position of the stack window relative to the master area
            int relx = (i - m->nmaster) * ww;

            // If it's the first or the second window in the stack, make it visible
            if (relx < 2 * ww) {
                XMoveWindow(dpy, c->win, m->wx + mw + relx + shiftAmount, m->wy);
            } else {
                XMoveWindow(dpy, c->win, m->ww + m->wx, m->wy);  // Move other windows out of view to the right
            }
        }
    }
}

void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

/* void */
/* attach(Client *c) */
/* { */
/* 	c->next = c->mon->clients; */
/* 	c->mon->clients = c; */
/* } */

/* void */
/* attach(Client *c) */
/* { */
/*     if (c->mon->lt[c->mon->sellt]->arrange == masterstack) { */
/*         Client **tc; */
/*         for (tc = &c->mon->clients; *tc; tc = &(*tc)->next); */
/*         *tc = c; */
/*         c->next = NULL; */
/*     } else { */
/*         c->next = c->mon->clients; */
/*         c->mon->clients = c; */
/*     } */
/* } */

void
attach(Client *c)
{
    if (c->mon->lt[c->mon->sellt]->arrange == masterstack ||
        c->mon->lt[c->mon->sellt]->arrange == dwindle ||
        c->mon->lt[c->mon->sellt]->arrange == spiral) {
        Client **tc;
        for (tc = &c->mon->clients; *tc; tc = &(*tc)->next);
        *tc = c;
        c->next = NULL;
    } else {
        c->next = c->mon->clients;
        c->mon->clients = c;
    }
}




void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
swallow(Client *p, Client *c)
{

	if (c->noswallow || c->isterminal)
		return;
	if (c->noswallow && !swallowfloating && c->isfloating)
		return;

	detach(c);
	detachstack(c);

	setclientstate(c, WithdrawnState);
	XUnmapWindow(dpy, p->win);

	p->swallowing = c;
	c->mon = p->mon;

	Window w = p->win;
	p->win = c->win;
	c->win = w;
	updatetitle(p);
	XMoveResizeWindow(dpy, p->win, p->x, p->y, p->w, p->h);
	arrange(p->mon);
	configure(p);
	updateclientlist();
}

void
unswallow(Client *c)
{
	c->win = c->swallowing->win;

	free(c->swallowing);
	c->swallowing = NULL;

	/* unfullscreen the client */
	setfullscreen(c, 0);
	updatetitle(c);
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
	setclientstate(c, NormalState);
	focus(NULL);
	arrange(c->mon);
}

void
buttonpress(XEvent *e)
{
	unsigned int i, x, click;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + blw)
			click = ClkLtSymbol;
		else if (ev->x > selmon->ww - (int)TEXTW(stext))
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		free(scheme[i]);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);

	ipc_cleanup();

	if (close(epoll_fd) < 0) {
			fprintf(stderr, "Failed to close epoll file descriptor\n");
	}
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	if (!usealtbar) {
		XUnmapWindow(dpy, mon->barwin);
		XDestroyWindow(dpy, mon->barwin);
	}
	free(mon);
}

// ORIGINAL
/* void */
/* clientmessage(XEvent *e) */
/* { */
/* 	XClientMessageEvent *cme = &e->xclient; */
/* 	Client *c = wintoclient(cme->window); */

/* 	if (!c) */
/* 		return; */
/* 	if (cme->message_type == netatom[NetWMState]) { */
/* 		if (cme->data.l[1] == netatom[NetWMFullscreen] */
/* 		|| cme->data.l[2] == netatom[NetWMFullscreen]) */
/* 			setfullscreen(c, (cme->data.l[0] == 1 /\* _NET_WM_STATE_ADD    *\/ */
/* 				|| cme->data.l[0] == 2 /\* _NET_WM_STATE_TOGGLE *\/)); */
/* 	} else if (cme->message_type == netatom[NetActiveWindow]) { */
/* 		if (c != selmon->sel && !c->isurgent) */
/* 			seturgent(c, 1); */
/* 	} */
/* } */

// URGENT
void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent) {
			seturgent(c, 1);
			// Automatically switch to the tag of the urgent client
			selmon->tagset[selmon->seltags] = c->tags;
			focus(NULL);
			arrange(selmon);
		}
	}
}


void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				/* XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh); */
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, m->bh); // anybar

			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}
// ORIGINAL
/* Monitor * */
/* createmon(void) */
/* { */
/* 	Monitor *m; */
/* 	unsigned int i; */

/* 	m = ecalloc(1, sizeof(Monitor)); */
/* 	m->tagset[0] = m->tagset[1] = 1; */
/* 	m->mfact = mfact; */
/* 	m->nmaster = nmaster; */
/* 	m->showbar = showbar; */
/* 	m->topbar = topbar; */
/* 	m->gappx = gappx; */
/* 	m->lt[0] = &layouts[0]; */
/* 	m->lt[1] = &layouts[1 % LENGTH(layouts)]; */
/* 	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol); */
/* 	m->pertag = ecalloc(1, sizeof(Pertag)); */
/* 	m->pertag->curtag = m->pertag->prevtag = 1; */

/* 	for (i = 0; i <= LENGTH(tags); i++) { */
/* 		m->pertag->nmasters[i] = m->nmaster; */
/* 		m->pertag->mfacts[i] = m->mfact; */

/* 		m->pertag->ltidxs[i][0] = m->lt[0]; */
/* 		m->pertag->ltidxs[i][1] = m->lt[1]; */
/* 		m->pertag->sellts[i] = m->sellt; */

/* 		m->pertag->showbars[i] = m->showbar; */
/* 	} */

/* 	return m; */
/* } */

Monitor *
createmon(void)
{
	Monitor *m, *mon;
	unsigned int i;
	int mi, j, layout;
	const MonitorRule *mr;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->bh = bh; //anybar
	m->gappx = gappx;

	// Initialize with default values first
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);

	for (mi = 0, mon = mons; mon; mon = mon->next, mi++);
	for (j = 0; j < LENGTH(monrules); j++) {
		mr = &monrules[j];
		if ((mr->monitor == -1 || mr->monitor == mi)
				&& (mr->tag <= 0 || (m->tagset[0] & (1 << (mr->tag - 1))))
		) {
			layout = MAX(mr->layout, 0);
			layout = MIN(layout, LENGTH(layouts) - 1);
			m->lt[0] = &layouts[layout];
			m->lt[1] = &layouts[1 % LENGTH(layouts)];
			strncpy(m->ltsymbol, layouts[layout].symbol, sizeof m->ltsymbol);

			if (mr->mfact > -1)
				m->mfact = mr->mfact;
			if (mr->nmaster > -1)
				m->nmaster = mr->nmaster;
			if (mr->showbar > -1)
				m->showbar = mr->showbar;
			if (mr->topbar > -1)
				m->topbar = mr->topbar;
			break;
		}
	}

	m->pertag = ecalloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;

		for (j = 0; j < LENGTH(monrules); j++) {
			mr = &monrules[j];
			if ((mr->monitor == -1 || mr->monitor == mi) && (mr->tag == -1 || mr->tag == i)) {
				layout = MAX(mr->layout, 0);
				layout = MIN(layout, LENGTH(layouts) - 1);
				m->pertag->ltidxs[i][0] = &layouts[layout];
				m->pertag->ltidxs[i][1] = m->lt[0];
				m->pertag->nmasters[i] = (mr->nmaster > -1 ? mr->nmaster : m->nmaster);
				m->pertag->mfacts[i] = (mr->mfact > -1 ? mr->mfact : m->mfact);
				m->pertag->showbars[i] = (mr->showbar > -1 ? mr->showbar : m->showbar);
				break;
			}
		}

		m->pertag->sellts[i] = m->sellt;
	}

	return m;
}


void
destroynotify(XEvent *e)
{
	Client *c;
    Monitor *m; // anybar
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else if ((m = wintomon(ev->window)) && m->barwin == ev->window)  // any
		unmanagealtbar(ev->window);                                  // bar
	else if ((c = swallowingclient(ev->window)))
		unmanage(c->swallowing, 1);
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void
drawbar(Monitor *m)
{
	if (usealtbar)
		return;

	int x, w, tw = 0;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client *c;

	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon) { /* status is only drawn on selected monitor */
		drw_setscheme(drw, scheme[SchemeNorm]);
		tw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
		drw_text(drw, m->ww - tw, 0, tw, bh, 0, stext, 0);
	}

	for (c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i)
			drw_rect(drw, x + boxs, boxs, boxw, boxw,
				m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
				urg & 1 << i);
		x += w;
	}
	w = blw = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

	if ((w = m->ww - tw - x) > bh) {
		if (m->sel) {
			drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
			drw_text(drw, x, 0, w, bh, lrpad / 2 + (m->sel->icon ? m->sel->icw + ICONSPACING : 0), m->sel->name, 0);
			if (m->sel->icon) drw_pic(drw, x + lrpad / 2, (bh - m->sel->ich) / 2, m->sel->icw, m->sel->ich, m->sel->icon);
			if (m->sel->isfloating) {
 				drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
				if (m->sel->isalwaysontop)
					drw_rect(drw, x + boxs, bh - boxw, boxw, boxw, 0, 0);
			}

		} else {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w, bh, 1, 1);
		}
	}
	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}


// ORIGINAL
/* void */
/* focus(Client *c) */
/* { */
/* 	if (!c || !ISVISIBLE(c)) */
/* 		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext); */
/* 	if (selmon->sel && selmon->sel != c) */
/* 		unfocus(selmon->sel, 0); */
/* 	if (c) { */
/* 		if (c->mon != selmon) */
/* 			selmon = c->mon; */
/* 		if (c->isurgent) */
/* 			seturgent(c, 0); */
/* 		detachstack(c); */
/* 		attachstack(c); */
/* 		grabbuttons(c, 1); */
/* 		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel); */
/* 		setfocus(c); */
/* 	} else { */
/* 		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime); */
/* 		XDeleteProperty(dpy, root, netatom[NetActiveWindow]); */
/* 	} */
/* 	selmon->sel = c; */
/* 	if (selmon->lt[selmon->sellt]->arrange == monocle) */
/* 		arrangemon(selmon); */
/* 	drawbars(); */
/* } */

// last
/* void */
/* focus(Client *c) */
/* { */
/*     if (!c || !ISVISIBLE(c)) */
/*         for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext); */
/*     if (selmon->sel && selmon->sel != c) */
/*         unfocus(selmon->sel, 0); */
/*     if (c) { */
/*         if (c->mon != selmon) */
/*             selmon = c->mon; */
/*         if (c->isurgent) */
/*             seturgent(c, 0); */
/*         detachstack(c); */
/*         attachstack(c); */
/*         grabbuttons(c, 1); */
/*         XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel); */
/*         setfocus(c); */
/*     } else { */
/*         XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime); */
/*         XDeleteProperty(dpy, root, netatom[NetActiveWindow]); */
/*     } */

/*     prevclient = selmon->sel; // Store the current client as the previous client before updating */
/*     Client *cc; */
/*     int idx = 0; */
/*     for (cc = selmon->clients; cc && cc != prevclient; cc = cc->next) { */
/*         if (ISVISIBLE(cc)) idx++; */
/*     } */
/*     prevclientidx = (cc == prevclient) ? idx : -1; */

/*     selmon->sel = c; */

/*     if (selmon->lt[selmon->sellt]->arrange == monocle) */
/*         arrangemon(selmon); */
/*     drawbars(); */
/* } */

void focus(Client *c) {
    if (!c || !ISVISIBLE(c))
        for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
    if (selmon->sel && selmon->sel != c)
        unfocus(selmon->sel, 0);
    if (c) {
        if (c->mon != selmon)
            selmon = c->mon;
        if (c->isurgent)
            seturgent(c, 0);
        detachstack(c);
        attachstack(c);
        grabbuttons(c, 1);
        XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
        setfocus(c);
    } else {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
    prevclient = selmon->sel; // Store the current client as the previous client before updating
    Client *cc;
    int idx = 0;
    for (cc = selmon->clients; cc && cc != prevclient; cc = cc->next) {
        if (ISVISIBLE(cc)) idx++;
    }
    prevclientidx = (cc == prevclient) ? idx : -1;

    selmon->sel = c;

    if (selmon->lt[selmon->sellt]->arrange == monocle)
        arrangemon(selmon);
    if (selmon->lt[selmon->sellt]->arrange == deck) // The new lines added from your patch
        arrangemon(selmon);
    drawbars();
}


/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

// ORIGINAL
/* void */
/* focusstack(const Arg *arg) */
/* { */
/* 	Client *c = NULL, *i; */

/* 	if (!selmon->sel || selmon->sel->isfullscreen) */
/* 		return; */
/* 	if (arg->i > 0) { */
/* 		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next); */
/* 		if (!c) */
/* 			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next); */
/* 	} else { */
/* 		for (i = selmon->clients; i != selmon->sel; i = i->next) */
/* 			if (ISVISIBLE(i)) */
/* 				c = i; */
/* 		if (!c) */
/* 			for (; i; i = i->next) */
/* 				if (ISVISIBLE(i)) */
/* 					c = i; */
/* 	} */
/* 	if (c) { */
/* 		focus(c); */
/* 		restack(selmon); */
/* 	} */
/* } */

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel)
		return;

    // If lockfullscreen is enabled and the current window is in fullscreen mode, return.
	if (lockfullscreen && selmon->sel->isfullscreen)
		return;

	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
		XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w/2, c->h/2);
	}
}





Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

static uint32_t prealpha(uint32_t p) {
	uint8_t a = p >> 24u;
	uint32_t rb = (a * (p & 0xFF00FFu)) >> 8u;
	uint32_t g = (a * (p & 0x00FF00u)) >> 8u;
	return (rb & 0xFF00FFu) | (g & 0x00FF00u) | (a << 24u);
}

Picture
geticonprop(Window win, unsigned int *picw, unsigned int *pich)
{
	int format;
	unsigned long n, extra, *p = NULL;
	Atom real;

	if (XGetWindowProperty(dpy, win, netatom[NetWMIcon], 0L, LONG_MAX, False, AnyPropertyType, 
						   &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return None; 
	if (n == 0 || format != 32) { XFree(p); return None; }

	unsigned long *bstp = NULL;
	uint32_t w, h, sz;
	{
		unsigned long *i; const unsigned long *end = p + n;
		uint32_t bstd = UINT32_MAX, d, m;
		for (i = p; i < end - 1; i += sz) {
			if ((w = *i++) >= 16384 || (h = *i++) >= 16384) { XFree(p); return None; }
			if ((sz = w * h) > end - i) break;
			if ((m = w > h ? w : h) >= ICONSIZE && (d = m - ICONSIZE) < bstd) { bstd = d; bstp = i; }
		}
		if (!bstp) {
			for (i = p; i < end - 1; i += sz) {
				if ((w = *i++) >= 16384 || (h = *i++) >= 16384) { XFree(p); return None; }
				if ((sz = w * h) > end - i) break;
				if ((d = ICONSIZE - (w > h ? w : h)) < bstd) { bstd = d; bstp = i; }
			}
		}
		if (!bstp) { XFree(p); return None; }
	}

	if ((w = *(bstp - 2)) == 0 || (h = *(bstp - 1)) == 0) { XFree(p); return None; }

	uint32_t icw, ich;
	if (w <= h) {
		ich = ICONSIZE; icw = w * ICONSIZE / h;
		if (icw == 0) icw = 1;
	}
	else {
		icw = ICONSIZE; ich = h * ICONSIZE / w;
		if (ich == 0) ich = 1;
	}
	*picw = icw; *pich = ich;

	uint32_t i, *bstp32 = (uint32_t *)bstp;
	for (sz = w * h, i = 0; i < sz; ++i) bstp32[i] = prealpha(bstp[i]);

	Picture ret = drw_picture_create_resized(drw, (char *)bstp, w, h, icw, ich);
	XFree(p);

	return ret;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
managealtbar(Window win, XWindowAttributes *wa)
{
	Monitor *m;
	if (!(m = recttomon(wa->x, wa->y, wa->width, wa->height)))
		return;

	m->barwin = win;
	m->by = wa->y;
	bh = m->bh = wa->height;
	updatebarpos(m);
	arrange(m);
	XSelectInput(dpy, win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	XMoveResizeWindow(dpy, win, wa->x, wa->y, wa->width, wa->height);
	XMapWindow(dpy, win);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &win, 1);
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
	}
}

int
handlexevent(struct epoll_event *ev)
{
	if (ev->events & EPOLLIN) {
		XEvent ev;
		while (running && XPending(dpy)) {
			XNextEvent(dpy, &ev);
			if (handler[ev.type]) {
				handler[ev.type](&ev); /* call handler */
				ipc_send_events(mons, &lastselmon, selmon);
			}
		}
	} else if (ev-> events & EPOLLHUP) {
		return -1;
	}

	return 0;
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

//HERE
void
loadxrdb()
{
  Display *display;
  char * resm;
  XrmDatabase xrdb;
  char *type;
  XrmValue value;

  display = XOpenDisplay(NULL);

  if (display != NULL) {
    resm = XResourceManagerString(display);

    if (resm != NULL) {
      xrdb = XrmGetStringDatabase(resm);

      if (xrdb != NULL) {
	XRDB_LOAD_COLOR("dwm.color0", normbordercolor);
	XRDB_LOAD_COLOR("dwm.color8", selbordercolor);
	XRDB_LOAD_COLOR("dwm.color0", normbgcolor);
	XRDB_LOAD_COLOR("dwm.color6", normfgcolor);
	XRDB_LOAD_COLOR("dwm.color0", selfgcolor);
	XRDB_LOAD_COLOR("dwm.color14", selbgcolor);
      }
    }
  }

  XCloseDisplay(display);
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL, *term = NULL;
	Window trans = None;
	XWindowChanges wc;
	XClassHint classres = { NULL, NULL };

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->pid = winpid(w);

	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

	updateicon(c);
	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
		term = termforwin(c);
	}

	if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	c->y = MAX(c->y, ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
		&& (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
	c->bw = globalBorderToggled ? 0 : borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c);
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);

	/* c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2; */
	/* c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2; */

    if (c->x == c->mon->mx && c->y == c->mon->my) {         // DONT
        c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;   // ALWAYS
        c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2; // CENTER
    }                                                    // thats rude


	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);

	// Check if the window's class is in the alwaysontopclasses
	if (XGetClassHint(dpy, c->win, &classres)) {
		for (int i = 0; i < LENGTH(alwaysontopclasses); i++) {
		    if (strcmp(classres.res_class, alwaysontopclasses[i]) == 0) {
		        c->isfloating = 1;
		        c->isalwaysontop = 1;
		        break;
		    }
		}
		if (classres.res_name) XFree(classres.res_name);
		if (classres.res_class) XFree(classres.res_class);
	}

	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);

	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h);
	setclientstate(c, NormalState);
	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	if (term)
		swallow(term, c);
	focus(NULL);
}



void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

/* void */
/* maprequest(XEvent *e) */
/* { */
/* 	static XWindowAttributes wa; */
/* 	XMapRequestEvent *ev = &e->xmaprequest; */

/* 	if (!XGetWindowAttributes(dpy, ev->window, &wa)) */
/* 		return; */
/* 	if (wa.override_redirect) */
/* 		return; */
/* 	if (!wintoclient(ev->window)) */
/* 		manage(ev->window, &wa); */
/* } */

void maprequest(XEvent *e) {
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;

    if (!XGetWindowAttributes(dpy, ev->window, &wa))
        return;
    if (wa.override_redirect)
        return;
    /* if (!wintoclient(ev->window)) */
	if (wmclasscontains(ev->window, altbarclass, "")) //anybar
		managealtbar(ev->window, &wa);
	else if (!wintoclient(ev->window))
        manage(ev->window, &wa);

    writewindowcount();  // Update the window count after managing a new window
}

// ORIGINAL
void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for (c = m->stack; c && (!ISVISIBLE(c) || c->isfloating); c = c->snext);
	if (c && !c->isfloating) {
		XMoveWindow(dpy, c->win, m->wx, m->wy);
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
		c = c->snext;
	}
	for (; c; c = c->snext)
		if (!c->isfloating && ISVISIBLE(c))
			XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
}

// alphα + resize
/* void monocle(Monitor *m) { */
/*     unsigned int n = 0; */
/*     Client *c, *focused = NULL; */

/*     for (c = m->clients; c; c = c->next) { */
/*         if (ISVISIBLE(c)) */
/*             n++; */
/*         if (c == selmon->sel) */
/*             focused = c;  // store the currently focused client */
/*     } */

/*     if (n > 0) /\* override layout symbol *\/ */
/*         snprintf(m->ltsymbol, sizeof m->ltsymbol, " %d", n); */

/*     if (focused) { */
/*         // Grow the window gaining focus */
/*         XMoveWindow(dpy, focused->win, m->wx, m->wy); */
/*         resize(focused, m->wx, m->wy, m->ww - 2 * focused->bw, m->wh - 2 * focused->bw, 0); */
/*         setWindowTransparency(focused->win, 1.0);  // make it opaque */
/*     } */

/*     for (c = m->stack; c; c = c->snext) { */
/*         if (c != focused && !c->isfloating && ISVISIBLE(c)) { */
/*             // Shrink the window losing focus */
/*             XMoveWindow(dpy, c->win, m->wx + (m->ww / 4), m->wy + (m->wh / 4)); */
/*             resize(c, m->wx + (m->ww / 4), m->wy + (m->wh / 4), m->ww / 2 - 2 * c->bw, m->wh / 2 - 2 * c->bw, 0); */
/*             setWindowTransparency(c->win, 0.0);  // make these windows transparent */
/*         } */
/*     } */
/* } */


void truefullscreen(Monitor *m) {
    Client *c;

    for (c = m->clients; c; c = c->next) {
        if (ISVISIBLE(c)) {
            // Set the window to floating mode
            c->isfloating = 1;
            c->istruefullscreen = 1;  // Add this line

            // Move and resize the window to cover the entire screen
            XMoveResizeWindow(dpy, c->win, m->mx, m->my, m->mw, m->mh);

            // Hide borders
            c->bw = 0;
            XConfigureWindow(dpy, c->win, CWBorderWidth, &(XWindowChanges){.border_width = c->bw});
        }
    }
}



// stack layout idea TODO
/* void stack(Monitor *m) { */
/*     unsigned int n = 0; */
/*     Client *c; */
/*     int offset = 30;  // The basic offset for each window */

/*     for (c = m->clients; c; c = c->next) { */
/*         if (ISVISIBLE(c)) { */
/*             n++; */
/*         } */
/*     } */

/*     if (n > 0) /\* override layout symbol *\/ */
/*         snprintf(m->ltsymbol, sizeof m->ltsymbol, " %d", n); */

/*     int shift_amount = 15;  // The amount we'll shift windows for each additional window */
/*     int shift_limit = 4;  // After this many windows, we'll start shifting in the other direction */

/*     // Calculate the starting point. If n is greater than shift_limit, we start shifting bottom-right. */
/*     int start_offset_x = m->wx + (n > shift_limit ? (n - shift_limit) * shift_amount : 0); */
/*     int start_offset_y = m->wy + (n > shift_limit ? (n - shift_limit) * shift_amount : 0); */

/*     double opacityDecrement = 0.15;  // Each subsequent window will be more transparent */
/*     double opacity = 1.0 - opacityDecrement;  // Starting opacity for the first window */

/*     for (c = m->stack; c; c = c->snext) { */
/*         if (!c->isfloating && ISVISIBLE(c)) { */
/*             // The first set of windows shift up-left; the rest shift down-right. */
/*             int adjusted_offset_x = start_offset_x + (n <= shift_limit ? -(n * shift_amount) : offset); */
/*             int adjusted_offset_y = start_offset_y + (n <= shift_limit ? -(n * shift_amount) : offset); */

/*             XMoveWindow(dpy, c->win, adjusted_offset_x, adjusted_offset_y); */
/*             resize(c, adjusted_offset_x, adjusted_offset_y, m->ww / 1.2 - 2 * c->bw, m->wh / 1.2 - 2 * c->bw, 0); */

/*             setWindowTransparency(c->win, opacity); */
/*             opacity -= opacityDecrement; */
/*             if (opacity < 0.2) opacity = 0.2;  // Set a minimum opacity */

/*             if (n <= shift_limit) { */
/*                 n--;  // Decrement for the top-left shift */
/*             } else { */
/*                 offset += 30;  // Increase offset for the bottom-right shift */
/*             } */
/*         } */
/*     } */
/* } */





void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

// ORIGINAL
/* void */
/* movemouse(const Arg *arg) */
/* { */
/* 	int x, y, ocx, ocy, nx, ny; */
/* 	Client *c; */
/* 	Monitor *m; */
/* 	XEvent ev; */
/* 	Time lasttime = 0; */

/* 	if (!(c = selmon->sel)) */
/* 		return; */
/* 	restack(selmon); */
/* 	ocx = c->x; */
/* 	ocy = c->y; */
/* 	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, */
/* 		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess) */
/* 		return; */
/* 	if (!getrootptr(&x, &y)) */
/* 		return; */
/* 	do { */
/* 		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev); */
/* 		switch(ev.type) { */
/* 		case ConfigureRequest: */
/* 		case Expose: */
/* 		case MapRequest: */
/* 			handler[ev.type](&ev); */
/* 			break; */
/* 		case MotionNotify: */
/* 			if ((ev.xmotion.time - lasttime) <= (1000 / 60)) */
/* 				continue; */
/* 			lasttime = ev.xmotion.time; */

/* 			nx = ocx + (ev.xmotion.x - x); */
/* 			ny = ocy + (ev.xmotion.y - y); */
/* 			if (abs(selmon->wx - nx) < snap) */
/* 				nx = selmon->wx; */
/* 			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap) */
/* 				nx = selmon->wx + selmon->ww - WIDTH(c); */
/* 			if (abs(selmon->wy - ny) < snap) */
/* 				ny = selmon->wy; */
/* 			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap) */
/* 				ny = selmon->wy + selmon->wh - HEIGHT(c); */
/* 			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange */
/* 			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap)) */
/* 				togglefloating(NULL); */
/* 			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) */
/* 				resize(c, nx, ny, c->w, c->h, 1); */
/* 			break; */
/* 		} */
/* 	} while (ev.type != ButtonRelease); */
/* 	XUngrabPointer(dpy, CurrentTime); */
/* 	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) { */
/* 		sendmon(c, m); */
/* 		selmon = m; */
/* 		focus(NULL); */
/* 	} */
/* } */


void
movemouse(const Arg *arg)
{
    static Time lastswitch = 0;  // New timestamp for tracking last tag switch
    int x, y, ocx, ocy, nx, ny;
    Client *c;
    Monitor *m;
    XEvent ev;
    Time lasttime = 0;

    if (!(c = selmon->sel))
        return;
    dragging = c;  // Set the dragging client when dragging begins
    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
        None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
        return;
    if (!getrootptr(&x, &y))
        return;
    do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
        switch(ev.type) {
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            if ((ev.xmotion.time - lasttime) <= (1000 / 60))
                continue;
            lasttime = ev.xmotion.time;

            nx = ocx + (ev.xmotion.x - x);
            ny = ocy + (ev.xmotion.y - y);
            if (abs(selmon->wx - nx) < snap)
                nx = selmon->wx;
            else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
                nx = selmon->wx + selmon->ww - WIDTH(c);
            if (abs(selmon->wy - ny) < snap)
                ny = selmon->wy;
            else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
                ny = selmon->wy + selmon->wh - HEIGHT(c);
            if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
            && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
                togglefloating(NULL);
            if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
                resize(c, nx, ny, c->w, c->h, 1);

            // Introducing a delay mechanism for tag switching
            /* if ((ev.xmotion.time - lastswitch) >= (1000 * 1)) {  // 1 second delay */
            if (DRAGGEDGESWITCH && (ev.xmotion.time - lastswitch) >= (1000 * 1)) {
                if (ev.xmotion.x >= selmon->mx + selmon->mw - EDGETHRESHOLD) {
                    if (selmon->tagset[selmon->seltags] < (1 << (LENGTH(tags) - 1))) {
                        Arg a = {.ui = selmon->tagset[selmon->seltags] << 1};
                        tag(&a);
                        view(&a);
                        nx = selmon->mx;
                        lastswitch = ev.xmotion.time;  // Update the timestamp
                    }
                } else if (ev.xmotion.x <= selmon->mx + EDGETHRESHOLD) {
                    if (selmon->tagset[selmon->seltags] > 1) {
                        Arg a = {.ui = selmon->tagset[selmon->seltags] >> 1};
                        tag(&a);
                        view(&a);
                        nx = selmon->mx + selmon->mw - WIDTH(c);
                        lastswitch = ev.xmotion.time;  // Update the timestamp
                    }
                }
            }
            break;
        }
    } while (ev.type != ButtonRelease);
    XUngrabPointer(dpy, CurrentTime);
    if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
        sendmon(c, m);
        selmon = m;
        focus(NULL);
    }
    dragging = NULL; // Reset dragging client
}



Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		else if (ev->atom == netatom[NetWMIcon]) {
			updateicon(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
quit(const Arg *arg)
{
	size_t i;

	/* kill child processes */
	for (i = 0; i < autostart_len; i++) {
		if (0 < autostart_pids[i]) {
			kill(autostart_pids[i], SIGTERM);
			waitpid(autostart_pids[i], NULL, 0);
		}
	}

	running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	int ocx2, ocy2, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	int horizcorner, vertcorner;
	int di;
	unsigned int dui;
	Window dummy;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	ocx2 = c->x + c->w;
	ocy2 = c->y + c->h;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!XQueryPointer (dpy, c->win, &dummy, &dummy, &di, &di, &nx, &ny, &dui))
	       return;
	horizcorner = nx < c->w / 2;
	vertcorner = ny < c->h / 2;
	XWarpPointer (dpy, None, c->win, 0, 0, 0, 0,
		      horizcorner ? (-c->bw) : (c->w + c->bw - 1),
		      vertcorner ? (-c->bw) : (c->h + c->bw - 1));

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			nx = horizcorner ? ev.xmotion.x : c->x;
			ny = vertcorner ? ev.xmotion.y : c->y;
			nw = MAX(horizcorner ? (ocx2 - nx) : (ev.xmotion.x - ocx - 2 * c->bw + 1), 1);
			nh = MAX(vertcorner ? (ocy2 - ny) : (ev.xmotion.y - ocy - 2 * c->bw + 1), 1);

			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
		      horizcorner ? (-c->bw) : (c->w + c->bw - 1),
		      vertcorner ? (-c->bw) : (c->h + c->bw - 1));
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);

	/* raise the aot window */
	for(Monitor *m_search = mons; m_search; m_search = m_search->next){
		for(c = m_search->clients; c; c = c->next){
			if(c->isalwaysontop){
				XRaiseWindow(dpy, c->win);
				break;
			}
		}
	}

	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

// original
/* void */
/* run(void) */
/* { */
/* 	XEvent ev; */
/* 	/\* main event loop *\/ */
/* 	XSync(dpy, False); */
/* 	while (running && !XNextEvent(dpy, &ev)) */
/* 		if (handler[ev.type]) */
/* 			handler[ev.type](&ev); /\* call handler *\/ */
/* } */

/* void */
/* run(void) */
/* { */
/*     XEvent ev; */
/*     while (running) { */
/*         XSync(dpy, False); */
/*         while (XPending(dpy)) { */
/*             XNextEvent(dpy, &ev); */
/*             if (handler[ev.type]) */
/*                 handler[ev.type](&ev); /\* call handler *\/ */
/*         } */
/*         checkedgeswitch(); */
/*     } */
/* } */

// ipc
void run(void) {
    int event_count = 0;
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    XSync(dpy, False);
    while (running) {
        // Handle X events
        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (handler[xev.type])
                handler[xev.type](&xev);  // call handler
        }

        // Check additional events registered with epoll
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < event_count; i++) {
            int event_fd = events[i].data.fd;
            // Handle events from different sources as per the patch
            if (event_fd == dpy_fd) {
                if (handlexevent(events + i) == -1)
                    return;
            } else if (event_fd == ipc_get_sock_fd()) {
                ipc_handle_socket_epoll_event(events + i);
            } else if (ipc_is_client_registered(event_fd)){
                if (ipc_handle_client_epoll_event(events + i, mons, &lastselmon, selmon,
                            tags, LENGTH(tags), layouts, LENGTH(layouts)) < 0) {
                    fprintf(stderr, "Error handling IPC event on fd %d\n", event_fd);
                }
            } else {
                fprintf(stderr, "Got event from unknown fd %d, ptr %p, u32 %d, u64 %lu",
                        event_fd, events[i].data.ptr, events[i].data.u32,
                        events[i].data.u64);
                fprintf(stderr, " with events %d\n", events[i].events);
                return;
            }
        }

        // Call your existing functionality
        checkedgeswitch();
    }
}



void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wmclasscontains(wins[i], altbarclass, ""))
				managealtbar(wins[i], &wa);
			else if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}
void
setcurrentdesktop(void){
	long data[] = { 0 };
	XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}
void setdesktopnames(void){
	XTextProperty text;
	/* Xutf8TextListToTextProperty(dpy, tags, TAGSLENGTH, XUTF8StringStyle, &text); */ // ORIGINAL
	Xutf8TextListToTextProperty(dpy, (char **)tags, TAGSLENGTH, XUTF8StringStyle, &text);
	XSetTextProperty(dpy, root, &text, netatom[NetDesktopNames]);
}

int
sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void
setnumdesktops(void){
	long data[] = { TAGSLENGTH };
	XChangeProperty(dpy, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}

void
setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
	}
}

void
setgaps(const Arg *arg)
{
	if ((arg->i == 0) || (selmon->gappx + arg->i < 0))
		selmon->gappx = 0;
	else
		selmon->gappx += arg->i;
	arrange(selmon);
}

void
setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void
setlayoutsafe(const Arg *arg)
{
	const Layout *ltptr = (Layout *)arg->v;
	if (ltptr == 0)
			setlayout(arg);
	for (int i = 0; i < LENGTH(layouts); i++) {
		if (ltptr == &layouts[i])
			setlayout(arg);
	}
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon);
}

void
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = usealtbar ? 0 : drw->fonts->h + 2;
	updategeom();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMIcon] = XInternAtom(dpy, "_NET_WM_ICON", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netatom[NetDesktopViewport] = XInternAtom(dpy, "_NET_DESKTOP_VIEWPORT", False);
	netatom[NetNumberOfDesktops] = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
	netatom[NetCurrentDesktop] = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	netatom[NetDesktopNames] = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);

	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	setnumdesktops();
	setcurrentdesktop();
	setdesktopnames();
	setviewport();
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
	spawnbar(); //anybar
	setupepoll();
}

void
setupepoll(void)
{
	epoll_fd = epoll_create1(0);
	dpy_fd = ConnectionNumber(dpy);
	struct epoll_event dpy_event;

	// Initialize struct to 0
	memset(&dpy_event, 0, sizeof(dpy_event));

	DEBUG("Display socket is fd %d\n", dpy_fd);

	if (epoll_fd == -1) {
		fputs("Failed to create epoll file descriptor", stderr);
	}

	dpy_event.events = EPOLLIN;
	dpy_event.data.fd = dpy_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, dpy_fd, &dpy_event)) {
		fputs("Failed to add display file descriptor to epoll", stderr);
		close(epoll_fd);
		exit(1);
	}

	if (ipc_init(ipcsockpath, epoll_fd, ipccommands, LENGTH(ipccommands)) < 0) {
		fputs("Failed to initialize IPC\n", stderr);
	}
}


void
setviewport(void){
	long data[] = { 0, 0 };
	XChangeProperty(dpy, root, netatom[NetDesktopViewport], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 2);
}


void
spawnbar()
{
	if (*altbarcmd)
		system(altbarcmd);
}

void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

// ORIGINAL
/* void */
/* showhide(Client *c) */
/* { */
/* 	if (!c) */
/* 		return; */
/* 	if (ISVISIBLE(c)) { */
/* 		/\* show clients top down *\/ */
/* 		XMoveWindow(dpy, c->win, c->x, c->y); */
/* 		if (!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) */
/* 			resize(c, c->x, c->y, c->w, c->h, 0); */
/* 		showhide(c->snext); */
/* 	} else { */
/* 		/\* hide clients bottom up *\/ */
/* 		showhide(c->snext); */
/* 		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y); */
/* 	} */
/* } */


int totalWidthOfClientsOnTag(unsigned int tagmask) {
    int totalWidth = 0;
    Client *c;
    for (c = selmon->clients; c; c = c->next)
        if (c->tags & tagmask)
            totalWidth += WIDTH(c) + 10;  // Added 5 pixel buffer
    return totalWidth;
}


// BASE
/* void showhide(Client *c) { */
/*     if (!c) */
/*         return; */

/*     /\* if (c->scratchkey) *\/ */
/*     /\*     return; *\/ */

/*     unsigned int currentTagMask = 1 << previousTag; */
/*     unsigned int targetTagMask = selmon->tagset[selmon->seltags]; */

/*     int currentSelectedTag = 0; */
/*     for (int i = 0; i < LENGTH(tags); i++) { */
/*         if (targetTagMask & (1 << i)) { */
/*             currentSelectedTag = i; */
/*             break; */
/*         } */
/*     } */

/*     int animationDirection = (currentSelectedTag > previousTag) ? 1 : -1; */

/*     int animationDistance = selmon->ww;  // Use screen width for animation */

/*     if (c->istruefullscreen) { */
/*         if (c->tags & targetTagMask) { */
/*             XMoveWindow(dpy, c->win, c->x, c->y + animationDirection * selmon->mh); */
/*             showhide(c->snext); */
/*             XMoveWindow(dpy, c->win, c->x, c->y); */
/*         } else if (c->tags & currentTagMask) { */
/*             XMoveWindow(dpy, c->win, c->x, c->y - animationDirection * selmon->mh); */
/*         } */
/*     } else if (c->isfloating) { */
/*         if (c->tags & targetTagMask) { */
/*             XMoveWindow(dpy, c->win, c->x, selmon->mh + 10); */
/*             showhide(c->snext); */
/*             XMoveWindow(dpy, c->win, c->x, c->y); */
/*         } else if (c->tags & currentTagMask) { */
/*             XMoveWindow(dpy, c->win, c->x, selmon->mh + 10); */
/*         } */
/*     } else if (c->tags & targetTagMask) { */
/*         if (WIDTH(c) < selmon->ww/4) { */
/*             XMoveWindow(dpy, c->win, c->x + animationDirection * (selmon->ww - 10), c->y); */
/*             showhide(c->snext); */
/*             XMoveWindow(dpy, c->win, c->x, c->y); */
/*         } else { */
/*             XMoveWindow(dpy, c->win, c->x + animationDirection * animationDistance, c->y); */
/*             showhide(c->snext); */
/*             XMoveWindow(dpy, c->win, c->x, c->y); */
/*         } */
/*     } else if (c->tags & currentTagMask) { */
/*         XMoveWindow(dpy, c->win, c->x - animationDirection * animationDistance, c->y); */
/*     } else { */
/*         XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y); */
/*     } */

/*     showhide(c->snext); */
/* } */

// best
/* void showhide(Client *c) { */
/*     if (!c) */
/*         return; */

/*     // Skip only the dragged client and proceed to its siblings */
/*     if (c != dragging) { */
/*         unsigned int currentTagMask = 1 << previousTag; */
/*         unsigned int targetTagMask = selmon->tagset[selmon->seltags]; */

/*         int currentSelectedTag = 0; */
/*         for (int i = 0; i < LENGTH(tags); i++) { */
/*             if (targetTagMask & (1 << i)) { */
/*                 currentSelectedTag = i; */
/*                 break; */
/*             } */
/*         } */

/*         int animationDirection = (currentSelectedTag > previousTag) ? 1 : -1; */

/*         int animationDistance = selmon->ww;  // Use screen width for animation */

/*         if (c->istruefullscreen) { */
/*             if (c->tags & targetTagMask) { */
/*                 XMoveWindow(dpy, c->win, c->x, c->y + animationDirection * selmon->mh); */
/*                 XMoveWindow(dpy, c->win, c->x, c->y); */
/*             } else if (c->tags & currentTagMask) { */
/*                 XMoveWindow(dpy, c->win, c->x, c->y - animationDirection * selmon->mh); */
/*             } */
/*         } else if (c->isfloating) { */
/*             if (c->tags & targetTagMask) { */
/*                 XMoveWindow(dpy, c->win, c->x, selmon->mh + 10); */
/*                 XMoveWindow(dpy, c->win, c->x, c->y); */
/*             } else if (c->tags & currentTagMask) { */
/*                 XMoveWindow(dpy, c->win, c->x, selmon->mh + 10); */
/*             } */
/*         } else if (c->tags & targetTagMask) { */
/*             if (WIDTH(c) < selmon->ww/4) { */
/*                 XMoveWindow(dpy, c->win, c->x + animationDirection * (selmon->ww - 10), c->y); */
/*                 XMoveWindow(dpy, c->win, c->x, c->y); */
/*             } else { */
/*                 XMoveWindow(dpy, c->win, c->x + animationDirection * animationDistance, c->y); */
/*                 XMoveWindow(dpy, c->win, c->x, c->y); */
/*             } */
/*         } else if (c->tags & currentTagMask) { */
/*             XMoveWindow(dpy, c->win, c->x - animationDirection * animationDistance, c->y); */
/*         } else { */
/*             XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y); */
/*         } */
/*     } */

/*     showhide(c->snext); */
/* } */


void showhide(Client *c) {
    if (!c)
        return;

    // Skip only the dragged client and proceed to its siblings
    if (c != dragging) {
        unsigned int currentTagMask = 1 << previousTag;
        unsigned int targetTagMask = selmon->tagset[selmon->seltags];

        int currentSelectedTag = 0;
        for (int i = 0; i < LENGTH(tags); i++) {
            if (targetTagMask & (1 << i)) {
                currentSelectedTag = i;
                break;
            }
        }

        int animationDirection = (currentSelectedTag > previousTag) ? 1 : -1;

        int animationDistance = selmon->ww;  // Use screen width for animation

        if (c->istruefullscreen) {
            if (c->tags & targetTagMask) {
                XMoveWindow(dpy, c->win, c->x, c->y + animationDirection * selmon->mh);
                XMoveWindow(dpy, c->win, c->x, c->y);
            } else if (c->tags & currentTagMask) {
                XMoveWindow(dpy, c->win, c->x, c->y - animationDirection * selmon->mh);
            }
        } else if (c->isfloating) {
            // Check if the window width is 100% of the screen width
            if (c->w == selmon->ww) {
                if (c->tags & targetTagMask) {
                    XMoveWindow(dpy, c->win, c->x + animationDirection * animationDistance, c->y);
                    XMoveWindow(dpy, c->win, c->x, c->y);
                } else if (c->tags & currentTagMask) {
                    XMoveWindow(dpy, c->win, c->x - animationDirection * animationDistance, c->y);
                }
            } else {
                if (c->tags & targetTagMask) {
                    XMoveWindow(dpy, c->win, c->x, selmon->mh + 10);
                    XMoveWindow(dpy, c->win, c->x, c->y);
                } else if (c->tags & currentTagMask) {
                    XMoveWindow(dpy, c->win, c->x, selmon->mh + 10);
                }
            }
        } else if (c->tags & targetTagMask) {
            if (WIDTH(c) < selmon->ww/4) {
                XMoveWindow(dpy, c->win, c->x + animationDirection * (selmon->ww - 10), c->y);
                XMoveWindow(dpy, c->win, c->x, c->y);
            } else {
                XMoveWindow(dpy, c->win, c->x + animationDirection * animationDistance, c->y);
                XMoveWindow(dpy, c->win, c->x, c->y);
            }
        } else if (c->tags & currentTagMask) {
            XMoveWindow(dpy, c->win, c->x - animationDirection * animationDistance, c->y);
        } else {
            XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
        }
    }

    showhide(c->snext);
}



void
sigchld(int unused)
{
	pid_t pid;

	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < (pid = waitpid(-1, NULL, WNOHANG))) {
		pid_t *p, *lim;

		if (!(p = autostart_pids))
			continue;
		lim = &p[autostart_len];

		for (; p < lim; p++) {
			if (*p == pid) {
				*p = -1;
				break;
			}
		}

	}
}

void
spawn(const Arg *arg)
{
	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void spawnscratch(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[1], ((char **)arg->v)+1);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[1]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

// ORIGINAL
/* void */
/* tag(const Arg *arg) */
/* { */
/* 	if (selmon->sel && arg->ui & TAGMASK) { */
/* 		selmon->sel->tags = arg->ui & TAGMASK; */
/* 		focus(NULL); */
/* 		arrange(selmon); */
/* 	} */
/* } */


/* When moving a window to a lower numbered tag: The window should go into the stack (not the master area). */
/* When moving a window to a higher numbered tag: The window should go into the master area. */
// SOMETIMES IT DOESNT WORK
void tag(const Arg *arg) {
    if (!selmon->sel || !(arg->ui & TAGMASK)) return;

    // Get the new tag index
    unsigned int newTagIndex = 0;
    for (newTagIndex = 0; newTagIndex < LENGTH(tags); newTagIndex++) {
        if (arg->ui & (1 << newTagIndex))
            break;
    }

    // Get direction (-1 for backward, 1 for forward)
    int direction = 0;
    if (newTagIndex < (previousTag - 1)) direction = -1;
    else if (newTagIndex > (previousTag - 1)) direction = 1;

    // If moving backward, place the window in the stack
    if (direction == -1) {
        Client *c = selmon->sel;
        detach(c);  // Remove client from current client list

        Client *last = NULL;
        // If there's a client in the master area, make 'last' point to it
        for (int n = 0, m = selmon->nmaster; n < m && (m > 0); n++) {
            if (last && last->next) last = last->next;
            else last = selmon->clients;
        }

        // If there is no client in the master area, traverse to the end of the list
        if (!last) {
            for (last = selmon->clients; last && last->next; last = last->next);
            c->next = NULL;
        } else {
            c->next = last->next;
            last->next = c;
        }
    }

    // Attach the window to the new tag
    selmon->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selmon);

    // Update the previousTag
    previousTag = newTagIndex + 1;
}

// PERSONAL FUNCTION
// DO NOT WORK
void rearrangewindows(unsigned int targetTagIndex, int direction) {
    // Temporarily switch to the target tag (for list manipulation only)
    unsigned int tempTagSet = selmon->tagset[selmon->seltags];
    selmon->tagset[selmon->seltags] = 1 << targetTagIndex;

    if (direction == -1) { // moving to a lower numbered tag
        Client *c = selmon->sel;
        detach(c);

        Client *last = NULL;
        for (int n = 0, m = selmon->nmaster; n < m && (m > 0); n++) {
            if (last && last->next) last = last->next;
            else last = selmon->clients;
        }

        if (!last) {
            for (last = selmon->clients; last && last->next; last = last->next);
            c->next = NULL;
        } else {
            c->next = last->next;
            last->next = c;
        }
    }
    // Here we may define the logic for moving to a higher numbered tag

    // Restore the original tagset without calling arrange
    selmon->tagset[selmon->seltags] = tempTagSet;
}




void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? m->ww * m->mfact : 0;
	else
		mw = m->ww - m->gappx;
	for (i = 0, my = ty = m->gappx, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			h = (m->wh - my) / (MIN(n, m->nmaster) - i) - m->gappx;
			resize(c, m->wx + m->gappx, m->wy + my, mw - (2*c->bw) - m->gappx, h - (2*c->bw), 0);
			my += HEIGHT(c) + m->gappx;
		} else {
			h = (m->wh - ty) / (n - i) - m->gappx;
			resize(c, m->wx + mw + m->gappx, m->wy + ty, m->ww - mw - (2*c->bw) - 2*m->gappx, h - (2*c->bw), 0);
			ty += HEIGHT(c) + m->gappx;
		}
}

void
masterstack(Monitor *m)
{
    tile(m);
}


void
togglebar(const Arg *arg)
{
	selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, selmon->bh);
	arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
			selmon->sel->w, selmon->sel->h, 0);
	else
		selmon->sel->isalwaysontop = 0; /* disabled, turn this off too */
	arrange(selmon);
}

void
togglealwaysontop(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen)
		return;

	if(selmon->sel->isalwaysontop){
		selmon->sel->isalwaysontop = 0;
	}else{
		/* disable others */
		for(Monitor *m = mons; m; m = m->next)
			for(Client *c = m->clients; c; c = c->next)
				c->isalwaysontop = 0;

		/* turn on, make it float too */
		selmon->sel->isfloating = 1;
		selmon->sel->isalwaysontop = 1;
	}

	arrange(selmon);
}

// ORIGINAL
/* void */
/* togglescratch(const Arg *arg) */
/* { */
/* 	Client *c; */
/* 	unsigned int found = 0; */

/* 	for (c = selmon->clients; c && !(found = c->scratchkey == ((char**)arg->v)[0][0]); c = c->next); */
/* 	if (found) { */
/* 		c->tags = ISVISIBLE(c) ? 0 : selmon->tagset[selmon->seltags]; */
/* 		focus(NULL); */
/* 		arrange(selmon); */

/* 		if (ISVISIBLE(c)) { */
/* 			focus(c); */
/* 			restack(selmon); */
/* 		} */

/* 	} else{ */
/* 		spawnscratch(arg); */
/* 	} */
/* } */

/* void */
/* togglescratch(const Arg *arg) */
/* { */
/*     Client *c; */
/*     unsigned int found = 0; */

/*     for (c = selmon->clients; c && !(found = c->scratchkey == ((char**)arg->v)[0][0]); c = c->next); */
/*     if (found) { */
/*         if (ISVISIBLE(c)) { */
/*             // Hide the scratchpad: move it down */
/*             XMoveWindow(dpy, c->win, c->x, selmon->mh); // Assuming 'selmon->mh' is the monitor's height */
/*             c->tags = 0; */
/*         } else { */
/*             // Show the scratchpad: move it to the original position */
/*             XMoveWindow(dpy, c->win, c->x, c->y - c->h); // Move the window to its original 'y' but ensure it's visible */
/*             c->tags = selmon->tagset[selmon->seltags]; */
/*             focus(c); */
/*             restack(selmon); */
/*         } */
/*     } else { */
/*         spawnscratch(arg); */
/*     } */
/* } */

void
togglescratch(const Arg *arg)
{
    Client *c;
    unsigned int found = 0;

    for (c = selmon->clients; c && !(found = c->scratchkey == ((char**)arg->v)[0][0]); c = c->next);
    if (found) {
        if (ISVISIBLE(c)) { // If visible, hide it
            // Save original position
            c->oldx = c->x;
            c->oldy = c->y;
            // Move the client window below the screen
            XMoveWindow(dpy, c->win, c->x, selmon->mh + 10); // This is the part inspired from your function
            // Hide the client by removing its tags
            c->tags = 0;
        } else { // If hidden, show it
            // Move it back to the original position
            XMoveWindow(dpy, c->win, c->oldx, c->oldy);
            // Apply tags so the client is visible
            c->tags = selmon->tagset[selmon->seltags];
        }

        focus(NULL);
        arrange(selmon);

        if (ISVISIBLE(c)) {
            focus(c);
            restack(selmon);
        }
    } else {
        spawnscratch(arg);
    }
}


void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
	updatecurrentdesktop();
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
	int i;

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;

		if (newtagset == ~0) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			selmon->pertag->curtag = 0;
		}

		/* test if the user did not select the same tag */
		if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			for (i = 0; !(newtagset & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}

		/* apply settings for this view */
		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
		selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
		selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

		if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
			togglebar(NULL);

		focus(NULL);
		arrange(selmon);
	}
	updatecurrentdesktop();
}

void
freeicon(Client *c)
{
	if (c->icon) {
		XRenderFreePicture(dpy, c->icon);
		c->icon = None;
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	if (c->swallowing) {
		unswallow(c);
		return;
	}

	Client *s = swallowingclient(c->win);
	if (s) {
		free(s->swallowing);
		s->swallowing = NULL;
		arrange(m);
		focus(NULL);
		return;
	}

	detach(c);
	detachstack(c);
	freeicon(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);

	if (!s) {
		arrange(m);
		focus(NULL);
		updateclientlist();
	}
}


// anybar
void
unmanagealtbar(Window w)
{
    Monitor *m = wintomon(w);

    if (!m)
        return;

    m->barwin = 0;
    m->by = 0;
    m->bh = 0;
    updatebarpos(m);
    arrange(m);
}


/* void */
/* unmapnotify(XEvent *e) */
/* { */
/* 	Client *c; */
/* 	XUnmapEvent *ev = &e->xunmap; */

/* 	if ((c = wintoclient(ev->window))) { */
/* 		if (ev->send_event) */
/* 			setclientstate(c, WithdrawnState); */
/* 		else */
/* 			unmanage(c, 0); */
/* 	} */
/* } */

void unmapnotify(XEvent *e) {
    Client *c;
    Monitor *m; // anybar
    XUnmapEvent *ev = &e->xunmap;

    if ((c = wintoclient(ev->window))) {
        if (ev->send_event)
            setclientstate(c, WithdrawnState);
        else
            unmanage(c, 0);
	} else if ((m = wintomon(ev->window)) && m->barwin == ev->window)
		unmanagealtbar(ev->window);


    writewindowcount();  // Update the window count after unmanaging a window
}

void
updatebars(void)
{
	if (usealtbar)
		return;

	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
				CopyFromParent, DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= m->bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + m->bh : m->wy;
	} else
		m->by = -m->bh;
}

void
updateclientlist()
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *) &(c->win), 1);
}
void updatecurrentdesktop(void){
	long rawdata[] = { selmon->tagset[selmon->seltags] };
	int i=0;
	/* while(*rawdata >> i+1){ */
	while(*rawdata >> (i+1)){
		i++;
	}
	long data[] = { i };
	XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) { /* new monitors available */
			for (i = 0; i < (nn - n); i++) {
				for (m = mons; m && m->next; m = m->next);
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n
				|| unique[i].x_org != m->mx || unique[i].y_org != m->my
				|| unique[i].width != m->mw || unique[i].height != m->mh)
				{
					dirty = 1;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = 1;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					attach(c);
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}
// ORIGINAL
/* void */
/* updatestatus(void) */
/* { */
/* 	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext))) */
/* 		strcpy(stext, "dwm-"VERSION); */
/* 	drawbar(selmon); */
/* } */

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "                                            ");  // Sace to add modules
	drawbar(selmon);
}


void
updatetitle(Client *c)
{
	char oldname[sizeof(c->name)];
	strcpy(oldname, c->name);

	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);

	for (Monitor *m = mons; m; m = m->next) {
		if (m->sel == c && strcmp(oldname, c->name) != 0)
			ipc_focused_title_change_event(m->num, c->win, oldname, c->name);
	}
}

void
updateicon(Client *c)
{
	freeicon(c);
	c->icon = geticonprop(c->win, &c->icw, &c->ich);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}
// ORIGINAL
/* void */
/* view(const Arg *arg) */
/* { */
/* 	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags]) */
/* 		return; */
/* 	selmon->seltags ^= 1; /\* toggle sel tagset *\/ */
/* 	if (arg->ui & TAGMASK) */
/* 		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK; */
/* 	focus(NULL); */
/* 	arrange(selmon); */
/* 	updatecurrentdesktop(); */
/* } */

// OLD METHOD
/* void view(const Arg *arg) { */
/*     if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags]) */
/*         return; */

/*     int previousTagIndex = selmon->seltags; */
/*     selmon->seltags ^= 1;  /\* toggle sel tagset *\/ */

/*     if (arg->ui & TAGMASK) */
/*         selmon->tagset[selmon->seltags] = arg->ui & TAGMASK; */

/*     // Correct the movement direction based on tags */
/*     if (selmon->seltags < previousTagIndex) */
/*         movement_direction = 1;  // Moving to a higher tag */
/*     else if (selmon->seltags > previousTagIndex) */
/*         movement_direction = -1; // Moving to a lower tag */

/*     focus(NULL); */
/*     arrange(selmon); */
/* } */

//ALWAYS CORRECT POSITION
/* void view(const Arg *arg) { */
/*     if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags]) */
/*         return; */

/*     // Determine the active tag index */
/*     int currentTagMask = selmon->tagset[selmon->seltags]; */
/*     int currentTagIndex; */
/*     for (currentTagIndex = 0; currentTagIndex < LENGTH(tags); currentTagIndex++) { */
/*         if (currentTagMask & (1 << currentTagIndex)) */
/*             break; */
/*     } */

/*     // Update the previousTag variable */
/*     previousTag = currentTagIndex; */

/*     selmon->seltags ^= 1;  /\* toggle sel tagset *\/ */

/*     if (arg->ui & TAGMASK) */
/*         selmon->tagset[selmon->seltags] = arg->ui & TAGMASK; */

/*     // Determine the new tag index */
/*     int newTagIndex; */
/*     for (newTagIndex = 0; newTagIndex < LENGTH(tags); newTagIndex++) { */
/*         if (arg->ui & (1 << newTagIndex)) */
/*             break; */
/*     } */

/*     // Correct the movement direction based on tags */
/*     if (newTagIndex < previousTag) */
/*         movement_direction = 1;  // Moving to a higher tag */
/*     else if (newTagIndex > previousTag) */
/*         movement_direction = -1; // Moving to a lower tag */

/*     focus(NULL); */
/*     arrange(selmon); */
/* } */



// ALWAYS CORRECT POSITION + PERTAG
/* void view(const Arg *arg) { */
/*     if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags]) */
/*         return; */

/*     // Determine the active tag index */
/*     int currentTagMask = selmon->tagset[selmon->seltags]; */
/*     int currentTagIndex; */
/*     for (currentTagIndex = 0; currentTagIndex < LENGTH(tags); currentTagIndex++) { */
/*         if (currentTagMask & (1 << currentTagIndex)) */
/*             break; */
/*     } */

/*     // Pertag logic */
/*     int i; */
/*     unsigned int tmptag; */

/*     selmon->seltags ^= 1;  /\* toggle sel tagset *\/ */

/*     if (arg->ui & TAGMASK) { */
/*         selmon->tagset[selmon->seltags] = arg->ui & TAGMASK; */
/*         selmon->pertag->prevtag = selmon->pertag->curtag; */

/*         if (arg->ui == ~0) */
/*             selmon->pertag->curtag = 0; */
/*         else { */
/*             for (i = 0; !(arg->ui & 1 << i); i++) ; */
/*             selmon->pertag->curtag = i + 1; */
/*         } */
/*     } else { */
/*         tmptag = selmon->pertag->prevtag; */
/*         selmon->pertag->prevtag = selmon->pertag->curtag; */
/*         selmon->pertag->curtag = tmptag; */
/*     } */

/*     selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag]; */
/*     selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag]; */
/*     selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag]; */
/*     selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt]; */
/*     selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1]; */

/*     if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag]) */
/*         togglebar(NULL); */

/*     // Update the previousTag variable */
/*     previousTag = currentTagIndex; */

/*     // Determine the new tag index */
/*     int newTagIndex; */
/*     for (newTagIndex = 0; newTagIndex < LENGTH(tags); newTagIndex++) { */
/*         if (arg->ui & (1 << newTagIndex)) */
/*             break; */
/*     } */

/*     // Correct the movement direction based on tags */
/*     if (newTagIndex < previousTag) */
/*         movement_direction = 1;  // Moving to a higher tag */
/*     else if (newTagIndex > previousTag) */
/*         movement_direction = -1; // Moving to a lower tag */

/*     focus(NULL); */
/*     arrange(selmon); */
/* } */


// ALWAYS CORRECT POSITION + PERTAG + WINDOW COUNT
void view(const Arg *arg) {
    if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
        return;

    // Determine the active tag index
    int currentTagMask = selmon->tagset[selmon->seltags];
    int currentTagIndex;
    for (currentTagIndex = 0; currentTagIndex < LENGTH(tags); currentTagIndex++) {
        if (currentTagMask & (1 << currentTagIndex))
            break;
    }

    // Pertag logic
    int i;
    unsigned int tmptag;

    selmon->seltags ^= 1;  /* toggle sel tagset */

    if (arg->ui & TAGMASK) {
        selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
        selmon->pertag->prevtag = selmon->pertag->curtag;

        if (arg->ui == ~0)
            selmon->pertag->curtag = 0;
        else {
            for (i = 0; !(arg->ui & 1 << i); i++) ;
            selmon->pertag->curtag = i + 1;
        }
    } else {
        tmptag = selmon->pertag->prevtag;
        selmon->pertag->prevtag = selmon->pertag->curtag;
        selmon->pertag->curtag = tmptag;
    }

    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
    selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
    selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

    if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
        togglebar(NULL);

    // Update the previousTag variable
    previousTag = currentTagIndex;

    // Determine the new tag index
    int newTagIndex;
    for (newTagIndex = 0; newTagIndex < LENGTH(tags); newTagIndex++) {
        if (arg->ui & (1 << newTagIndex))
            break;
    }

    // Correct the movement direction based on tags
    if (newTagIndex < previousTag)
        movement_direction = 1;  // Moving to a higher tag
    else if (newTagIndex > previousTag)
        movement_direction = -1; // Moving to a lower tag

    focus(NULL);

    writewindowcount();  // Update the window count after switching tags

    arrange(selmon);
}





pid_t
winpid(Window w)
{

	pid_t result = 0;

#ifdef __linux__
	xcb_res_client_id_spec_t spec = {0};
	spec.client = w;
	spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

	xcb_generic_error_t *e = NULL;
	xcb_res_query_client_ids_cookie_t c = xcb_res_query_client_ids(xcon, 1, &spec);
	xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(xcon, c, &e);

	if (!r)
		return (pid_t)0;

	xcb_res_client_id_value_iterator_t i = xcb_res_query_client_ids_ids_iterator(r);
	for (; i.rem; xcb_res_client_id_value_next(&i)) {
		spec = i.data->spec;
		if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
			uint32_t *t = xcb_res_client_id_value_value(i.data);
			result = *t;
			break;
		}
	}

	free(r);

	if (result == (pid_t)-1)
		result = 0;

#endif /* __linux__ */

#ifdef __OpenBSD__
        Atom type;
        int format;
        unsigned long len, bytes;
        unsigned char *prop;
        pid_t ret;

        if (XGetWindowProperty(dpy, w, XInternAtom(dpy, "_NET_WM_PID", 0), 0, 1, False, AnyPropertyType, &type, &format, &len, &bytes, &prop) != Success || !prop)
               return 0;

        ret = *(pid_t*)prop;
        XFree(prop);
        result = ret;

#endif /* __OpenBSD__ */
	return result;
}

pid_t
getparentprocess(pid_t p)
{
	unsigned int v = 0;

#ifdef __linux__
	FILE *f;
	char buf[256];
	snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

	if (!(f = fopen(buf, "r")))
		return 0;

	fscanf(f, "%*u %*s %*c %u", &v);
	fclose(f);
#endif /* __linux__*/

#ifdef __OpenBSD__
	int n;
	kvm_t *kd;
	struct kinfo_proc *kp;

	kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, NULL);
	if (!kd)
		return 0;

	kp = kvm_getprocs(kd, KERN_PROC_PID, p, sizeof(*kp), &n);
	v = kp->p_ppid;
#endif /* __OpenBSD__ */

	return (pid_t)v;
}

int
isdescprocess(pid_t p, pid_t c)
{
	while (p != c && c != 0)
		c = getparentprocess(c);

	return (int)c;
}

Client *
termforwin(const Client *w)
{
	Client *c;
	Monitor *m;

	if (!w->pid || w->isterminal)
		return NULL;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->isterminal && !c->swallowing && c->pid && isdescprocess(c->pid, w->pid))
				return c;
		}
	}

	return NULL;
}

Client *
swallowingclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->swallowing && c->swallowing->win == w)
				return c;
		}
	}

	return NULL;
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

int
wmclasscontains(Window win, const char *class, const char *name)
{
	XClassHint ch = { NULL, NULL };
	int res = 1;

	if (XGetClassHint(dpy, win, &ch)) {
		if (ch.res_name && strstr(ch.res_name, name) == NULL)
			res = 0;
		if (ch.res_class && strstr(ch.res_class, class) == NULL)
			res = 0;
	} else
		res = 0;

	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);

	return res;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
xrdb(const Arg *arg)
{
  loadxrdb();
  int i;
  for (i = 0; i < LENGTH(colors); i++)
                scheme[i] = drw_scm_create(drw, colors[i], 3);
  focus(NULL);
  arrange(NULL);
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!selmon->lt[selmon->sellt]->arrange
	|| (selmon->sel && selmon->sel->isfloating))
		return;
	if (c == nexttiled(selmon->clients))
		if (!c || !(c = nexttiled(c->next)))
			return;
	pop(c);
}

/* int */
/* main(int argc, char *argv[]) */
/* { */
/* 	if (argc == 2 && !strcmp("-v", argv[1])) */
/* 		die("dwm-"VERSION); */
/* 	else if (argc != 1) */
/* 		die("usage: dwm [-v]"); */
/* 	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) */
/* 		fputs("warning: no locale support\n", stderr); */
/* 	if (!(dpy = XOpenDisplay(NULL))) */
/* 		die("dwm: cannot open display"); */
/* 	if (!(xcon = XGetXCBConnection(dpy))) */
/* 		die("dwm: cannot get xcb connection\n"); */
/* 	checkotherwm(); */
/*         autostart_exec();  // Inserting the autostart function here */
/*         XrmInitialize(); */
/*         loadxrdb(); */
/* 	setup(); */
/* #ifdef __OpenBSD__ */
/* 	if (pledge("stdio rpath proc exec ps", NULL) == -1) */
/* 		die("pledge"); */
/* #endif /\* __OpenBSD__ *\/ */
/* 	scan(); */
/* 	run(); */
/* 	cleanup(); */
/* 	XCloseDisplay(dpy); */
/* 	return EXIT_SUCCESS; */
/* } */


int main(int argc, char *argv[]) {
    if (argc == 2 && !strcmp("-v", argv[1]))
        die("dwm-"VERSION);
    else if (argc != 1)
        die("usage: dwm [-v]");
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
        fputs("warning: no locale support\n", stderr);
    if (!(dpy = XOpenDisplay(NULL)))
        die("dwm: cannot open display");
    if (!(xcon = XGetXCBConnection(dpy)))
        die("dwm: cannot get xcb connection\n");
    checkotherwm();
    autostart_exec();  // Inserting the autostart function here
    XrmInitialize();
    loadxrdb();
    setup();
#ifdef __OpenBSD__
    if (pledge("stdio rpath proc exec ps", NULL) == -1)
        die("pledge");
#endif /* __OpenBSD__ */
    scan();
    run();
    writewindowcount();  // Call the function here to update the window count
    cleanup();
    XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}
