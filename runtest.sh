#!/bin/bash -e
if [[ $# -ne 1 ]]
then
  echo usage: $0 test-executable
  exit 1
fi
trap 'case $? in
        133) cat $1.log ; echo "FAILURE: Assertion failed in $1";;
        139) cat $1.log ; echo "FAILURE: Segmentation fault in $1";;
      esac' EXIT
$1 >$1.log 2>&1
RETVAL=$?
if [[ $RETVAL -ne 0 ]]
then
  cat $1.log
  echo FAILURE: $1 returned $RETVAL
  exit $RETVAL
fi
awk '/===/ { if (NF == 11) print "SUCCESS:",$2,$3,$4,$5,$6,$7,$8,$9,$10,$11 }' $1.log
