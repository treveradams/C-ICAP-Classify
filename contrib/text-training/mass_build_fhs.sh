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
rm -rfv fhs_categories/fhs-classifier-acls.conf
rm -rfv fhs_categories/classifier-deny-pages.conf
rm -rfv fhs_categories/http_reply_access-acls.conf

function PWD {
tmp=${PWD%/*};
[ ${#tmp} -gt 0 -a "$tmp" != "$PWD" ] && echo ${PWD:${#tmp}+1} || echo $PWD;
}

function learn {
	for file in *; do
		if [ -f $file ]; then
			olddir=`pwd`
			category=`PWD`
			cd ../../
			echo $file
			echo $file >> /tmp/newdata.txt
			./fhs_learn -p $phash_key -s $shash_key -o fhs_files/$category.fhs -i $olddir/$file >> /tmp/newdata.txt
			cd $olddir
		fi;
	done
}

if [ -d fhs_files ]; then
	cd fhs_files
	rm -rf *.fhs
	cd ..
else
	mkdir fhs_files
fi;


cd fhs_categories
for directory in *; do
	if [ -d $directory ]; then
		cd $directory
		fdupes . ./possible -r -f | xargs -- rm -rfv > /dev/null
		echo `PWD`
                # Make ACL
                echo -n "acl " >> ../fhs-classifier-acls.conf
                echo -n `PWD` >> ../fhs-classifier-acls.conf
                echo -n " rep_header X-TEXT-CATEGORY-HS ^" >> ../fhs-classifier-acls.conf
		echo -n `PWD` >> ../fhs-classifier-acls.conf
		echo "$" >> ../fhs-classifier-acls.conf
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
	else
		echo "" > /dev/null
	fi;
done
cd .. # get out of category directory

echo  >> fhs_categories/fhs-classifier-acls.conf
echo  >> fhs_categories/fhs-classifier-acls.conf
echo "acl TextConfidenceSolid rep_header X-TEXT-CATEGORY-CONFIDENCE-HS ^SOLID"  >> fhs_categories/fhs-classifier-acls.conf
echo "acl TextConfidenceAmbiguous rep_header X-TEXT-CATEGORY-CONFIDENCE-HS ^AMBIGUOUS"  >> fhs_categories/fhs-classifier-acls.conf

rm -rfv fhs_files/.fhs
./fhs_makepreload -p $phash_key -s $shash_key -o preload.fhs -d fhs_files/
$topdir/report.py $topdir/fhs_categories/ > /tmp/report-fhs.html
