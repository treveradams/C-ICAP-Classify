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

source training_config.sh

$topdir/fnb_findtolearn -p $phash_key -s $shash_key -i $1 -d $topdir/fnb_files -t $fnb_training_threshold -c $2 -m $find_threads
