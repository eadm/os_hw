#!/bin/bash

DIR="$1/*"
TIME="+7"

FILES=`find $DIR -mtime $TIME`

for f in $FILES
do
	if test -L $f; then
		#echo "$f is symlink"
		if [ ! -e $f ]; then
			echo "$f"
		#else
		#	echo "Bad"
		fi
	#else 
	#	echo "$f isn't symlink"
	fi
done

