#!/bin/bash

find . \( -name '*.[hc]' -o -name '*.cpp' -o -name '*.cl' \) \
  -exec sed -i 's/[ \t]*$//' {} \;
