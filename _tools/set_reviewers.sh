#!/bin/bash

# Script usage:
#  set_reviewers <username> <change>
# where <change> can be either Git commitid or Gerrit Change-Id or Gerrit change number

__USER=$1
__CHANGE=$2

if [ -z $__USER ]; then
  echo "error: no username specified"
  exit -1
fi

if [ -z $__CHANGE ]; then
  echo "error: no change specified"
  exit -1
fi

ssh -p 29418 $__USER@git-ccr-1.devtools.intel.com gerrit set-reviewers \
  -p mdp_msdk-samples \
  -a dvrogozh \
  -a fzharino \
  -a sidorovv \
  $__CHANGE
