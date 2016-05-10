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

echo "This should ONLY be used after a mass_build_fhs.sh call"

source training_config.sh

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
		echo $file >> /tmp/newdata-fhs.txt
		./fhs_learn -p $phash_key -s $shash_key -o fhs_files/$category.fhs -i $olddir/$file >> /tmp/newdata-fhs.txt
		cd $olddir
	done
}

if [ -d fhs_files ]; then
	cd fhs_files
	cd ..
else
	mkdir fhs_files
fi;


exec 2>> /tmp/newdata-fhs.txt
cd fhs_categories

case $1 in
keep)
	echo -e "\nKeeping pre-hash data\n"
	;;
* )
	echo "Call me with \"keep\" as first argument to not erase pre-hash data"
	for directory in *; do
		if [[ -d $directory && -d $directory/possible/ && $directory != "." && $directory != ".." && ! -z $(ls $directory/possible/) ]]; then
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
		fi
	done
	;;
esac

let count=10
until [ $count -eq 0 ]; do
	let count=0
	for directory in *; do
		if [[ -d $directory && -d $directory/possible/ && $directory != "." && $directory != ".." && ! -z $(ls $directory/possible/) ]]; then
			category=$directory
			echo $category
			echo $category >> /tmp/newdata-fhs.txt
			if [ $category="nml" ]; then
				let training_threshold=$fhs_training_threshold
			else
				let training_threshold=$fhs_nml_threshold
			fi
			use_file=`../fhs_findtolearn -p $phash_key -s $shash_key -i $directory/possible -d ../fhs_files/ -c $category -t $training_threshold -r "$related"` >> /tmp/newdata-fhs.txt
			if [ $? -eq 0 ]; then
				let count=$count+1
				mv $directory/possible/$use_file $directory/$use_file
				echo $use_file
				echo $use_file >> /tmp/newdata-fhs.txt
				../fhs_learn -p $phash_key -s $shash_key -o ../fhs_files/$category.fhs -i $directory/$use_file >> /tmp/newdata-fhs.txt
			fi;
			echo $count
		else
			echo "" > /dev/null
		fi;
	done;
	../fhs_makepreload -p $phash_key -s $shash_key -o preload.fhs -d ../fhs_files/
	$topdir/report.py $topdir/fhs_categories/ > /tmp/report-fhs.html
done;
cd .. # get out of category directory

echo  >> fhs_categories/fhs-classifier-acls.conf
echo  >> fhs_categories/fhs-classifier-acls.conf
echo "acl TextConfidenceSolid rep_header X-TEXT-CATEGORY-CONFIDENCE-HS ^SOLID"  >> fhs_categories/fhs-classifier-acls.conf
echo "acl TextConfidenceAmbiguous rep_header X-TEXT-CATEGORY-CONFIDENCE-HS ^AMBIGUOUS"  >> fhs_categories/fhs-classifier-acls.conf

rm -rfv fhs_files/.fhs
./fhs_makepreload -p $phash_key -s $shash_key -o preload.fhs -d fhs_files/
$topdir/report.py $topdir/fhs_categories/ > /tmp/report-fhs.html
