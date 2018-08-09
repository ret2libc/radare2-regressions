#!/bin/sh

TDIR=$(uname)-$(uname -m)
for i in ./t.archos/${TDIR}/* ; do
	./$i
done
