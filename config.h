/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 3;        /* border pixel of windows */
static const unsigned int gappx     = 5;        /* gaps between windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const int swallowfloating    = 0;        /* 1 means swallow floating windows by default */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char *fonts[]          = { "JetBrains Mono:size=11", "JoyPixels:pixelsize=11:antialias=true:autohint=true"};
static const char dmenufont[]       = "JetBrains Mono:size=11";
static char normbgcolor[]           = "#222222";
static char normbordercolor[]       = "#444444";
static char normfgcolor[]           = "#bbbbbb";
static char selfgcolor[]            = "#eeeeee";
static char selbordercolor[]        = "#005577";
static char selbgcolor[]            = "#005577";
static char *colors[][3] = {
       /*               fg           bg           border   */
       [SchemeNorm] = { normfgcolor, normbgcolor, normbordercolor },
       [SchemeSel]  = { selfgcolor,  selbgcolor,  selbordercolor  },
};


static const char *autostart[][4] = {
    { "unclutter", NULL, NULL, NULL },
    { "sh", "-c", "xset r rate 160 60", NULL },
    { "sh", "-c", "xrandr --output \"$(xrandr | awk '/ connected/ {print $1; exit}')\" --mode 1920x1080 --rate 144", NULL },
    { "wal", "-R", "-q", NULL },
    { "picom", NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL } /* terminate */
};


/*First arg only serves to match against key in rules*/
static const char *scratchpadcmd[] = {"s", "st", "-t", "scratchpad", NULL};

/* tagging */
static const char *tags[] = { "", "", "", "", "       ", "", "", "", "" }; /**/

// ORIGINAL
/* static const Rule rules[] = { */
/* 	/\* xprop(1): */
/* 	 *	WM_CLASS(STRING) = instance, class */
/* 	 *	WM_NAME(STRING) = title */
/* 	 *\/ */
/* 	/\* class                instance  title           tags mask  isfloating  isterminal  noswallow  monitor *\/ */
/* 	/\* { "TelegramDesktop",    NULL,     NULL,           0,         1,          0,           0,        -1 }, *\/ */
/* 	{ "obs",                NULL,     NULL,           0,         1,          0,           0,        -1 }, */
/* 	{ "Lutris",             NULL,     NULL,           0,         1,          0,           0,        -1 }, */
/* 	{ "firefox",   	    	NULL,     NULL,           1 << 2,    0,          0,          -1,        -1 }, */
/* 	{ "St",                 NULL,     NULL,           0,         0,          1,           0,        -1 }, */
/* 	{ NULL,                 NULL,     "Event Tester", 0,         0,          0,           1,        -1 }, /\* xev *\/ */
/* }; */

// SCRATCHPAD PATCH
static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class            instance  title           tags mask  isfloating  isterminal  noswallow  monitor   scratch key */
	{ "obs",            NULL,     NULL,           0,         1,          0,          0,         -1,        0  },
	{ "Lutris",         NULL,     NULL,           0,         1,          0,          0,         -1,        0  },
	{ "firefox",        NULL,     NULL,           1 << 2,    0,          0,          -1,        -1,        0  },
	{ "discord",        NULL,     NULL,           1 << 3,    0,          0,          -1,        -1,        0  },
	{ "emacs",          NULL,     NULL,           1 << 0,    0,          0,          -1,        -1,        0  },
	{ "mpv",            NULL,     NULL,           1 << 5,    0,          0,          -1,        -1,        0  },
	{ "St",             NULL,     NULL,           0,         0,          1,          0,         -1,        0  },
	{ NULL,             NULL,     "Event Tester", 0,         0,          0,          1,         -1,        0  }, /* xev */
	{ "Gimp",           NULL,     NULL,           0,         1,          0,          0,         -1,        0  },
	{ NULL,             NULL,     "scratchpad",   0,         1,          0,          0,         -1,       's' },
};



/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 0; /* 1 will force focus on the fullscreen window */



// Funtions
/* void */
/* truefullscreen(Monitor *m) { */
/*     Client *c; */

/*     for (c = m->clients; c; c = c->next) { */
/*         if (ISVISIBLE(c)) { */
/*             // Set the window to floating mode */
/*             c->isfloating = 1; */

/*             // Move and resize the window to cover the entire screen */
/*             XMoveResizeWindow(dpy, c->win, m->mx, m->my, m->mw, m->mh); */

/*             // Hide borders */
/*             c->bw = 0; */
/*             XConfigureWindow(dpy, c->win, CWBorderWidth, &(XWindowChanges){.border_width = c->bw}); */
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




static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
	{ "[T]",    truefullscreen },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmrun"};
static const char *termcmd[]  = { "kitty", NULL };
static const char *stcmd[]  = { "st", NULL };
static const char *zoomcmd[]  = { "boomer", NULL };




#include "movestack.c"
static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY|ShiftMask,             XK_j,      movestack,      {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_k,      movestack,      {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_h,      cyclelayout,    {.i = -1 } }, // Go to the previous layout
    { MODKEY|ShiftMask,             XK_l,      cyclelayout,    {.i = +1 } }, // Go to the next layout
    { MODKEY,                       XK_grave,  togglescratch,  {.v = scratchpadcmd } }, // Go to the next layout
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_t,	   togglefloating, {0} },
	{ MODKEY,                  XK_BackSpace,   zoom,           {0} },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_s,      incnmaster,     {.i = +1 } },
	{ MODKEY,		                XK_q,      killclient,     {0} },
	{ MODKEY,                       XK_w,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_e,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_r,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY|ShiftMask,             XK_r,  	   quit,           {1} },
	/* { MODKEY,                       XK_space,  	   setlayout,      {0} }, */
	{ MODKEY,                       XK_space, toggletruefullscreen, {0} },
	{ MODKEY,                       XK_p,  spawn,              {.v = dmenucmd } },
	{ MODKEY,			            XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY|ShiftMask,			XK_Return, spawn,          {.v = stcmd } },
    { MODKEY,                       XK_o,      toggleborder, {0} },
    { MODKEY|ShiftMask,             XK_o,      toggleborder, {.ui = ShiftMask} },
	{ MODKEY,			            XK_z,      spawn,          {.v = zoomcmd } },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	{ MODKEY,			XK_minus,  setgaps,	   {.i = -1 } },
	{ MODKEY,			XK_equal,  setgaps,	   {.i = +1 } },
	{ MODKEY|ShiftMask,		XK_equal,  setgaps,	   {.i =  0 } },
	{ MODKEY,                       XK_F5,     xrdb,           {.v = NULL } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,		XK_q,      quit,           {0} },
};

static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
	{ ClkRootWin,           MODKEY,         Button4,        view,           {0} }, // Added for Scroll Up
	{ ClkRootWin,           MODKEY,         Button5,        view,           {0} }, // Added for Scroll Down
	{ ClkClientWin,         MODKEY,         Button4,        view,           {0} },
	{ ClkClientWin,         MODKEY,         Button5,        view,           {0} },

};
