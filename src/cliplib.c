/**********************************************************************/
/* FILE:  cliplib.c
 * DATE:  24 AUG 2000
 * AUTH:  G. E. Deschaines
 * DESC:  Methods to determine clipping of a given line segment with
 *        the edges of a viewing pyramidal frustum.  These methods
 *        were derived from algorithms presented on pages 152-155 in
 *        Chapter 3 of "Procedural Elements for Computer Graphics"
 *        by David F. Rogers, published by McGraw-Hill, Inc., 1985.
*/
/**********************************************************************/

#ifndef THREED_TYPES
#define THREED_TYPES
typedef short int           Integer;
typedef long int            Longint;
typedef unsigned long int   Word;
typedef double              Extended;
#endif

#define mxvcnt  32       /* maximum vertices in clipped polygon */
#define zmin        0.1  /* minimum z clipping distance         */
#define zmax    10000.0  /* maximum z clipping distance         */

/*
 * Calculates edge code for given pyramidal frustum edge and
 * polygon vertex point.
*/ 
void EdgeCode( Integer edge, Pnt3D a_pt, Longint *code )
{
   Extended  x, y, z;

/* Load point into viewing pyramid space. */

   x =  a_pt.Y;
   y = -a_pt.Z;
   z =  a_pt.X;

/* Initialize edge code. */

   *code = 0;

/* Calculated edge code. */

   switch ( edge )
   {
   case 1 : if ( x == -z ) return;      /* on left edge      */
            if ( x <  -z ) *code = -1;  /* outside left edge */
            else           *code =  1;  /* inside left edge  */
            break;
   case 2 : if ( x ==  z ) return;      /* on right edge      */
            if ( x >   z ) *code = -2;  /* outside right edge */
            else           *code =  2;  /* inside right edge  */
            break;
   case 3 : if ( y == -z ) return;      /* on bottom edge    */
            if ( y <  -z ) *code = -4;  /* below bottom edge */
            else           *code =  4;  /* above bottom edge */
            break;
   case 4 : if ( y ==  z ) return;      /* on top edge    */
            if ( y >   z ) *code = -8;  /* above top edge */
            else           *code =  8;  /* below top edge */
            break;
   case 5 : if ( z ==  zmax ) return;       /* on zmax          */
            if ( z >   zmax ) *code = -16;  /* in front of zmax */
            else              *code =  16;  /* behind zmax      */
            break;
   case 6 : if ( z ==  zmin ) return;       /* on zmin          */
            if ( z <   zmin ) *code = -32;  /* behind zmin      */
            else              *code =  32;  /* in front of zmin */
            break;
   }
}

/*
 * Determines pyramidal frustum edge clipping of given line segment.
*/
void EdgeClip( Integer edge, Pnt3D pt_s, Pnt3D pt_e, Pnt3D *pt_i )
{
   Extended  xs, ys, zs;     /* Start point   */
   Extended  xe, ye, ze;     /* End point     */
   Extended  xsp, ysp, zsp;  /* Clipped point */
   Extended  k, t;
  
   xs =  pt_s.Y;
   ys = -pt_s.Z;
   zs =  pt_s.X;
   xe =  pt_e.Y;
   ye = -pt_e.Z;
   ze =  pt_e.X;
   switch ( edge ) 
   { 
   case 1 :  k   = (xe-xs);            /* left edge intercept */
             t   = (zs+xs)/(zs-ze-k);
             xsp = k*t + xs;
             ysp = (ye-ys)*t + ys;
             zsp = -xsp;
             break;
   case 2 :  k   = (xe-xs);            /* right edge intercept */
             t   = (zs-xs)/(zs-ze+k);
             xsp = k*t + xs;
             ysp = (ye-ys)*t + ys;
             zsp = xsp;
             break;
   case 3 :  k   = (ye-ys);            /* bottom edge intercept */
             t   = (zs+ys)/(zs-ze-k);
             xsp = (xe-xs)*t + xs;
             ysp = k*t + ys;
             zsp = -ysp;
             break;
   case 4 :  k   = (ye-ys);            /* top edge intercept */
             t   = (zs-ys)/(zs-ze+k);
             xsp = (xe-xs)*t + xs;
             ysp = k*t + ys;
             zsp = ysp;
             break;
   case 5 :  k   = (ze-zs);            /* max z clip plane intercept */
             t   = (zmax-zs)/k;
             xsp = (xe-xs)*t + xs;
             ysp = (ye-ys)*t + ys;
             zsp = zmax;
             break;
   case 6 :  k   = (ze-zs);            /* min z clip plane intercept */
             t   = (zmin-zs)/k;
             xsp = (xe-xs)*t + xs;
             ysp = (ye-ys)*t + ys;
             zsp = zmin;
             break;
   }
   pt_i->X =  zsp;
   pt_i->Y =  xsp;
   pt_i->Z = -ysp;
}

/*
 * Clips given polygon to 3D viewing pyramidal frustum.
*/
void PolyClip( Integer* pcnt, Integer vcnt[], Pnt3D vlist[][mxvcnt] )
{
   Longint  cs, ce;
   Integer  pcntp1;
   Integer  icnt, jcnt;
   Pnt3D    pt_S, pt_E, pt_X;

   do {
   /* check all polygon points against each frustum edge */
      pcntp1 = *pcnt + 1;
      jcnt   = 0;
      pt_S   = vlist[*pcnt][1];
      EdgeCode(*pcnt,pt_S,&cs);
      if ( cs >= 0 ) {
      /* pt_S inside or on frustum edge - save */
         jcnt                = jcnt + 1;
         vlist[pcntp1][jcnt] = pt_S;
      }
      
      for ( icnt = 2 ; icnt <= vcnt[*pcnt] ; icnt++ ) {
      /* check all subsequent points along polygon */
         pt_E = vlist[*pcnt][icnt];
         EdgeCode(*pcnt,pt_E,&ce);
         if ( cs != ce ) {
         /* line segment intercepts frustum edge */
            if ( cs < ce ) {
            /* pt_S left of pt_E */
               EdgeClip(*pcnt,pt_S,pt_E,&pt_X);
	    } else {
            /* pt_E left of pt_S */
               EdgeClip(*pcnt,pt_E,pt_S,&pt_X);
            }
            /* save this intercept */
	    jcnt                = jcnt + 1;
	    vlist[pcntp1][jcnt] = pt_X;
         }
         if ( icnt < vcnt[*pcnt] ) {
         /* not last polygon point */
            pt_S = pt_E;
	    cs   = ce;
	    if ( cs >= 0 ) {
            /* save this point */
	       jcnt                = jcnt + 1;
	       vlist[pcntp1][jcnt] = pt_S;
	    }
         }
      }
      if ( jcnt > 0 ) {
      /* close polygon */
         jcnt                = jcnt + 1;
         vlist[pcntp1][jcnt] = vlist[pcntp1][1];
      }
      *pcnt       = pcntp1;
      vcnt[*pcnt] = jcnt;
#if DBG_LVL > 5
      Integer i;
      for (i=1; i<=vcnt[*pcnt]; i++)
         printf("PolyClip:  %d %d %f %f %f\n",*pcnt,i,
                vlist[*pcnt][i].X,vlist[*pcnt][i].Y,vlist[*pcnt][i].Z);
#endif
   } while ( ! ( ( *pcnt == 7 ) || ( jcnt == 0 ) ) );
}

/**********************************************************************/
/**********************************************************************/

