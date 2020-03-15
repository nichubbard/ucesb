#!/bin/bash

# x28-68 y15-36
# x12-50 y14-36
# x13-49 y0-13
# x32-62 y0-16

# The range in y had to be cut at the edges, due to drift in the
# parameters.  (This comes from the log-fits not being perfect (due to
# not being gaussians.  And towards the edges, the x-y correlations
# (equations) are not strong enough to force the values to be sane)

./fix_calib.pl det0 x28-68 y17-35 < d_3_0_1 > calib_sst_0.hh
./fix_calib.pl det1 x12-50 y14-35 < d_3_1_1 > calib_sst_1.hh
./fix_calib.pl det2 x13-49 y1-12  < d_3_2_1 > calib_sst_2.hh
./fix_calib.pl det3 x32-62 y1-15  < d_3_3_1 > calib_sst_3.hh
