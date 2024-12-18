/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 0;        /* border pixel of windows */
static const unsigned int gappx     = 5;        /* gaps between windows */
static const unsigned int snap      = 0;       /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */

static const int  usealtbar        = 1;        /* 1 means use non-dwm status bar */
static const char *altbarclass     = "Polybar"; /* Alternate bar class name */
static const char *altbarcmd       = "$HOME/.config/emacs/polybar/launch.sh --cuts"; /* Alternate bar launch command */



static const char *fonts[]          = { "JetBrains Mono:size=11", "JoyPixels:pixelsize=11:antialias=true:autohint=true"};
static const char dmenufont[]       = "JetBrains Mono:size=11";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_cyan,  col_cyan  },
};

#define EDGETHRESHOLD 5
#define MOUSEEDGESWITCH 1  // 1 to enable, 0 to disable
#define DRAGGEDGESWITCH 1  // 1 to enable, 0 to disable

/* static const char *const autostart[] = { */
/*     "sh", "-c", "xrandr --output \"$(xrandr | awk '/ connected/ {print $1; exit}')\" --mode 1920x1080 --rate 144", NULL, */
/*     "feh", "--bg-scale", "/home/l/Desktop/test/oglo/hyprland-rice/themes/ayu_dark/wallpaper.png", NULL, */
/*     "unclutter", NULL, */
/*     "picom", NULL, */
/*     "xset", "r", "rate", "160", "60", NULL, */
/*     NULL /\* terminate *\/ */
/* }; */

static const char *const autostart[] = {
    "sh", "-c", "xrandr --output \"$(xrandr | awk '/ connected/ {print $1; exit}')\" --mode 1920x1080 --rate 144", NULL,
    "feh", "--bg-scale", "/home/l/xos/suckless/dwm/themes/ayu-dark/wallpaper.png", NULL,
    "unclutter", NULL,
    "/home/l/xos/suckless/dwm/themes/aELF/picom/picom", "--config", "/home/l/xos/suckless/dwm/themes/aELF/picom/picom.conf", NULL,
    "xset", "r", "rate", "160", "60", NULL,
    "/home/l/xos/suckless/dwm/themes/aELF/polybar/launch.sh", "--dwm", NULL,
    NULL /* terminate */
};


/* tagging */
static const char *tags[] = { "󰟜", "", "", "", "", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 0; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
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
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "st", NULL };
static const char *boomercmd[]  = { "boomer", NULL };


#include "selfrestart.c"
static const Key keys[] = {
	/* modifier                     key        function        argument */
    { MODKEY,                       XK_Up,     viewup,         {0} },
    { MODKEY,                       XK_Down,   viewdown,       {0} },
    { MODKEY,                       XK_f,      viewnext,       {0} },
    { MODKEY,                       XK_p,      viewup,         {0} },
    { MODKEY,                       XK_n,      viewdown,       {0} },
    { MODKEY,                       XK_b,      viewprev,       {0} },
    { MODKEY|ShiftMask,             XK_f,      tagtonext,      {0} },
    { MODKEY|ShiftMask,             XK_b,      tagtoprev,      {0} },
	{ MODKEY,                       XK_z,      spawn,          {.v = boomercmd } },
	{ MODKEY,                       XK_x,      spawn,          {.v = dmenucmd } },
	{ MODKEY,                       XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_m,      spawn,          {.v = termcmd } },
	{ MODKEY|ControlMask,           XK_b,      togglebar,      {0} },
	{ MODKEY|ShiftMask,             XK_j,      rotatestack,    {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_k,      rotatestack,    {.i = -1 } },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_o,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	/* { MODKEY,                       XK_Return, zoom,           {0} }, */
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY,                       XK_q,      killclient,     {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	/* { MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} }, */
	/* { MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} }, */
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
    { MODKEY,                       XK_Right,  viewnext,       {0} },
    { MODKEY,                       XK_Left,   viewprev,       {0} },
    { MODKEY|ShiftMask,             XK_Right,  tagtonext,      {0} },
    { MODKEY|ShiftMask,             XK_Left,   tagtoprev,      {0} },
	{ MODKEY,                       XK_minus,  smartresizegaps,{.i = +1 } },
	{ MODKEY,                       XK_equal,  smartresizegaps,{.i = -1 } },
	{ MODKEY|ShiftMask,             XK_equal,  setgaps,        {.i = 0  } },
    
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
    { MODKEY|ShiftMask,             XK_r,      self_restart,   {0} },
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
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
};

static const char *ipcsockpath = "/tmp/dwm.sock";
static IPCCommand ipccommands[] = {
    IPCCOMMAND(  view,                1,      {ARG_TYPE_UINT}   ),
    IPCCOMMAND(  toggleview,          1,      {ARG_TYPE_UINT}   ),
    IPCCOMMAND(  tag,                 1,      {ARG_TYPE_UINT}   ),
    IPCCOMMAND(  toggletag,           1,      {ARG_TYPE_UINT}   ),
    IPCCOMMAND(  tagmon,              1,      {ARG_TYPE_UINT}   ),
    IPCCOMMAND(  focusmon,            1,      {ARG_TYPE_SINT}   ),
    IPCCOMMAND(  focusstack,          1,      {ARG_TYPE_SINT}   ),
    IPCCOMMAND(  zoom,                1,      {ARG_TYPE_NONE}   ),
    IPCCOMMAND(  incnmaster,          1,      {ARG_TYPE_SINT}   ),
    IPCCOMMAND(  killclient,          1,      {ARG_TYPE_SINT}   ),
    IPCCOMMAND(  togglefloating,      1,      {ARG_TYPE_NONE}   ),
    IPCCOMMAND(  setmfact,            1,      {ARG_TYPE_FLOAT}  ),
    IPCCOMMAND(  setlayoutsafe,       1,      {ARG_TYPE_PTR}    ),
    IPCCOMMAND(  quit,                1,      {ARG_TYPE_NONE}   )
};

