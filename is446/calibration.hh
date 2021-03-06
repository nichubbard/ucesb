#ifdef UNPACKER_IS_is446

CALIB_PARAM( BACK_1_E ,SLOPE_OFFSET,  6.443 , -682.431 );
CALIB_PARAM( BACK_2_E ,SLOPE_OFFSET,  4.259 , -247.077 );

CALIB_PARAM_C( BACK[0].T ,SLOPE_OFFSET,  1.0, 0.0 );
CALIB_PARAM_C( BACK[1].T ,SLOPE_OFFSET,  1.0, 0.0 );

#endif/*UNPACKER_IS_is446*/

#ifdef UNPACKER_IS_is446_toggle

CALIB_PARAM( TOGGLE 1: TGL_1_E ,SLOPE_OFFSET,   1.1 ,  1.2 );
CALIB_PARAM( TOGGLE 2: TGL_1_E ,SLOPE_OFFSET,  11.1 , 12.3 );

CALIB_PARAM_C( TGL[0].T ,SLOPE_OFFSET,  2.2 , 3.3 );

#endif/*UNPACKER_IS_is446_toggle*/

#ifdef UNPACKER_IS_is446_tglarray

CALIB_PARAM( TOGGLE 1: TGL_3_E ,SLOPE_OFFSET,   1.1 ,  1.2 );
CALIB_PARAM( TOGGLE 2: TGL_3_E ,SLOPE_OFFSET,  11.1 , 12.3 );

CALIB_PARAM_C( TGL[1].T ,SLOPE_OFFSET,  2.2 , 3.3 );

#endif/*UNPACKER_IS_is446_tglarray*/
