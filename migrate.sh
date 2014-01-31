#!/usr/bin/env bash

BLACKLIST='.vcxproj\|.sln\|.unsuccessfulbuild'
git log --name-only --pretty="format:" --author="Nathan Andrews" --relative=sp/src > ./tmp

cd sp/src
cat ../../tmp | grep -v $BLACKLIST | sort | uniq | while read line
do 	
	[[ -z "$line" ]] && continue	
	OLD=$line 
	NEW=../../mp/src/	
	cp --parents $OLD $NEW
done

rm -rf ../../tmp

cd ../game/mod_hl2/

cp --parents -R materials/* ../../../mp/game/mod_hl2mp
cp --parents -R models/* ../../../mp/game/mod_hl2mp
cp --parents -R cfg/* ../../../mp/game/mod_hl2mp
cp --parents -R particles/* ../../../mp/game/mod_hl2mp
cp --parents -R resource/* ../../../mp/game/mod_hl2mp
cp --parents -R scripts/* ../../../mp/game/mod_hl2mp