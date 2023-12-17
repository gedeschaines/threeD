/**********************************************************************/
/* FILE:  threeD.c
 * DATE:  24 AUG 2000
 * AUTH:  G. E. Deschaines
 * DESC:  Three dimensional drawing of objects defined as collections
 *        of polygons.
*/
/**********************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifdef __linux__
#include <bits/time.h>
#endif
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xw32defs.h>
#ifdef __linux__
#include <X11/xpm.h>
#endif
#ifdef __CYGWIN__
#include <X11/Xpm.h>
#endif
#include <X11/keysym.h>
#include <Xm/Xm.h>
#include <Xm/DrawingA.h>

static GC         the_GC;
static XGCValues  the_GCv;
static int        img_FPS = 50;
static Pixel      pixels[8];
static Boolean    quitflag = FALSE;


#include "draw3D.c"

void do_draw3D(w, client_data, event)
   Widget   w;
   caddr_t  client_data;
   XEvent*  event;
{

/* Create the Graphics Context used for drawing.
*/
   the_GC = XCreateGC(XtDisplay(w), XtWindow(w), 0, &the_GCv);

/* Three D drawing.
*/
   draw3D(w, XtDisplay(w), XtWindow(w));

/* Free the Graphics Context
*/
   XFlush(XtDisplay(w));
   XFreeGC(XtDisplay(w),the_GC);
}

void do_keypress(w, client_data, event)
   Widget      w;
   caddr_t     client_data;
   XKeyEvent*  event;
{
   if ( event->type == KeyPress ) {
      switch ( XkbKeycodeToKeysym(XtDisplay(w), event->keycode, 0, event->state & ShiftMask ? 1 : 0 ) )
      {
      case XK_Escape : /* exit */
         XtAppSetExitFlag(XtWidgetToApplicationContext(w));
         break;
      case XK_r      : /* replay */
         quitflag = FALSE;
         do_draw3D(w, client_data, event);
         break;
      default :
         break;
      }
   }
}

int main(argc, argv)
   int    argc;
   char*  argv[];
{
   XtAppContext  the_app;
   Display*      the_dsp;
   Widget        toplevel, a_widget;
   Arg           wargs[10];
   int           n=0;

/* Initialize the X Toolkit Intrinsics.
*/
   XtToolkitInitialize();

/* Create the application context.
*/
   the_app = XtCreateApplicationContext();

/* Open the display.
*/
   the_dsp = XtOpenDisplay(the_app,
                           "",
                           argv[0], "threeD",
                           NULL,    0,
                           &argc,   argv);

/* Create a toplevel shell.
*/
   toplevel = XtAppCreateShell(argv[0],
                               "threeD",
                               applicationShellWidgetClass,
                               the_dsp,
                               NULL, 0);

/* Create a widget.
*/
   XtSetArg(wargs[n], XtNwidth,  800); n++;  // NOTE: Value for fovs in draw3D.c should match
   XtSetArg(wargs[n], XtNheight, 600); n++;  //       the value of height here.

   a_widget = XtCreateManagedWidget("a_widget",
                                     xmDrawingAreaWidgetClass,
                                     toplevel,
                                     wargs,
                                     n);

/* Register the event handler to be called when a key is pressed.
*/
   XtAddEventHandler(a_widget, KeyPressMask, FALSE, do_keypress, NULL);

/* Register the event handler to be called when a button is pressed.
*/
   XtAddEventHandler(a_widget, ButtonPressMask, FALSE, do_draw3D, NULL);

/* Realize all widgets.
*/
   XtRealizeWidget(toplevel);

   printf("Click mouse button with cursor in threeD window to begin.\n");
   printf("Press T key to toggle field-of-view towards target.\n");
   printf("Press M key to toggle field-of-view towards missile.\n");
   printf("Press H key to toggle field-of-view along missile heading.\n");
   printf("Press Z key to reset zoom to one.\n");
   printf("Press Up Arrow key to increase zoom.\n");
   printf("Press Down Arrow to decrease zoom.\n");
   printf("Press 0 (zero) key to reset animation step delay to zero.\n");
   printf("Press Left Arrow key to slow animation down by 50 msec increments.\n");
   printf("Press Right Arrow key to speed animation up by 50 msec increments.\n");
   printf("Press P key to toggle pause/unpause.\n");
// printf("Press C key to continue.\n");
   printf("Press Q key to quit animation.\n");
   printf("Press R key to replay animation.\n"); 
   printf("Press Esc key to exit.\n");

/* Enter the event loop.
*/
   XtAppMainLoop(the_app);

   return 0;
}
/**********************************************************************/
/**********************************************************************/
