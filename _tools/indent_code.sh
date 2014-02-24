#!/bin/bash

# For details see documentation @ http://astyle.sourceforge.net/
# --style=linux : Linux style formatting
#
#int Foo(bool isBar)
#{
#    if (isFoo) {
#        bar();
#        return 1;
#    } else
#        return 0;
#}

ASTYLE_OPTIONS=" \
  --indent=spaces=4 \
  --convert-tabs \
  --align-pointer=type \
  --pad-header \
  --break-after-logical \
  --indent-preproc-define \
  --suffix=none \
  "

find . \( -name '*.[hc]' -o -name '*.cpp' \) \
  -exec astyle --style=linux $ASTYLE_OPTIONS {} \;
