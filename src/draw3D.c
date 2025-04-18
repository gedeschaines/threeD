/**********************************************************************/
/* FILE:  draw3D.c
 * DATE:  24 AUG 2000
 * AUTH:  G. E. Deschaines
 * DESC:  Routines to load, transform and draw polygon data describing
 *        objects in a 3D world space.
 *
 * NOTE:  Cartesian coordinate frames for world space, field-of-view
 *        (FOV) viewport (size WxH pixels; aspect ratio (AR) of W/H),
 *        viewport clipping frustum (pyramid) and drawable pixmap
 *        are depicted in the following pictograms.
 *
 *                  +X                            +x
 *        [0,0,0]   /                   [0,0,0]   /  [-W*AR/2,+W*AR/2]
 *           |     /                       |     /           |
 *           \--> + ----- +Y               \--> + ----- +y <-/
 *                |                             |
 *                |                             |   [-H/2,+H/2]
 *               +Z                            +z <-----/
 *
 *           World Space                    FOV Viewport
 *
 *
 *        [-H/2,+H/2]
 *           /-> +y  +z                       [0,0]
 *                |  /                          + ----- +x [W]
 *                | /  [-W/AR/2,+W/AR/2]        |
 *                + ----- +x <-/                |
 *             [0,0,0]                         +y [H]
 *
 *           Clipping Pyramid               Drawable Pixmap
 *
 *        The x coordinate of points in the FOV viewport are not
 *        normalized between the FOV frustum near and far planes
 *        as in OpenGL or other graphics libraries.
 *
*/
/**********************************************************************/

/* Following #ifndef THREED_C block eliminates problems detected in
 * VS Code Editor when this file not included in threeD.c file.
*/
#ifndef THREED_C
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
static int        run_NUM = 0;
static int        msl_TYP = 1;
static int        img_OUT = 0;
static Pixel      pixels[8];
static Boolean    quitflag = FALSE;
#endif

#ifndef THREED_TYPES
#define THREED_TYPES
typedef short int           Integer;
typedef long int            Longint;
typedef unsigned long int   Word;
typedef double              Extended;
#endif

/* This function replaces the problematic lround() in libm for
 * Cygwin 1.5.25 and Redhat Linux 7.1
*/
Longint lroundd(double x)
{
   static int      init = 1;
   static double   zero;
   static double   half;
   static double   one;
   double          ipart;

   if ( init ) {
      zero = 0.0;
      half = 0.5;
      one  = 1.0;
      init = 0;
   }
   if ( x == zero ) return (zero);
   if ( x <  zero ) {
      if ( modf(fabs(x),&ipart) < half ) return (-ipart);
      else                               return (-(ipart+one));
   } else {
      if ( modf(x,&ipart) < half ) return (ipart);
      else                         return (ipart+one);
   }
}

/* These functions are used instead of fmin() and fmax() since
 * they are not included in libm for RedHat Linux 7.1
*/
double dmin(double x1, double x2)
{
   if ( x2 < x1 ) return x2;
   else           return x1;
}
double dmax(double x1, double x2)
{
   if ( x2 > x1 ) return x2;
   else           return x1;
}

Longint lmin(Longint x1, Longint x2)
{
   if ( x2 < x1 ) return x2;
   else           return x1;
}
Longint lmax(Longint x1, Longint x2)
{
   if ( x2 > x1 ) return x2;
   else           return x1;
}

#include "pquelib.c"

typedef struct Pnt_3D
{
  Extended  X;
  Extended  Y;
  Extended  Z;
} Pnt3D;

typedef struct Pol_Rec *PolPtr;
typedef struct Pol_Rec
{
  PolPtr  Nxt;
  Pnt3D   Pt0;
  Pnt3D   Pt1;
  Pnt3D   Pt2;
} PolRec;

typedef struct
{
  Boolean  Flg;
  Longint  Pri;
  Integer  Typ;
  Integer  Vis;
  Pnt3D    Cnt0;
  Pnt3D    Cnt1;
  Pnt3D    Nrm0;
  Pnt3D    Nrm1;
  Word     Pat;
  PolPtr   Ptr;
} Pol3D;

/* CONSTANTS */

Extended  fZero  = 0.0;
Extended  fHalf  = 0.5;
Extended  fOne   = 1.0;
Extended  fTwo   = 2.0;
Extended  f1K    = 1000.0;
Extended  rpd    = 0.01745329;  /* Radian per degree */
Longint   CpMsec = CLOCKS_PER_SEC/1000;

/* FOV INFORMATION */

Extended  fova =  90.0;  /* Field-of-View whole angle (deg)     */
Extended  fovs = 600.0;  /* Field-of-View display size (pixels) */
Extended  ratio;
Extended  fovcx;
Extended  fovcy;
Extended  fl;
Extended  flmin;
Extended  zoom;
Extended  zfovr;
Extended  sfacx, sfacy, sfacz;
Extended  sfacyAR;  // sfacy scaled by viewport aspect ratio
Extended  px, py, pz;
Extended  x, y, z;
Extended  p, t, r;  // yaw (psi), pitch (theta), roll (rho or phi)
Integer   xMax;
Integer   yMax;
Pnt3D     fovpt;
Pnt3D     offset;

/* COORDINATE TRANSFORMATION INFORMATION */

Extended  cosp, sinp;
Extended  cost, sint;
Extended  cosr, sinr;
Extended  dcx1, dcy1, dcz1;
Extended  dcx2, dcy2, dcz2;
Extended  dcx3, dcy3, dcz3;

/* GROUND PLANE GRID INFORMATION */

Pnt3D  GridPt1[4];
Pnt3D  GridPt2[4];

/* POLYGON INFORMATION */

#define maxpol  1024  /* Maximum number of polygons                 */
#define maxpnt    16  /* Maximum number of points in loaded polygon */

Integer  polcnt = 0;
Pol3D    pollist[maxpol];
Pnt3D    pntlist[maxpnt];
PQtype   polPQ;

#define poltyp_gnd  0  /* ground polygon type  */
#define poltyp_tgt  1  /* target polygon type  */
#define poltyp_msl  2  /* missile polygon type */

/* TXYZ TRAJECTORY INFORMATION */

Extended  tsec;
Integer   ktot;
Extended  XM, YM, ZM;
Extended  XT, YT, ZT;
Extended  PSM, THM, PHM;
Extended  PST, THT, PHT;
char      numstr[24];

/* DISPLAY INFORMATION */

Extended  xAspect = 1.0;
Extended  yAspect = 1.0;
Pixmap    drawn;
Pixmap    blank;
Word      SolidPattern;
Word      White  = 0;
Word      Black  = 1;
Word      Red    = 2;
Word      Green  = 3;
Word      Blue   = 4;
Word      Cyan   = 5;
Word      Yellow = 6;
Word      Brown  = 7;
Word      Colors[] = {0, 1, 2, 3, 4, 5, 6, 7};
Longint   cpumsec1;
Longint   cpumsec2;

/* I/O BUFFERS */

char  sbuff[512];

/*
 * PNT_3D VECTOR MATH FUNCTIONS
*/
Extended MagP3D(Pnt3D A)
{
   // Return scalar m as magnitude of A.
   Extended  msq;
   Extended  m;

   msq = A.X*A.X + A.Y*A.Y + A.Z*A.Z;
   if (msq > 0.0) {
      m =  sqrt(msq);
   } else {
      m = 0.0;
   }
   return m;
}

Pnt3D NrmP3D(Pnt3D A)
{
   // Return Pnt3D N as normalized vector of A.
   Extended  magA;
   Pnt3D     N;

   magA = MagP3D(A);
   if (magA > 0.0) {
      N.X = A.X / magA;
      N.Y = A.Y / magA;
      N.Z = A.Z / magA;
   } else {
      N.X = 0.0;
      N.Y = 0.0;
      N.Z = 0.0;
   }
   return N;
}

