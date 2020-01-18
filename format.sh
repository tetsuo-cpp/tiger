#!/bin/bash

git ls-files | grep "\.\(c\|h\)$" | grep -v "\.yy\.c$" | xargs clang-format -i
