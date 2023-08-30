#!/bin/bash
if [[ $# -ne 3 ]]
then
  echo usage: $0 executable object-directory html-directory
  exit 1
fi
lcov -z -d $2 -o $2/coverage_full.info
./runtest.sh $1
lcov -c -d $2 -o $2/coverage_full.info
lcov -r $2/coverage_full.info '/usr/include/*' '/*thirdparty/*' -o $2/coverage_filtered.info
genhtml -q -o $3 $2/coverage_filtered.info
