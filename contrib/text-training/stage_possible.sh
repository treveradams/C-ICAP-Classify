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

if [[ -z $1 || -z $2 ]]; then
	echo "My first argument MUST be the a valid category for the files to be placed as possibles."
	echo "My second argument MUST be the directory holding the files which will be placed as possibles."
	echo "NOTE: The files are removed from the original location"
	exit
fi

fnb_dir="$topdir/fnb_categories/"${1}/"possible/"
fhs_dir="$topdir/fhs_categories/"${1}/"possible/"

if [ ! -d $fnb_dir ]; then
	echo $fnb_dir " doesn't exist. Did you mispell primary argument?"
	exit
fi
#if [ ! -d $fhs_dir ]; then
#	echo $fhs_dir " doesn't exist. Did you mispell primary argument?"
#	exit
#fi

#cp $1/* $fhs_dir
mv $1/* $fnb_dir