Extended DotP3D(Pnt3D A, Pnt3D B)
{
   // Return scalar d as vector dot product of A and B.
   Extended  d;

   d = A.X*B.X + A.Y*B.Y + A.Z*B.Z;
   return d;
}

Pnt3D CrossP3D(Pnt3D A, Pnt3D B)
{
   // Return Pnt3D C as vector cross product of A and B.
   Pnt3D  C;

   C.X = A.Y*B.Z - A.Z*B.Y;
   C.Y = A.Z*B.X - A.X*B.Z;
   C.Z = A.X*B.Y - A.Y*B.X;
   return C;
}

/*
 * MAKES LINKED LIST OF POLYGON DATA FROM ARRAY OF POLYGON DATA
*/
void MakePol( Integer pntcnt,
              Integer thePri,
              Integer theTyp, 
              Integer theVis,
              Word thePat, Pnt3D offset )
{
   PolPtr    newPtr;
   PolPtr    oldPtr;
   PolPtr    aPolRec;
   Extended  sumP;
   Extended  sumX;
   Extended  sumY;
   Extended  sumZ;
   Pnt3D     V0;
   Pnt3D     V1;
   Pnt3D     V01;
   Pnt3D     NrmV01;
   Integer   i;

/* Allocate memory for new polygon point record. */

   newPtr = (PolPtr)malloc(sizeof(PolRec));

/* Increment polygon counter. */

   polcnt = polcnt + 1;

#if DBG_LVL > 0
   printf("MakePol:  polcnt = %d\n", polcnt);
#endif

/* Initialize polygon list entry. */

   pollist[polcnt].Flg = FALSE;
   pollist[polcnt].Pri = thePri*100000000;
   pollist[polcnt].Pat = thePat;
   pollist[polcnt].Typ = theTyp;
   pollist[polcnt].Vis = theVis;
   pollist[polcnt].Ptr = newPtr;

/* Initialize first polygon point record. */

   newPtr->Nxt   = NULL;
   newPtr->Pt0.X = pntlist[1].X + offset.X;
   newPtr->Pt0.Y = pntlist[1].Y + offset.Y;
   newPtr->Pt0.Z = pntlist[1].Z + offset.Z;
   newPtr->Pt1.X = newPtr->Pt0.X;
   newPtr->Pt1.Y = newPtr->Pt0.Y;
   newPtr->Pt1.Z = newPtr->Pt0.Z;
   newPtr->Pt2.X = fZero;
   newPtr->Pt2.Y = fZero;
   newPtr->Pt2.Z = fZero;

#if DBG_LVL > 2
   printf("MakePol:   vertice # %d =  %f  %f  %f\n",1,newPtr->Pt0.X,
                                                      newPtr->Pt0.Y,
                                                      newPtr->Pt0.Z);
#endif

/* Initialize polygon centroid summation variables. */

   sumP = fOne;
   sumX = newPtr->Pt0.X;
   sumY = newPtr->Pt0.Y;
   sumZ = newPtr->Pt0.Z;

/* Initialize polygon point record for each additional point. */

   i = 2;
   do {
/*--- Save current polygon point record pointer. */
      oldPtr = newPtr;
/*--- Allocate next polygon point record. */
      newPtr = (PolPtr)malloc(sizeof(PolRec));
/*--- Link last polygon record to new record. */
      oldPtr->Nxt = newPtr;
/*--- Initialize new polygon point record. */
      newPtr->Nxt   = NULL;
      newPtr->Pt0.X = pntlist[i].X + offset.X;
      newPtr->Pt0.Y = pntlist[i].Y + offset.Y;
      newPtr->Pt0.Z = pntlist[i].Z + offset.Z;
      newPtr->Pt1.X = newPtr->Pt0.X;
      newPtr->Pt1.Y = newPtr->Pt0.Y;
      newPtr->Pt1.Z = newPtr->Pt0.Z;
      newPtr->Pt2.X = fZero;
      newPtr->Pt2.Y = fZero;
      newPtr->Pt2.Z = fZero;
#if DBG_LVL > 2
      printf("MakePol:   vertice # %d =  %f  %f  %f\n",i,newPtr->Pt0.X,
                                                         newPtr->Pt0.Y,
                                                         newPtr->Pt0.Z);
#endif

/*--- Increment polygon centroid summation variables. */
      sumP = sumP + fOne;
      sumX = sumX + newPtr->Pt0.X;
      sumY = sumY + newPtr->Pt0.Y;
      sumZ = sumZ + newPtr->Pt0.Z;
/*--- Increment polygon point counter. */
      i = i + 1;
   } while ( i <= pntcnt );

/* Calculate polygon centroid. */

   pollist[polcnt].Cnt0.X = sumX/sumP;
   pollist[polcnt].Cnt0.Y = sumY/sumP;
   pollist[polcnt].Cnt0.Z = sumZ/sumP;
   pollist[polcnt].Cnt1.X = sumX/sumP;
   pollist[polcnt].Cnt1.Y = sumY/sumP;
   pollist[polcnt].Cnt1.Z = sumZ/sumP;
#if DBG_LVL > 2
   printf("MakePol:  centroid 0 =  %f  %f  %f\n",pollist[polcnt].Cnt0.X,
         pollist[polcnt].Cnt0.Y,
         pollist[polcnt].Cnt0.Z);
   printf("MakePol:  centroid 1 =  %f  %f  %f\n",pollist[polcnt].Cnt1.X,
         pollist[polcnt].Cnt1.Y,
         pollist[polcnt].Cnt1.Z);
#endif

/* Calculate polygon normal assuming traversal from point 0 
   to point 1 is in a counterclockwise direction.
*/
   aPolRec = pollist[polcnt].Ptr;
   V0.X = aPolRec->Pt0.X - pollist[polcnt].Cnt0.X;
   V0.Y = aPolRec->Pt0.Y - pollist[polcnt].Cnt0.Y;
   V0.Z = aPolRec->Pt0.Z - pollist[polcnt].Cnt0.Z;
   aPolRec = aPolRec->Nxt;
   V1.X = aPolRec->Pt0.X - pollist[polcnt].Cnt0.X;
   V1.Y = aPolRec->Pt0.Y - pollist[polcnt].Cnt0.Y;
   V1.Z = aPolRec->Pt0.Z - pollist[polcnt].Cnt0.Z;

   V01    = CrossP3D(V0, V1);
   NrmV01 = NrmP3D(V01);

   pollist[polcnt].Nrm0.X = NrmV01.X;
   pollist[polcnt].Nrm0.Y = NrmV01.Y;
   pollist[polcnt].Nrm0.Z = NrmV01.Z;
   pollist[polcnt].Nrm1.X = NrmV01.X;
   pollist[polcnt].Nrm1.Y = NrmV01.Y;
   pollist[polcnt].Nrm1.Z = NrmV01.Z;

#if DBG_LVL > 2
printf("MakePol:  normal 0 =  %f  %f  %f\n",pollist[polcnt].Nrm0.X,
      pollist[polcnt].Nrm0.Y,
      pollist[polcnt].Nrm0.Z);
printf("MakePol:  normal 1 =  %f  %f  %f\n",pollist[polcnt].Nrm1.X,
      pollist[polcnt].Nrm1.Y,
      pollist[polcnt].Nrm1.Z);
#endif
}

