#!/bin/bash
if [[ $# -eq 0 ]]
then
  echo usage: $0 executable ...
  exit 1
fi
LOG=$1.log
trap 'case $? in
        133) cat $LOG ; echo "FAILURE: Assertion failed";;
        139) cat $LOG ; echo "FAILURE: Segmentation fault";;
      esac' EXIT
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $* >$LOG 2>&1
RETVAL=$?
if [[ $RETVAL -ne 0 ]]
then
  cat $LOG
  echo FAILURE: $1 returned $RETVAL
  exit $RETVAL
fi
awk '/===|total heap usage/{if(NF==11){$1="SUCCESS:";print}}' $LOG
