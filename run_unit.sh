#!/bin/sh

# To run with kcov
# export KCOV="kcov /path/to/output"
# kcov output will be placed in the /path/to/output/index.html

for i in $(find ./unit -name 'test_*' -type f -perm -111); do
	filename=$(basename "$i")
	echo "$filename"
	${KCOV} $i
done