/*
 * COMPUTE WORLD SPACE TO VIEW SPACE TRANSFORMATION MATRIX
*/
void MakeMatrix ( Extended p, Extended t, Extended r )
{
   // RHS yaw, pitch, roll as p, t, r respectively in radians.

   Extended  kctcp;
   Extended  kctsp;
   Extended  kstcp;
   Extended  kstsp;
   Extended  kcrsp;
   Extended  kcrcp;
   Extended  ksrsp;
   Extended  ksrcp;
   Extended  ksrct;
   Extended  kcrct;
   Extended  ksrstcp;
   Extended  ksrstsp;
   Extended  kcrstcp;
   Extended  kcrstsp;

   cosp = cos(p);
   sinp = sin(p);
   cost = cos(t);
   sint = sin(t);
   cosr = cos(r);
   sinr = sin(r);

   kctcp   = cost*cosp;
   kctsp   = cost*sinp;
   kstcp   = sint*cosp;
   kstsp   = sint*sinp;
   kcrsp   = cosr*sinp;
   kcrcp   = cosr*cosp;
   ksrsp   = sinr*sinp;
   ksrcp   = sinr*cosp;
   ksrct   = sinr*cost;
   kcrct   = cosr*cost;
   ksrstcp = sinr*kstcp;
   ksrstsp = sinr*kstsp;
   kcrstcp = cosr*kstcp;
   kcrstsp = cosr*kstsp;

   dcx1 =  kctcp;
   dcy1 =  kctsp;
   dcz1 = -sint;
   dcx2 =  ksrstcp - kcrsp;
   dcy2 =  ksrstsp + kcrcp;
   dcz2 =  ksrct;
   dcx3 =  kcrstcp + ksrsp;
   dcy3 =  kcrstsp - ksrcp;
   dcz3 =  kcrct;
}

/*
* TRANSFORMS GRID WORLD SPACE COORDINATES TO VIEWPORT COORDINATES
*/
void XfrmGrid ()
{
   Integer   k;
   Extended  xd, yd, zd;
   Extended  xs, ys, zs;

   if ( msl_TYP == 1 ){
      GridPt1[0].X =  2000.0;
      GridPt1[0].Y = -2000.0;
      GridPt1[0].Z =     0.0;
      GridPt1[1].X =  2000.0;
      GridPt1[1].Y =  2000.0;
      GridPt1[1].Z =     0.0;
      GridPt1[2].X = -2000.0;
      GridPt1[2].Y =  2000.0;
      GridPt1[2].Z =     0.0;
      GridPt1[3].X = -2000.0;
      GridPt1[3].Y = -2000.0;
      GridPt1[3].Z =     0.0;
   } else {
      GridPt1[0].X =  20000.0;
      GridPt1[0].Y = -16000.0;
      GridPt1[0].Z =      0.0;
      GridPt1[1].X =  20000.0;
      GridPt1[1].Y =   4000.0;
      GridPt1[1].Z =      0.0;
      GridPt1[2].X =      0.0;
      GridPt1[2].Y =   4000.0;
      GridPt1[2].Z =      0.0;
      GridPt1[3].X =      0.0;
      GridPt1[3].Y = -16000.0;
      GridPt1[3].Z =      0.0;
   }


   for ( k = 0 ; k < 4 ; k++ )
   {
/*--- TRANSLATE COORDINATES INTO VIEWPORT FOV REFERENCE FRAME */
      xd = GridPt1[k].X - fovpt.X;
      yd = GridPt1[k].Y - fovpt.Y;
      zd = GridPt1[k].Z - fovpt.Z;
/*--- ROTATE COORDINATES INTO VIEWPORT REFERENCE FRAME AND SCALE */
      xs = dcx1*xd + dcy1*yd + dcz1*zd;
      ys = dcx2*xd + dcy2*yd + dcz2*zd;
      zs = dcx3*xd + dcy3*yd + dcz3*zd;
      xs = xs;
      ys = ys*sfacyAR; // account for square clipping frustum base of fovs pixels
      zs = zs*sfacz;
/*--- SAVE SCALED VIEWPORT COORDINATES */
      GridPt2[k].X = xs;
      GridPt2[k].Y = ys;
      GridPt2[k].Z = zs;
   }
}

/*
 * TRANSFORMS POLYGON WORLD SPACE COORDINATES TO VIEWPORT COORDINATES
*/
void XfrmPoly ( Integer iPol )
{
   PolPtr       aPolRec;
   Extended     eye1x, eye1y, eye1z;
   Extended     eye2x, eye2y, eye2z;
   Pnt3D        nrm1;
   Extended     nrm2x, nrm2y, nrm2z;
   Extended     dotp;
   Extended     xd, yd, zd;
   Extended     xs, ys, zs;
   Extended     rs;
   Extended     rsmm;
   Longint      irsmm;
   Longint      pcode;
   HeapElement  anElement;
   Boolean      inflag;

#if DBG_LVL > 3
   printf("  Polygon # %d\n",iPol);
#endif

   /* Compute eye vector to polygon centroid in viewport. */

   eye1x = pollist[iPol].Cnt1.X - fovpt.X;
   eye1y = pollist[iPol].Cnt1.Y - fovpt.Y;
   eye1z = pollist[iPol].Cnt1.Z - fovpt.Z;
   eye2x = dcx1*eye1x + dcy1*eye1y + dcz1*eye1z;
   eye2y = dcx2*eye1x + dcy2*eye1y + dcz2*eye1z;
   eye2z = dcx3*eye1x + dcy3*eye1y + dcz3*eye1z;

   /* Check if polygon surface is visible. */

   if ( pollist[iPol].Vis == 2 ) {
      nrm1  = pollist[iPol].Nrm1;
      nrm2x = dcx1*nrm1.X + dcy1*nrm1.Y + dcz1*nrm1.Z;
      nrm2y = dcx2*nrm1.X + dcy2*nrm1.Y + dcz2*nrm1.Z;
      nrm2z = dcx3*nrm1.X + dcy3*nrm1.Y + dcz3*nrm1.Z;
      dotp  = nrm2x*eye2x + nrm2y*eye2y + nrm2z*eye2z;
      if (dotp > 0.0) {
         pollist[iPol].Flg = FALSE;
         return;
      }
   }

   /* Compute screen coordinates for polygon vertice. */

   inflag  = FALSE;
   pcode   = pollist[iPol].Pri;
   aPolRec = pollist[iPol].Ptr;

   while ( aPolRec != NULL )
   {
#if DBG_LVL > 4
      printf("    - 1 vertice  :  %f  %f  %f\n",aPolRec->Pt1.X,
                                                aPolRec->Pt1.Y,
                                                aPolRec->Pt1.Z);
#endif
/*--- TRANSLATE COORDINATES INTO VIEWPORT FOV REFERENCE FRAME */
      xd = aPolRec->Pt1.X - fovpt.X;
      yd = aPolRec->Pt1.Y - fovpt.Y;
      zd = aPolRec->Pt1.Z - fovpt.Z;
/*--- ROTATE COORDINATES INTO VIEWPORT REFERENCE FRAME AND SCALE */
      xs = dcx1*xd + dcy1*yd + dcz1*zd;
      ys = dcx2*xd + dcy2*yd + dcz2*zd;
      zs = dcx3*xd + dcy3*yd + dcz3*zd;
      xs = xs;
      ys = ys*sfacyAR;  // account for square clipping frustum base of fovs pixels
      zs = zs*sfacz;
/*--- SAVE SCALED VIEWPORT COORDINATES */
      aPolRec->Pt2.X = xs;
      aPolRec->Pt2.Y = ys;
      aPolRec->Pt2.Z = zs;
#if DBG_LVL > 4
      printf("    - 2 vertice  :  %f  %f  %f\n",aPolRec->Pt2.X,
                                                aPolRec->Pt2.Y,
                                                aPolRec->Pt2.Z);
#endif
      aPolRec = aPolRec->Nxt;
/*--- CHECK IF POINT IS IN FRONT OF VIEW POINT */
      if ( xs >= fZero )
      {
         inflag = TRUE;
      }
   }

   /* Enque polygon if at least one vertice is in the viewport. */

   if ( ( inflag ) && ( ! FullPQ(polPQ) ) )
   {
      rs    = sqrt(eye2x*eye2x+ eye2y*eye2y + eye2z*eye2z);
      rsmm  = rs*f1K;
      irsmm = lroundd(rsmm);
      anElement.Key  = pcode + irsmm;
      anElement.Info = iPol;
#if DBG_LVL > 3
      printf("    - element:  %ld  %hd  %hd  %hd  %ld  %f  %f  %f  %f  %ld\n",
               anElement.Key,
                  anElement.Info,
                        pollist[iPol].Typ,
                              pollist[iPol].Vis,
                                 pollist[iPol].Pri,
                                    eye2x, eye2y, eye2z, rsmm, irsmm);
#endif
      PriorityEnq(&polPQ,anElement);
      if ( irsmm < 0L ) {
         printf("*** XfrmPoly:  lroundd(rsmm) < 0 for iPol=%hd\n",iPol);
         quitflag = TRUE;
      }
   }
   pollist[iPol].Flg = inflag;
}

