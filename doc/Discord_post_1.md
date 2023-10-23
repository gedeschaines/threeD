The above screen capture video shows img_anim_0001.mp4 file playing at 25% speed within MS Media Player. The img_anim_0001.mp4 file was generated using ImageMagick convert and ffmpeg to merge pixel buffer images captured as XPM files from a program written more than 30 years ago (before OpenGL was created) to process missile/target engagement trajectory output files from 3-DOF and 6-DOF missile simulations and generate 3D faceted object rendering using painter's algorithm for hidden surface removal. I originally wrote the program in Pascal on an Apple Mac in 1986, and later refactored it to FORTRAN and C.

I've provided the following C code snippet for those in #opengl that have expressed interest in how to place and orient an observer's fov in 3D Cartesian space.

```
/*------ CALCULATE UNIT VECTOR FROM MISSILE TO TARGET */
         // NOTE: RHS where +X is forward, +Y is to the
         //       right and +Z is down; -Z is up.
         DXTM = XT - XM;  // NOTE: It's highly improbable missile and
         DYTM = YT - YM;  //       target positions would be identical,
         DZTM = ZT - ZM;  //       yielding [DXTM,DYTM,DZTM]=[0,0,0].
         RTM  = sqrt(DXTM*DXTM + DYTM*DYTM + DZTM*DZTM);
         UXTM = DXTM/RTM;
         UYTM = DYTM/RTM;
         UZTM = DZTM/RTM;
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
         /* Place fovpt near missile; align fov normal axis with missile heading, but keep in                       horizontal plane */
            fovpt.X = XM - 3.0*cos(p);
            fovpt.Y = YM - 3.0*sin(p);
            fovpt.Z = dmin(ZM - 1.5, -0.1);  /* keep fovpt above ground */
            t       = 0.0;
            r       = 0.0;
         }
/*------ COMPUTE FOV ROTATION TRANSFORMATION MATRIX */
         MakeMatrix(p,t,r);
```
 