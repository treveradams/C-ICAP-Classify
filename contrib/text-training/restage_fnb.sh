#!/bin/bash

# Copyright (C) 2008-2016 Trever L. Adams
# While these scripts are designed to work with C-ICAP Classify,
# They are not part of it and are NOT under the GNU LGPL v3.
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, version 3 of the License.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

source fnb_zeroset.sh
source training_config.sh

function PWD {
tmp=${PWD%/*};
[ ${#tmp} -gt 0 -a "$tmp" != "$PWD" ] && echo ${PWD:${#tmp}+1} || echo $PWD;
}

if [ -d fnb_files ]; then
	rm -rfv fnb_files/*
else
	mkdir fnb_files
fi;


cd fnb_categories
let count=10
for directory in *; do
	category=$directory
	if [[ -d $directory && -d $directory/possible/ && $directory != "." && $directory != ".." ]]; then
		if [ -z ${notrestage["$category"]} ]; then
			echo "RESTAGING:" $category
			mv $directory/* $directory/possible/
			rm -rfv $directory/possible/.pre-hash-a* > /dev/null
			rm -rfv $directory/possible/.pre-hash-b* > /dev/null
			rm -rfv $directory/possible/.pre-hash-c* > /dev/null
			rm -rfv $directory/possible/.pre-hash-d* > /dev/null
			rm -rfv $directory/possible/.pre-hash-e* > /dev/null
			rm -rfv $directory/possible/.pre-hash-f* > /dev/null
			rm -rfv $directory/possible/.pre-hash-g* > /dev/null
			rm -rfv $directory/possible/.pre-hash-h* > /dev/null
			rm -rfv $directory/possible/.pre-hash-i* > /dev/null
			rm -rfv $directory/possible/.pre-hash-j* > /dev/null
			rm -rfv $directory/possible/.pre-hash-k* > /dev/null
			rm -rfv $directory/possible/.pre-hash-l* > /dev/null
			rm -rfv $directory/possible/.pre-hash-m* > /dev/null
			rm -rfv $directory/possible/.pre-hash-n* > /dev/null
			rm -rfv $directory/possible/.pre-hash-o* > /dev/null
			rm -rfv $directory/possible/.pre-hash-p* > /dev/null
			rm -rfv $directory/possible/.pre-hash-q* > /dev/null
			rm -rfv $directory/possible/.pre-hash-r* > /dev/null
			rm -rfv $directory/possible/.pre-hash-s* > /dev/null
			rm -rfv $directory/possible/.pre-hash-t* > /dev/null
			rm -rfv $directory/possible/.pre-hash-u* > /dev/null
			rm -rfv $directory/possible/.pre-hash-v* > /dev/null
			rm -rfv $directory/possible/.pre-hash-w* > /dev/null
			rm -rfv $directory/possible/.pre-hash-x* > /dev/null
			rm -rfv $directory/possible/.pre-hash-y* > /dev/null
			rm -rfv $directory/possible/.pre-hash-z* > /dev/null
			rm -rfv $directory/possible/.pre-hash-* > /dev/null
			if [ -n ${partialrestage["$category"]} ]; then
				$restage_fix_dir/$category.sh
			fi;
		fi;
	else
		echo "" > /dev/null
	fi;
done;
cd .. # get out of category directory

rm -rfv fnb_files/.fnb
./mass_build_fnb.sh