/*
 * MOVES POLYGONS IN WORLD SPACE
*/
void MovePoly ( Integer iPol, Extended px, Extended py, Extended pz )
{
   PolPtr    aPolRec;
   Extended  xb, yb, zb;
   Pnt3D     V0;
   Pnt3D     V1;
   Pnt3D     V01;
   Pnt3D     NrmV01;

   /* Move the polygon centroid.
   */
   xb                   = pollist[iPol].Cnt0.X;
   yb                   = pollist[iPol].Cnt0.Y;
   zb                   = pollist[iPol].Cnt0.Z;
   pollist[iPol].Cnt1.X = dcx1*xb + dcx2*yb + dcx3*zb + px;
   pollist[iPol].Cnt1.Y = dcy1*xb + dcy2*yb + dcy3*zb + py;
   pollist[iPol].Cnt1.Z = dcz1*xb + dcz2*yb + dcz3*zb + pz;
#if DBG_LVL > 3
   printf("  Polygon # %d\n",iPol);
   printf("    - centroid :  %f  %f  %f\n",pollist[iPol].Cnt1.X,
                                           pollist[iPol].Cnt1.Y,
                                           pollist[iPol].Cnt1.Z);
#endif

   /* Move the polygon vertice.
   */
   aPolRec = pollist[iPol].Ptr;
   while ( aPolRec != NULL )
   {
      xb             = aPolRec->Pt0.X;
      yb             = aPolRec->Pt0.Y;
      zb             = aPolRec->Pt0.Z;
      aPolRec->Pt1.X = dcx1*xb + dcx2*yb + dcx3*zb + px;
      aPolRec->Pt1.Y = dcy1*xb + dcy2*yb + dcy3*zb + py;
      aPolRec->Pt1.Z = dcz1*xb + dcz2*yb + dcz3*zb + pz;
#if DBG_LVL > 3
      printf("    - vertice :  %f  %f  %f\n",aPolRec->Pt1.X,
                                             aPolRec->Pt1.Y,
                                             aPolRec->Pt1.Z);
#endif
      aPolRec        = aPolRec->Nxt;
   }

   /* Compute moved polygon normal assuming traversal from point 0 
      to point 1 is in a counterclockwise direction.
   */
   aPolRec = pollist[iPol].Ptr;
   V0.X = aPolRec->Pt1.X - pollist[iPol].Cnt1.X;
   V0.Y = aPolRec->Pt1.Y - pollist[iPol].Cnt1.Y;
   V0.Z = aPolRec->Pt1.Z - pollist[iPol].Cnt1.Z;
   aPolRec = aPolRec->Nxt;
   V1.X = aPolRec->Pt1.X - pollist[iPol].Cnt1.X;
   V1.Y = aPolRec->Pt1.Y - pollist[iPol].Cnt1.Y;
   V1.Z = aPolRec->Pt1.Z - pollist[iPol].Cnt1.Z;
   
   V01    = CrossP3D(V0, V1);
   NrmV01 = NrmP3D(V01);
   
   pollist[iPol].Nrm1.X = NrmV01.X;
   pollist[iPol].Nrm1.Y = NrmV01.Y;
   pollist[iPol].Nrm1.Z = NrmV01.Z;
}

#include "cliplib.c"

/*
* DRAWS GRID LINES CLIPPED TO 3D VIEWING PYRAMID
*/
void DrawGrid3D( Integer iaxis, Display *display, Pixmap drawable )
{
   Integer   k;
   Integer   i10,i11,i20,i21;
   Extended  xd1, yd1, zd1;
   Extended  xd2, yd2, zd2;
   Integer   pcnt;
   Integer   vcnt[8];
   Pnt3D     vlist[8][mxvcnt];
   Extended  xs, ys, zs, sf;
   XPoint    tempLine[mxvcnt];
   Integer   i;

#if DBG_LVL > 4
   printf("DrawGrid3D:  Drawing grid axis %d...\n",iaxis);
#endif

/* SET LINE DRAWING COLOR */

   XSetForeground(display,the_GC,pixels[White]);

   if ( iaxis == 1 )
   {
/*--- DRAW GRID LINES PARALLEL TO WORLD X-AXIS */
      i10 = 3;
      i11 = 2;
      i20 = 0;
      i21 = 1;
   }
   else
   {
/*--- DRAW GRID LINES PARALLEL TO WORLD Y-AXIS */
      i10 = 3;
      i11 = 0;
      i20 = 2;
      i21 = 1;
   }

/* CALCULATE INCREMENTAL DISTANCES */

   xd1 = 0.025*(GridPt2[i11].X - GridPt2[i10].X);
   yd1 = 0.025*(GridPt2[i11].Y - GridPt2[i10].Y);
   zd1 = 0.025*(GridPt2[i11].Z - GridPt2[i10].Z);
   xd2 = 0.025*(GridPt2[i21].X - GridPt2[i20].X);
   yd2 = 0.025*(GridPt2[i21].Y - GridPt2[i20].Y);
   zd2 = 0.025*(GridPt2[i21].Z - GridPt2[i20].Z);

   for ( k = 0 ; k < 41 ; k++ )
   {
      pcnt = 1;

/*--- CALCULATE X COORDINATE OF UN-CLIPPED LINE */
      vlist[pcnt][1].X = GridPt2[i10].X + k*xd1;
      vlist[pcnt][2].X = GridPt2[i20].X + k*xd2;

      if ( ! ((vlist[pcnt][1].X <= flmin) && (vlist[pcnt][2].X <= flmin)) )
      {
/*------ CREATE UN-CLIPPED LINE */
         vlist[pcnt][1].Y = GridPt2[i10].Y + k*yd1;
         vlist[pcnt][1].Z = GridPt2[i10].Z + k*zd1;
         vlist[pcnt][2].Y = GridPt2[i20].Y + k*yd2;
         vlist[pcnt][2].Z = GridPt2[i20].Z + k*zd2;
         vlist[pcnt][3]   = vlist[pcnt][1];
         vcnt[pcnt]       = 3;

#if DBG_LVL > 4
         for (i=1; i<= vcnt[pcnt]; i++)
            printf("DrawGrid3D:  %d %d %f %f %f\n",pcnt,i,
                   vlist[pcnt][i].X,vlist[pcnt][i].Y,vlist[pcnt][i].Z);
#endif

/*------ CREATE CLIPPED LINE */
         PolyClip( (Integer *)&pcnt, vcnt, vlist );

#if DBG_LVL > 4
         for (i=1; i<= vcnt[pcnt]; i++)
            printf("DrawGrid3D:  %d %d %f %f %f\n",pcnt,i,
                   vlist[pcnt][i].X,vlist[pcnt][i].Y,vlist[pcnt][i].Z);
#endif

/*------ DRAW CLIPPED LINE */
         if ( vcnt[pcnt] > 2 )
         {
            for ( i = 1 ; i < 3 ; i++ )
            {
                xs              = vlist[pcnt][i].X;
                ys              = vlist[pcnt][i].Y/sfacyAR;
                zs              = vlist[pcnt][i].Z/sfacz;
                sf              = fl/xs;
                tempLine[i-1].x = lroundd(sf*ys) + floor(fovcx);
                tempLine[i-1].y = lroundd(sf*zs) + floor(fovcy);
            }
            XDrawLines(display, drawable, the_GC, tempLine, 2, CoordModeOrigin);
         }
      }
   }
}

