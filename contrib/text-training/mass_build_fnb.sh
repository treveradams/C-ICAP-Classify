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

rm -rfv *~ > /dev/null
rm -rfv */*~ > /dev/null
rm -rfv */*/*~ > /dev/null
rm -rfv fnb_categories/fnb-classifier-acls.conf
rm -rfv fnb_categories/classifier-deny-pages.conf
rm -rfv fnb_categories/http_reply_access-acls.conf

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
		./fnb_learn -p $phash_key -s $shash_key -o fnb_files/$category.fnb -d $olddir -m $learn_threads >> /tmp/newdata-fnb.txt
	else
		./fnb_learn -p $phash_key -s $shash_key -o fnb_files/$category.fnb -d $olddir -m $learn_threads -z ${zeroset["$category"]} >> /tmp/newdata-fnb.txt
	fi
	cd $olddir
}

if [ -d fnb_files ]; then
	cd fnb_files
	rm -rf *.fnb
	cd ..
else
	mkdir fnb_files
fi;


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

for directory in *; do
	if [ -d $directory ]; then
		cd $directory
		fdupes . -r -f | xargs -- rm -rfv > /dev/null
		echo `PWD`
                # Make ACL
                echo -n "acl " >> ../fnb-classifier-acls.conf
                echo -n `PWD` >> ../fnb-classifier-acls.conf
                echo -n " rep_header X-TEXT-CATEGORY-NB ^" >> ../fnb-classifier-acls.conf
		echo -n `PWD` >> ../fnb-classifier-acls.conf
		echo "$" >> ../fnb-classifier-acls.conf
                # Make DENY INFO
                echo -n "deny_info ERR_ACCESS_DENIED_" >> ../classifier-deny-pages.conf
                echo -n `PWD | tr a-z A-Z` >> ../classifier-deny-pages.conf
                echo -n " " >> ../classifier-deny-pages.conf
                echo `PWD` >> ../classifier-deny-pages.conf
                # Make HTTP_REPLY_ACCESS ACLs
                echo -n "#http_reply_access deny TextConfidenceSolid " >> ../http_reply_access-acls.conf
                echo `PWD` >> ../http_reply_access-acls.conf
                echo >> ../http_reply_access-acls.conf
		learn
		cd .. # get out of actual category
#		../fnb_zeroset.sh $directory ../fnb_files/$directory.fnb
	else
		echo "" > /dev/null
	fi;
done
cd .. # get out of category directory

echo  >> fnb_categories/fnb-classifier-acls.conf
echo  >> fnb_categories/fnb-classifier-acls.conf
echo "acl TextConfidenceSolid rep_header X-TEXT-CATEGORY-CONFIDENCE-NB ^SOLID"  >> fnb_categories/fnb-classifier-acls.conf
echo "acl TextConfidenceAmbiguous rep_header X-TEXT-CATEGORY-CONFIDENCE-NB ^AMBIGUOUS"  >> fnb_categories/fnb-classifier-acls.conf

rm -rfv fnb_files/.fnb
./fnb_makepreload -p $phash_key -s $shash_key -o preload.fnb -d fnb_files/
$topdir/report.py $topdir/fnb_categories/ > /tmp/report-fnb.html

