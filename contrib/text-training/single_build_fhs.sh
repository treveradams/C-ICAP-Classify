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

source training_config.sh

rm -rfv *~ > /dev/null
rm -rfv */*~ > /dev/null
rm -rfv */*/*~ > /dev/null

function PWD {
tmp=${PWD%/*};
[ ${#tmp} -gt 0 -a "$tmp" != "$PWD" ] && echo ${PWD:${#tmp}+1} || echo $PWD;
}

function learn {
	for file in *; do
		olddir=`pwd`
		category=`PWD`
		cd ../../
		echo $file
		echo $file >> /tmp/newdata.txt
		./fhs_learn --p $phash_key -s $shash_key -o fhs_files/$category.fhs -i $olddir/$file >> /tmp/newdata.txt
		cd $olddir
	done
}

if [ -d fhs_files ]; then
	cd fhs_files
	rm -rf $1.fhs
	cd ..
else
	mkdir fhs_files
fi;


cd fhs_categories
cd $1
learn
cd .. # get out of category directory
cd ..

rm -rfv fhs_files/.fhs fhs_files/preload.fhs
./fhs_makepreload -p $phash_key -s $shash_key -o preload.fhs -d fhs_files/
$topdir/report.py $topdir/fhs_categories/ > /tmp/report-fhs.html
