#!/bin/bash

git ls-files | grep "\.\(c\|h\)$" | grep -v "\y\..*\(c\|h\)$" | xargs clang-format -i
