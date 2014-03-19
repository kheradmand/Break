#!/usr/bin/env bash

PB="break-lt"
FL=file
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cp $DIR/../../$PB $DIR;
cp $DIR/../../$FL $DIR;
rm -f $DIR/$FL.bz2
$DIR/$PB $DIR/$FL
let rnd=RANDOM%1000;
if [[ $rnd = 0 ]]; then
	rm -f _runtime-address-order-graph.txt
fi
