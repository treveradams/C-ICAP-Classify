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

echo "This should ONLY be used after a mass_build_fnb.sh call"

source fnb_zeroset.sh
source training_config.sh

function PWD {
tmp=${PWD%/*};
[ ${#tmp} -gt 0 -a "$tmp" != "$PWD" ] && echo ${PWD:${#tmp}+1} || echo $PWD;
}

if [ -d fnb_files ]; then
	cd fnb_files
	cd ..
else
	mkdir fnb_files
fi;

exec 2>> /tmp/newdata-fnb.txt
cd fnb_categories

# To avoid propagation of bad data in bugs, we MUST ALWAYS remove .pre-hash files in the actual training
# category directories!
for directory in *; do
	if [[ -d $directory && -d $directory/ && $directory != "." && $directory != ".." && ! -z $(ls $directory/) ]]; then
		rm -rfv $directory/.pre-hash-a* > /dev/null
		rm -rfv $directory/.pre-hash-b* > /dev/null
		rm -rfv $directory/.pre-hash-c* > /dev/null
		rm -rfv $directory/.pre-hash-d* > /dev/null
		rm -rfv $directory/.pre-hash-e* > /dev/null
		rm -rfv $directory/.pre-hash-f* > /dev/null
		rm -rfv $directory/.pre-hash-g* > /dev/null
		rm -rfv $directory/.pre-hash-h* > /dev/null
		rm -rfv $directory/.pre-hash-i* > /dev/null
		rm -rfv $directory/.pre-hash-j* > /dev/null
		rm -rfv $directory/.pre-hash-k* > /dev/null
		rm -rfv $directory/.pre-hash-l* > /dev/null
		rm -rfv $directory/.pre-hash-m* > /dev/null
		rm -rfv $directory/.pre-hash-n* > /dev/null
		rm -rfv $directory/.pre-hash-o* > /dev/null
		rm -rfv $directory/.pre-hash-p* > /dev/null
		rm -rfv $directory/.pre-hash-q* > /dev/null
		rm -rfv $directory/.pre-hash-r* > /dev/null
		rm -rfv $directory/.pre-hash-s* > /dev/null
		rm -rfv $directory/.pre-hash-t* > /dev/null
		rm -rfv $directory/.pre-hash-u* > /dev/null
		rm -rfv $directory/.pre-hash-v* > /dev/null
		rm -rfv $directory/.pre-hash-w* > /dev/null
		rm -rfv $directory/.pre-hash-x* > /dev/null
		rm -rfv $directory/.pre-hash-y* > /dev/null
		rm -rfv $directory/.pre-hash-z* > /dev/null
		rm -rfv $directory/.pre-hash-* > /dev/null
	fi
done

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
			echo $category >> /tmp/newdata-fnb.txt
			if [ $category="nml" ]; then
				let training_threshold=$fnb_training_threshold
			else
				let training_threshold=$fnb_nml_threshold
			fi
			use_file=`../fnb_findtolearn -p $phash_key -s $shash_key -i $directory/possible -d ../fnb_files/ -c $category -t $training_threshold -m $find_threads -r "$related"` >> /tmp/newdata-fnb.txt
			if [ $? -eq 0 ]; then
				let count=$count+1
				mv $directory/possible/$use_file $directory/$use_file
				rm -rfv $directory/possible/.pre-hash-$use_file > /dev/null
				echo $use_file
				echo $use_file >> /tmp/newdata-fnb.txt
				rm -rf ../fnb_files/$category.fnb
				if [ -z ${zeroset["$category"]} ]; then
					../fnb_learn -p $phash_key -s $shash_key -o ../fnb_files/$category.fnb -d $directory -m $learn_threads >> /tmp/newdata-fnb.txt
				else
					../fnb_learn -p $phash_key -s $shash_key -o ../fnb_files/$category.fnb -d $directory -m $learn_threads -z ${zeroset["$category"]} >> /tmp/newdata-fnb.txt
				fi
			fi;
			echo $count
		else
			echo "" > /dev/null
		fi;
	done;
	../fnb_makepreload -p $phash_key -s $shash_key -o preload.fnb -d ../fnb_files/
	$topdir/report.py $topdir/fnb_categories/ > /tmp/report-fnb.html
done;
cd .. # get out of category directory

rm -rfv fnb_files/.fnb
./fnb_makepreload -p $phash_key -s $shash_key -o preload.fnb -d fnb_files/
$topdir/report.py $topdir/fnb_categories/ > /tmp/report-fnb.html

cd SRPM-data/
./make_fnb_rpm.sh
cd ..
