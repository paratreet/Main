#!/bin/bash
# Run density/SPH tests on paratreet
./SPH -i 1 -f adiabtophat_glass_28721.bin -v test.acc

../array/subarr test.acc.den changa_test.000000.gasden > den.diff
../array/maxarr < den.diff
../array/rmsarr < den.diff