/*
 * DRAWS POLYGON CLIPPED TO 3D VIEWING PYRAMID
*/
void DrawPoly3D( Integer iPol, Display *display, Pixmap drawable )
{
   PolPtr    aPolRec;
   Integer   pcnt, icnt;
   Integer   vcnt[8];
   Pnt3D     vlist[8][mxvcnt];
   Extended  xs, ys, zs, sf;
   XPoint    tempPoly[mxvcnt];
   Integer   i;

/* GET UN-CLIPPED POLYGON */

#if DBG_LVL > 4
   printf("DrawPoly3D:  Drawing polygon %d...\n",iPol);
#endif

   aPolRec = pollist[iPol].Ptr;
   pcnt    = 1;
   icnt    = 0;
   while ( aPolRec != NULL )
   {
      icnt              = icnt + 1;
      vlist[pcnt][icnt] = aPolRec->Pt2;
      aPolRec           = aPolRec->Nxt;
   }
   icnt              = icnt + 1;
   vlist[pcnt][icnt] = vlist[pcnt][1];
   vcnt[pcnt]        = icnt;

#if DBG_LVL > 4
   for (i=1; i<= vcnt[pcnt]; i++)
      printf("DrawPoly3D:  %d %d %f %f %f\n",pcnt,i,
             vlist[pcnt][i].X,vlist[pcnt][i].Y,vlist[pcnt][i].Z);
#endif

/* CREATE CLIPPED POLYGON */

   PolyClip( (Integer *)&pcnt, vcnt, vlist );

#if DBG_LVL > 4
   for (i=1; i<= vcnt[pcnt]; i++)
      printf("DrawPoly3D:  %d %d %f %f %f\n",pcnt,i,
             vlist[pcnt][i].X,vlist[pcnt][i].Y,vlist[pcnt][i].Z);
#endif

/* DRAW CLIPPED POLYGON */

   if ( vcnt[pcnt] > 3 )
   {
      for ( i = 1 ; i <= vcnt[pcnt] ; i++ )
      {
         xs              = vlist[pcnt][i].X;
         ys              = vlist[pcnt][i].Y/sfacyAR;
         zs              = vlist[pcnt][i].Z/sfacz;
         sf              = fl/xs;
         tempPoly[i-1].x = lroundd(sf*ys) + floor(fovcx);
         tempPoly[i-1].y = lroundd(sf*zs) + floor(fovcy);
      }
      if ( pollist[iPol].Vis > 0 )
      {
         XSetForeground(display,the_GC,pixels[pollist[iPol].Pat]);
         XFillPolygon(display, drawable, the_GC,
                      tempPoly, vcnt[pcnt], Convex, CoordModeOrigin);
      }
      else
      {
         XSetForeground(display,the_GC,pixels[pollist[iPol].Pat]);
         XSetLineAttributes(display,the_GC,2,LineSolid,CapButt,JoinMiter);
         XDrawLines(display, drawable, the_GC,
                    tempPoly, vcnt[pcnt], CoordModeOrigin);
         XSetLineAttributes(display,the_GC,1,LineSolid,CapButt,JoinMiter);
      }
   }
}

/*
 * LOADS POLYGON DATA STRUCTURES FROM FACET SHAPE MODEL POLYGON FILES.
*/
void LoadPoly ( FILE *lfni, const char* polyfile )
{
   Integer   i,k;
   Integer   polpnt, polpri, polcol, poltyp, polvis;
   Extended  polsfc;
   Extended  x, y, z;
   Extended  mdloffx, mdloffy, mdloffz, mdlsfc;
   char*     sptr;
   char      mdlnam[60];
   char      polnam[60];

   static char fmt0[]="%lf %lf %lf %lf %s\n";
   static char fmt1[]="%hd %hd %hd %hd %hd %lf %s\n";
   static char fmt2[]="%lf %lf %lf\n";

/* Read shape model offsets, scaling factor and name record. */
   sptr = fgets(sbuff,132,lfni);
   if ( sptr == NULL ) {
      printf("LoadPoly:  fgets error for 1st record in polyfile %s.\n",polyfile);
      return;
   }
   k = sscanf(sbuff,fmt0,&mdloffx,&mdloffy,&mdloffz,&mdlsfc,mdlnam);
   if ( k != 5 ) {
      printf("LoadPoly:  sscanf error for 1st record in polyfile %s.\n",polyfile);
      return;
   }

   do {
/*+++ Load polygon specification record. */
      sptr = fgets(sbuff,132,lfni);
      k = sscanf(sbuff,fmt1,&polpnt,&polpri,&polcol,&poltyp,&polvis,&polsfc,polnam);
      if ( k == 7 )
      {
#if DBG_LVL > 1
         printf("LoadPoly:  Loaded specs -  %d  %d  %d  %d  %d  %f  %s\n",
                 polpnt,polpri,polcol,poltyp,polvis,polsfc,polnam);
#endif
/*++++++ Load vertex points. */
         for ( i = 1 ; i <= polpnt ; i++ )
         {
            fgets(sbuff,132,lfni);
            sscanf(sbuff,fmt2,&x,&y,&z);
#if DBG_LVL > 1
            printf("LoadPoly:  Loaded vertex -  %f  %f  %f\n",x,y,z);
#endif
            pntlist[i].X = x*polsfc*mdlsfc;
            pntlist[i].Y = y*polsfc*mdlsfc;
            pntlist[i].Z = z*polsfc*mdlsfc;
#if DBG_LVL > 1
            printf("LoadPoly:  Scaled vertex -  %f  %f  %f\n",pntlist[i].X,
                                                              pntlist[i].Y,
                                                              pntlist[i].Z);
#endif
         }
/*++++++ Load offset point. */
         fgets(sbuff,132,lfni);
         sscanf(sbuff,fmt2,&x,&y,&z);
#if DBG_LVL > 1
         printf("LoadPoly:  Loaded offset -  %f  %f  %f\n",x,y,z);
#endif
         offset.X = x*polsfc*mdlsfc + mdloffx;
         offset.Y = y*polsfc*mdlsfc + mdloffy;
         offset.Z = z*polsfc*mdlsfc + mdloffz;
#if DBG_LVL > 1
         printf("LoadPoly:  Scaled offset -  %f  %f  %f\n",offset.X,
                                                           offset.Y,
                                                           offset.Z);
#endif
         if ( (polcol >= 0) && (polcol< 8) ) {
            MakePol(polpnt,polpri,poltyp,polvis,Colors[polcol],offset);
         } else {
            MakePol(polpnt,polpri,poltyp,polvis,Black,offset);
         }
      }
   } while ( ! ( feof(lfni) || (polcnt == maxpol) ) );
}

