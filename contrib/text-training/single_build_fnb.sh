#!/bin/bash

# Copyright (C) 2008-2016 Trever L. Adams
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

rm -rfv *~ > /dev/null
rm -rfv */*~ > /dev/null
rm -rfv */*/*~ > /dev/null

function PWD {
tmp=${PWD%/*};
[ ${#tmp} -gt 0 -a "$tmp" != "$PWD" ] && echo ${PWD:${#tmp}+1} || echo $PWD;
}

function learn {
	olddir=`pwd`
	category=`PWD`
	cd ../../
	echo $file
	echo $file >> /tmp/newdata-fnb.txt
	if [ -z ${zeroset["$category"]} ]; then
		./fnb_learn -p $phash_key -s $shash_key -o fnb_files/$category.fnb -d $olddir >> /tmp/newdata-fnb.txt
	else
		./fnb_learn -p $phash_key -s $shash_key -o fnb_files/$category.fnb -d $olddir -z ${zeroset["$category"]} >> /tmp/newdata-fnb.txt
	fi
	cd $olddir
}

if [ -d fnb_files ]; then
	cd fnb_files
	rm -rf $1.fnb
	cd ..
else
	mkdir fnb_files
fi;


cd fnb_categories
cd $1
learn
cd .. # get out of category directory
#../fnb_zeroset.sh $1 ../fnb_files/$1.fnb
cd ..

rm -rfv fnb_files/.fnb fnb_files/preload.fnb
./fnb_makepreload -p $phash_key -s $shash_key -o preload.fnb -d fnb_files/
$topdir/report.py $topdir/fnb_categories/ > /tmp/report-fnb.html

cd SRPM-data/
./make_fnb_rpm.sh
cd ..
