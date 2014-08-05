#!/bin/bash

# Script usage:
#  tc_review <username> <branch> <commitid>
#
# Notes:
#  - Call the script from the repo root folder.
#  - Processing is performed only if HEAD of the branch matches specified <commitid>
#

__STATUS=0

function is_gerrit_branch
{
  if echo $1 | grep -q -E refs/changes/[0-9]{2}/[0-9]*/[0-9]+$ ; then
    return 0;
  else
    return 1;
  fi
}

function is_first_patchset
{
  if echo $1 | grep -q -E refs/changes/[0-9]{2}/[0-9]*/1$ ; then
    return 0;
  else
    return 1;
  fi
}

function set_status
{
  if [ $1 -ne 0 ] ; then
    __STATUS=$1
  fi
}

_USERNAME=$1
_GIT_BRANCH=$2
_COMMITID=`git rev-parse $3`

_HEAD=`git rev-parse HEAD`

echo "info: _USERNAME=$_USERNAME"
echo "info: _GIT_BRANCH=$_GIT_BRANCH"
echo "info: _COMMITID=$_COMMITID"
echo "info: _HEAD=$_HEAD"

if [ $_HEAD != $_COMMITID ]; then
  echo "note: HEAD of the branch does not match specified commitid - no processing"
  exit 0
fi

# Here we set reviewers. We are doing that for the first patchset only.
if is_first_patchset $_GIT_BRANCH; then
  echo "Setting reviewers for the $_GIT_BRANCH"
  ./_tools/set_reviewers.sh $_USERNAME $_HEAD
  set_status $?
else
  echo "That's not the first patch set - no need to set reviewers"
fi

# Here we check for the coding guidelines. We simply apply them and check whether anything
# have changed in the repo.
./_tools/strip_tabs.sh
./_tools/strip_tailing_spaces.sh

__CHANGES=$(git status --porcelain -uno)
__REVIEW=
__VOTE=
if echo "$__CHANGES" | grep -q . ; then
  echo "error: the change violates coding guidelines in the following files:"
  echo "$__CHANGES"
  echo "The diff looks like:"
  git --no-pager diff
  __DIFF=$(git diff | head)
  __REVIEW="
The change violates the coding guidelines in the following files:
$__CHANGES

The head of the diff looks like:
$__DIFF

Consider to run and reupload the change:
  ./_tools/tc_review.sh <username> HEAD HEAD
  git commit --amend .
"
  __VOTE=-1
  set_status -1
else
  echo "The change follows coding guidelines"
  __REVIEW="The change follows coding guidelines."
  __VOTE=+1
fi

if is_gerrit_branch $_GIT_BRANCH ; then
  ssh -p 29418 $_USERNAME@git-ccr-1.devtools.intel.com gerrit review \
    -p mdp_msdk-samples \
    --code-review $__VOTE \
    \'--message="$__REVIEW"\' \
    `git rev-parse HEAD`

  set_status $?
fi

exit $__STATUS