/*
 * 3D RENDERING OF MISSILE/TARGET ENGAGEMENT FROM TXYZ FILE
*/
void draw3D (Widget w, Display *display, Window drawable)
{
   Arg          args[10];
   Dimension    width, height;
   Extended     tanfv;
   Extended     true_tsec =  0.0;
   Extended     last_tsec = -1.0/img_FPS;
   Extended     last_XM, last_YM, last_ZM;
   Extended     DXTM, DYTM, DZTM, RTM, UXTM, UYTM, UZTM;
   Integer      n = 0;
   Integer      i, k, itot, ipad;
   HeapElement  anElement;
   XEvent       event;
   XColor       screen_def, exact_def;
   Colormap     cmap = XDefaultColormapOfScreen(XtScreen(w));
   FILE         *lfni;
   FILE         *lfnt;
   Boolean      paused = FALSE;
   Boolean      align_fov_toward_tgt = FALSE;
   Boolean      align_fov_toward_msl = FALSE;
   Boolean      align_fov_along_head = TRUE;
   Longint      waitmsec = 10;
   Integer      img_count= 0;
   Extended     img_dtsec= 1.0/img_FPS;
   char         imgout_fpath[24];
   char         grndpoly_fpath[24];
   char         mislpoly_fpath[24];
   char         txyzout_fpath[24];

/* GET PIXEL COLORS */

   if ( XAllocNamedColor(display,cmap,"white",&exact_def,&screen_def) != 0 )
   {
      pixels[White] = screen_def.pixel;
   }
   if ( XAllocNamedColor(display,cmap,"black",&exact_def,&screen_def) != 0 )
   {
      pixels[Black] = screen_def.pixel;
   }
   if ( XAllocNamedColor(display,cmap,"red",&exact_def,&screen_def) != 0 )
   {
      pixels[Red] = screen_def.pixel;
   }
   if ( XAllocNamedColor(display,cmap,"green",&exact_def,&screen_def) != 0 )
   {
      pixels[Green] = screen_def.pixel;
   }
   if ( XAllocNamedColor(display,cmap,"blue",&exact_def,&screen_def) != 0 )
   {
      pixels[Blue] = screen_def.pixel;
   }
   if ( XAllocNamedColor(display,cmap,"cyan",&exact_def,&screen_def) != 0 )
   {
      pixels[Cyan] = screen_def.pixel;
   }
   if ( XAllocNamedColor(display,cmap,"yellow",&exact_def,&screen_def) != 0 )
   {
      pixels[Yellow] = screen_def.pixel;
   }
   if ( XAllocNamedColor(display,cmap,"brown",&exact_def,&screen_def) != 0 )
   {
      pixels[Brown] = screen_def.pixel;
   }
#if DBG_LVL > 0
   printf("White = %lu\n",pixels[White]);
   printf("Black = %lu\n",pixels[Black]);
   printf("Red = %lu\n",pixels[Red]);
   printf("Green = %lu\n",pixels[Green]);
   printf("Blue = %lu\n",pixels[Blue]);
   printf("Cyan = %lu\n",pixels[Cyan]);
   printf("Yellow = %lu\n",pixels[Yellow]);
   printf("Brown = %lu\n",pixels[Brown]);
#endif

/* INITIALIZE VIEWPORT */

   XtSetArg(args[n], XtNwidth,  &width ); n++;
   XtSetArg(args[n], XtNheight, &height); n++;
   XtGetValues(w, args, n);
   xAspect = width;
   yAspect = height;
   ratio = (xAspect*1.0)/(yAspect*1.0);
   xMax  = lroundd(fovs*ratio);
   yMax  = lroundd(fovs);
   fovcx = xMax/2.0;
   fovcy = yMax/2.0;
   zoom = 1.0;
#if DBG_LVL > 0
   printf("draw3D:  xAspect,yAspect,ratio = %f %f %f\n",xAspect,yAspect,ratio);
   printf("draw3D:  xMax,yMax = %d %d\n",xMax,yMax);
   printf("draw3D:  fovcx,fovcy = %f %f\n",fovcx,fovcy);
#endif

/* DEFINE DRAWING ATTRIBUTES AND CLEAR DISPLAY */

   XSetLineAttributes(display,the_GC,1,LineSolid,CapButt,JoinMiter);
   XSetFillRule(display,the_GC,WindingRule);
   XSetForeground(display,the_GC,pixels[White]);
   XSetBackground(display,the_GC,pixels[Black]);
   XClearArea(display, drawable, 0, 0, xMax, yMax, TRUE);

/* FRAME VIEWPORT */
/* Fills viewport with black
   XSetForeground(display,the_GC,pixels[Black]);
   XFillRectangle(display,drawable,the_GC,0,0,xMax,yMax);
   XSetForeground(display,the_GC,pixels[White]);
*/
   XDrawRectangle(display,drawable,the_GC,0,0,xMax-1,yMax-1);
   XFlush(display);

/* CREATE AND INITIALIZE DRAWN PIXMAP */

   drawn = XCreatePixmap(display,drawable,xMax,yMax,
                          DefaultDepthOfScreen(XtScreen(w)));
   XCopyArea(display,drawable,drawn,the_GC,0,0,xMax,yMax,0,0);

/* CREATE AND INITIALIZE BLANK PIXMAP */

   blank = XCreatePixmap(display,drawable,xMax,yMax,
                          DefaultDepthOfScreen(XtScreen(w)));
   XCopyArea(display,drawable,blank,the_GC,0,0,xMax,yMax,0,0);

/* COMPUTE VIEWPORT FOV FOCAL LENGTHS */

   tanfv   = sin((fova/fTwo)*rpd)/cos((fova/fTwo)*rpd);
   fl      = (fovs/fTwo)/tanfv;
   flmin   = 0.1*fl;
   sfacx   = fOne;  // Not used
   sfacy   = (fOne/tanfv);
   sfacyAR = sfacy/ratio;
   sfacz   = (fOne/tanfv);
#if DBG_LVL > 0
   printf("draw3D:  tanfv,fl = %f %f\n",tanfv,fl);
   printf("draw3D:  sfacx,sfacy,sfacyAR,sfacz = %f %f %f %f\n",sfacx,sfacy,sfacyAR,sfacz);
#endif

/* READ AND MAKE OBJECT POLYGONS */

   polcnt = 0;

   sprintf(grndpoly_fpath,"./dat/grndpoly%1hd.dat",msl_TYP);
   lfni = fopen(grndpoly_fpath,"r");
   if ( lfni )
   {
#if DBG_LVL > 0
      printf("draw3D:  Loading polygons from file %s\n",grndpoly_fpath);
#endif
      LoadPoly(lfni,grndpoly_fpath);
      fclose(lfni);
   }

   lfni = fopen("./dat/fwngpoly.dat","r");
   if ( lfni )
   {
#if DBG_LVL > 0
      printf("draw3D:  Loading polygons from file %s\n","fwngpoly.dat");
#endif
      LoadPoly(lfni,"./dat/fwngpoly.dat");
      fclose(lfni);
   }

   sprintf(mislpoly_fpath,"./dat/mislpoly%1hd.dat",msl_TYP);
   lfni = fopen(mislpoly_fpath,"r");
   if ( lfni )
   {
#if DBG_LVL > 0
      printf("draw3D:  Loading polygons from file %s\n",mislpoly_fpath);
#endif
      LoadPoly(lfni,mislpoly_fpath);
      fclose(lfni);
   }

/* MAY NEED TO SAVE LAST MISSILE POSITION */
   
   XM = 0.0;
   YM = 0.0;
   ZM = 0.0;

/* OPEN TRAJECTORY DATA FILE */

   sprintf(txyzout_fpath,"./txyz/TXYZ.OUT.%04hd",run_NUM);
#if DBG_LVL > 0
   printf("draw3D:  Opening trajectory file %s\n",txyzout_fpath);
#endif
   lfnt = fopen(txyzout_fpath,"r");

/* MAIN PROCESSING LOOP OVER TRAJECTORY DATA FILE */

   while( ! ( feof(lfnt) || quitflag ) )
   {
      cpumsec1 = clock()/CpMsec;

/*--- CHECK FOR KEYPRESS EVENT */
      if ( XCheckWindowEvent(XtDisplay(w),XtWindow(w),KeyPressMask,&event) ) {
         if ( event.type == KeyPress ) {
            switch ( XkbKeycodeToKeysym(display, event.xkey.keycode, 0, event.xkey.state & ShiftMask ? 1 : 0) )
            {
            case XK_t :
               align_fov_toward_tgt = ! align_fov_toward_tgt;
               align_fov_toward_msl = FALSE;
               align_fov_along_head = FALSE;
               break;
            case XK_m :
               align_fov_toward_msl = ! align_fov_toward_msl;
               align_fov_toward_tgt = FALSE;
               align_fov_along_head = FALSE;
               break;
            case XK_h :
               align_fov_along_head = TRUE;
               align_fov_toward_msl = FALSE;
               align_fov_toward_tgt = FALSE;
               break;
            case XK_z :
               zoom    = fOne;
               zfovr   = fTwo*atan(tan(fHalf*fova*rpd)/zoom);
               tanfv   = sin(zfovr/fTwo)/cos(zfovr/fTwo);
               fl      = (fovs/fTwo)/tanfv;
               flmin   = 0.1*fl;
               sfacx   = fOne;  // Not used
               sfacy   = fOne/tanfv;
               sfacyAR = sfacy/ratio;
               sfacz   = fOne/tanfv;
               break;
            case XK_Up :
               zoom    = zoom*1.25;
               zfovr   = fTwo*atan(tan(fHalf*fova*rpd)/zoom);
               tanfv   = sin(zfovr/fTwo)/cos(zfovr/fTwo);
               fl      = (fovs/fTwo)/tanfv;
               flmin   = 0.1*fl;
               sfacx   = fOne;  // Not used
               sfacy   = fOne/tanfv;
               sfacyAR = sfacy/ratio;
               sfacz   = fOne/tanfv;
               break;
            case XK_Down :
               zoom    = zoom/1.25;
               zfovr   = fTwo*atan(tan(fHalf*fova*rpd)/zoom);
               tanfv   = sin(zfovr/fTwo)/cos(zfovr/fTwo);
               fl      = (fovs/fTwo)/tanfv;
               flmin   = 0.1*fl;
               sfacx   = fOne;  // Not used
               sfacy   = fOne/tanfv;
               sfacyAR = sfacy/ratio;
               sfacz   = fOne/tanfv;
               break;
            case XK_Right :
               waitmsec -= 10;
               if ( img_OUT == 1 ) {
                  waitmsec = lmax(0, waitmsec);
               } else {
                  waitmsec = lmax(10, waitmsec);
               }
               break;
            case XK_Left :
               waitmsec += 10;
               waitmsec = lmin(250, waitmsec);
               break;
            case XK_0 :
               if ( img_OUT == 1 ) {
                  waitmsec = 0;
               } else {
                  waitmsec = 10;
               }
               break;
            case XK_space :
               paused = ! paused;
               /*
               do {
                  XWindowEvent(XtDisplay(w),XtWindow(w),KeyPressMask,&event);
                  if ( event.type == KeyPress ) {
                     if ( XKeycodeToKeysym(display,event.xkey.keycode,0) == XK_c ) {
                        paused = FALSE;
                     }
                  }
               } while ( paused );
               */
               break;
            case XK_q :
               quitflag = TRUE;
               break;
            default :
               break;
            }
         }
      }

      if ( paused ) {
         continue;
      }

      // Save last "true" missile position (i.e., that
      // read from a previous ktot >= 0 record).
      if ( true_tsec > 0.0 ) {
         last_XM = XM;
         last_YM = YM;
         last_ZM = ZM;
      }

/*--- GET MISSILE AND TARGET POSITION */
      fgets(sbuff,128,lfnt);
      k = sscanf(sbuff,"%lf %hd %lf %lf %lf %lf %lf %lf\n",
                       &tsec,&ktot,&XM,&YM,&ZM,&XT,&YT,&ZT);
      if ( k == 8 )
      {
/*------ GET MISSILE AND TARGET ORIENTATION */
         fgets(sbuff,128,lfnt);
         //sscanf(sbuff,"%hd ",&ipad);
         if ( strstr(sbuff,"     -9999     -9999") ) {
         /* PROPNAV1.MCD 3-DOF simulation trajectory output file */
            sscanf(sbuff,"%hd %hd %lf %lf %lf %lf %lf %lf\n",
                          &ipad,&ipad,&PHM,&THM,&PSM,&PHT,&THT,&PST);
         } else {
         /* Other 6-DOF simulation trajectory output file */
            sscanf(sbuff,"%lf %lf %lf %lf %lf %lf\n",
                          &PHM,&THM,&PSM,&PHT,&THT,&PST);
         }
/*------ SKIP DECOY POSITION AND RADIANCE */
         for ( itot = 0 ; itot < ktot ; itot++ )
         {
            fgets(sbuff,128,lfnt);
         }
/*------ GET TARGET POSITION COMPONENTS */
         px = XT;
         py = YT;
         pz = ZT;
/*------ CONVERT TARGET ORIENTATION VALUES TO RADIANS */
         p = PST*rpd;
         t = THT*rpd;
         r = PHT*rpd;
#if DBG_LVL > 2
         printf("  tgt - px,py,pz,p,t,r = %f %f %f %f %f %f\n",
                         px,py,pz,p,t,r);
#endif
/*------ COMPUTE POLYGON ROTATION TRANSFORMATION MATRIX */
#if DBG_LVL > 2
         printf("draw3D:  Make target polygon transformation matrix...\n");
#endif
         MakeMatrix(p,t,r);
#if DBG_LVL > 2
         printf("           %f  %f  %f\n",dcx1,dcy1,dcz1);
         printf("           %f  %f  %f\n",dcx2,dcy2,dcz2);
         printf("           %f  %f  %f\n",dcx3,dcy3,dcz3);
#endif
/*------ MOVE TARGET POLYGONS */
#if DBG_LVL > 2
         printf("draw3D:  Move target polygons...\n");
#endif
         for ( i = 1 ; i <= polcnt ; i++ )
         {
            if ( pollist[i].Typ == poltyp_tgt )
            {
               MovePoly(i,px,py,pz);
            }
         }
/*------ GET MISSILE POSITION COMPONENTS */
         px = XM;
         py = YM;
         pz = ZM;
/*------ CONVERT MISSILE ORIENTATION VALUES TO RADIANS */
         p = PSM*rpd;
         t = THM*rpd;
         r = PHM*rpd;
#if DBG_LVL > 2
         printf("  msl - px,py,pz,p,t,r = %f %f %f %f %f %f\n",
                         px,py,pz,p,t,r);
#endif
/*------ COMPUTE POLYGON ROTATION TRANSFORMATION MATRIX */
#if DBG_LVL > 2
         printf("draw3D:  Make missile polygon transformation matrix...\n");
#endif
         MakeMatrix(p,t,r);
#if DBG_LVL > 2
         printf("           %f  %f  %f\n",dcx1,dcy1,dcz1);
         printf("           %f  %f  %f\n",dcx2,dcy2,dcz2);
         printf("           %f  %f  %f\n",dcx3,dcy3,dcz3);
#endif
/*------ MOVE MISSILE POLYGONS */
#if DBG_LVL > 2
         printf("draw3D:  Move missile polygons...\n");
#endif
         for ( i = 1 ; i <= polcnt ; i++ )
         {
            if ( pollist[i].Typ == poltyp_msl )
            {
               MovePoly(i,px,py,pz);
            }
         }
/*------ CALCULATE UNIT VECTOR FROM MISSILE TO TARGET */
         // NOTE: RHS where +X is forward, +Y is to the
         //       right and +Z is down; -Z is up.
         DXTM = XT - XM;  // NOTE: It's highly improbable missile and
         DYTM = YT - YM;  //       target positions being identical,
         DZTM = ZT - ZM;  //       yielding [DXTM,DYTM,DZTM]=[0,0,0].
         RTM  = sqrt(DXTM*DXTM + DYTM*DYTM + DZTM*DZTM);
         if ( RTM > 0.0 ) {
            UXTM = DXTM/RTM;
            UYTM = DYTM/RTM;
            UZTM = DZTM/RTM;
         } else if ( ktot > -1 ) {
            // Use last valid missile velocity direction vector.
            DXTM = XM - last_XM;
            DYTM = YM - last_YM;
            DZTM = ZM - last_ZM;
            RTM  = sqrt(DXTM*DXTM + DYTM*DYTM + DZTM*DZTM);
            UXTM = DXTM/RTM;
            UYTM = DYTM/RTM;
            UZTM = DZTM/RTM;
         }
/*------ CALCULATE FOV POSITION AND ORIENTATION */
         if ( align_fov_toward_tgt ) {
         /* Place fovpt near missile; align fov normal axis with unit vector from missile to target */
            fovpt.X = XM - 2.0*UXTM;
            fovpt.Y = YM - 2.0*UYTM;
            fovpt.Z = dmin(ZM - 2.0*UZTM + 0.5, -0.1); /* keep fovpt above ground */
            p       = atan2(UYTM,UXTM);  // Yaw    NOTE: Gimbal lock occurs when Pitch is
            t       = asin(-UZTM);       // Pitch        +/- 90 deg as both UXTM and UYTM
            r       = 0.0;               // Roll         are zero and Yaw is indeterminate.
         } else if ( align_fov_toward_msl ) {
         /* Place fovpt near target; align fov normal axis with unit vector from target to missile */
            fovpt.X = XT + 30.0*UXTM;
            fovpt.Y = YT + 30.0*UYTM;
            fovpt.Z = ZT + 30.0*UZTM + 15.0;
            p       = atan2(-UYTM,-UXTM);
            t       = asin(UZTM);
            r       = 0.0;
         } else {
         /* Place fovpt near missile; align fov normal axis with missile heading, but keep in horizontal plane */
            fovpt.X = XM - 3.0*cos(p);
            fovpt.Y = YM - 3.0*sin(p);
            fovpt.Z = dmin(ZM - 1.5, -0.1);  /* keep fovpt above ground */
            t       = 0.0;
            r       = 0.0;
         }
/*------ COMPUTE FOV ROTATION TRANSFORMATION MATRIX */
#if DBG_LVL > 2
         printf("draw3D:  Make field-of-view rotation matrix...\n");
#endif
         MakeMatrix(p,t,r);
#if DBG_LVL > 2
         printf("           %f  %f  %f\n",dcx1,dcy1,dcz1);
         printf("           %f  %f  %f\n",dcx2,dcy2,dcz2);
         printf("           %f  %f  %f\n",dcx3,dcy3,dcz3);
#endif
/*------ TRANSFORM GROUND PLANE POLYGON INTO VIEWING PORT */
#if DBG_LVL > 2
         printf("draw3D:  Transform ground plane polygon...\n");
#endif
         XfrmPoly(1);

/*------ TRANSFORM GROUND PLANE GRID INTO VIEWING PORT */
#if DBG_LVL > 2
         printf("draw3D:  Transform ground plane grid...\n");
#endif
         XfrmGrid();

/*------ TRANSFORM OBJECT POLYGONS INTO VIEWING PORT */
#if DBG_LVL > 2
         printf("draw3D:  Transform polygons...\n");
#endif
         ClearPQ(&polPQ);
         for ( i = 2 ; i <= polcnt ; i++ )
         {
            XfrmPoly(i);
         }

/*------ DRAW GROUND PLANE POLYGON */
#if DBG_LVL > 2
         printf("draw3D:  Draw ground plane polygon...\n");
#endif
         XSetLineAttributes(display,the_GC,1,LineSolid,CapButt,JoinMiter);
         if ( pollist[1].Flg ) {
            DrawPoly3D(1, display, drawn);
         }

/*------ DRAW GROUND GRID PLANE */
#if DBG_LVL > 2
         printf("draw3D:  Draw ground plane grid...\n");
#endif
         XSetLineAttributes(display,the_GC,0,LineSolid,CapButt,JoinMiter);
         DrawGrid3D(1, display, drawn);
         DrawGrid3D(2, display, drawn);

/*------ DRAW TARGET AND MISSILE POLYGONS */
#if DBG_LVL > 2
         printf("draw3D:  Draw target and missile polygons...\n");
#endif
         XSetLineAttributes(display,the_GC,1,LineSolid,CapButt,JoinMiter);
         while ( ! EmptyPQ(polPQ) )
         {
            PriorityDeq(&polPQ, &anElement);
#if DBG_LVL > 3
            printf("  %ld  %hd  %hd  %hd  %ld\n", anElement.Key,
                   anElement.Info,
                   pollist[anElement.Info].Typ,
                   pollist[anElement.Info].Vis,
                   pollist[anElement.Info].Pri);
#endif
            DrawPoly3D(anElement.Info, display, drawn);
         }

/*------ DISPLAY TIME, ZOOM, MISSILE AND TARGET STATE VARIABLES */
         XSetForeground(display,the_GC,pixels[White]);
         if ( ktot < 0 ) {
            // TXYZ padded time record
            sprintf(numstr,"Time= %8.4f",true_tsec);
         } else {
            // TXYZ true time record
            sprintf(numstr,"Time= %8.4f",tsec);
            true_tsec = tsec;
         }

         XDrawImageString(display,drawn,the_GC, 10,12,numstr,14);
         sprintf(numstr,"Zoom= %8.4f",zoom);
         XDrawImageString(display,drawn,the_GC, 10,24,numstr,14);
         sprintf(numstr,"Xm= %10.2f", XM);
         XDrawImageString(display,drawn,the_GC,100,12,numstr,14);
         sprintf(numstr,"Ym= %10.2f", YM);
         XDrawImageString(display,drawn,the_GC,100,24,numstr,14);
         sprintf(numstr,"Hm= %10.2f",-ZM);
         XDrawImageString(display,drawn,the_GC,100,36,numstr,14);
         sprintf(numstr,"PSm= %8.3f",PSM);
         XDrawImageString(display,drawn,the_GC,190,12,numstr,13);
         sprintf(numstr,"THm= %8.3f",THM);
         XDrawImageString(display,drawn,the_GC,190,24,numstr,13);
         sprintf(numstr,"PHm= %8.3f",PHM);
         XDrawImageString(display,drawn,the_GC,190,36,numstr,13);
         sprintf(numstr,"Xt= %10.2f", XT);
         XDrawImageString(display,drawn,the_GC,280,12,numstr,14);
         sprintf(numstr,"Yt= %10.2f", YT);
         XDrawImageString(display,drawn,the_GC,280,24,numstr,14);
         sprintf(numstr,"Ht= %10.2f",-ZT);
         XDrawImageString(display,drawn,the_GC,280,36,numstr,14);
         sprintf(numstr,"PSt= %8.3f",PST);
         XDrawImageString(display,drawn,the_GC,370,12,numstr,13);
         sprintf(numstr,"THt= %8.3f",THT);
         XDrawImageString(display,drawn,the_GC,370,24,numstr,13);
         sprintf(numstr,"PHt= %8.3f",PHT);
         XDrawImageString(display,drawn,the_GC,370,36,numstr,13);

/*------ FRAME VIEWPORT */
         XSetForeground(display,the_GC,pixels[White]);
         XDrawRectangle(display,drawn,the_GC,0,0,xMax-1,yMax-1);

/*------ COPY DRAWN PIXMAP TO DISPLAY WINDOW */
         XCopyArea(display,drawn,drawable,the_GC,0,0,xMax,yMax,0,0);

/*------ SAVE DRAWN PIXMAP TO X11 PIXMAP FILE */

         if ( img_OUT == 1 ) {
            if ( (tsec+0.005 - last_tsec) >= img_dtsec  ) {
               sprintf(imgout_fpath,"./Ximg/img_%04hd.xpm",img_count++);
               XpmWriteFileFromPixmap(display,imgout_fpath,drawn,None,NULL);
               last_tsec = tsec;
            }
            if ( ktot < 0 ) {
               // Duplicate last image to ensure final frame in an animated
               // GIF or MP4 video file shows time of intercept.
               sprintf(imgout_fpath,"./Ximg/img_%04hd.xpm",img_count);
               XpmWriteFileFromPixmap(display,imgout_fpath,drawn,None,NULL);               
            }
         }

/*------ COPY BLANK PIXMAP TO DRAWN PIXMAP */
         XCopyArea(display,blank,drawn,the_GC,0,0,xMax,yMax,0,0);

/*------ TIME DELAY */
         do {
            cpumsec2 = clock()/CpMsec;
         } while ( (cpumsec2-cpumsec1) < waitmsec );

/*------ RESET STRING BUFFER */
         sbuff[0] = '\0';

      }
   }

/* CLOSE TRAJECTORY DATA FILE */

   fclose(lfnt);

/* FREE OFFSCREEN PIXMAPS */

   XFreePixmap(display,drawn);
   XFreePixmap(display,blank);

}

/**********************************************************************/
/**********************************************************************/

