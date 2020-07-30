##!/bin/bash

# Copyright (C) 2008-2019 Trever L. Adams
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

# Primary Hash Key Seed
phash_key=B8B5A232
# Secondary Hash Key Seed
shash_key=E35105B1

# Training Project Top Directory (all recommended/required/used directories and files must exist in this location)
topdir=~/Projects/TextFilter

# Directory for restage fixup scripts (i.e. how to make a restage partial)
# The scripts in this directory should be category_name.sh. They should
# un-restable (move out of the possibles directory) any data that snouldn't
# be restaged.
restage_fix_dir=$topdir/restage_fixup/

# Setup categories not to restage or to partially restage
declare -A notrestage
declare -A partialrestage

# notrestage["category"]=1
partialrestage["religion"]=1

# Maximum number of threads for use by the training programs that make use of multiple cores.
# This should not exceed the number of cores, and may be best at no more than n - 1.
# This should be at least 3, if you have 4 cores.
# Beyond that it is very complex and depends on a number of factors including memory, disk speed, etc.
# Using top, you can try doing mass training runs (fnb only, fhs doesn't do threading as the scripts
# haven't been updated and the program support hasn't been tested) and see what number yields the
# highest CPU utilization.
# learn_threads is how many threads the learner will use
# find_threads is how many threads the findtolearn programs will use
let find_threads=3
let learn_threads=4

# Threshold determines how high of a score an item has to be before it is ignored by fnb_findtolearn.
# The higher the value, the more files will be included, this can help ensure that data is included in the data set.
# However, this can lead to more keys in the database than necessary (speed and size issues) and can lead
# to an unbalanced data set. 100-200 has been used with success.
# Thoughts: Should probably be set to at least 50. Should probably be at least 4-5x what your setting for a "Solid"
# classification is/will be.
#let fnb_training_threshold=200 what I used to do
let fnb_training_threshold=100

# Threshold determines how high of a score an item has to be before it is ignored by fhs_findtolearn.
# The higher the value, the more files will be included, this can help ensure that data is included in the data set.
# However, this can lead to more keys in the database than necessary (speed and size issues) and can lead
# to an unbalanced data set. 6 has been used with success.
# Thoughts: FHS is a thin threshold algorithm, so this should be about 2x what the "Solid" setting is.
# (Which should likely be about 3.)
let fhs_training_threshold=6

# Threshold for "no-mans-land"
let fnb_nml_threshold=-2
let fhs_nml_threshold=-2

# Related Categories (aka Primary/Secondary categories)
# The category string must be quoted!
# This string must not contain any spaces! Use "=" to separate groups.
# Make this "\"null,null,0\"" if you do not wish to use this functionality.
# 0 means one direction only (the first is the highest, the second is the next).
# 1 means bidirectional and will be checked both ways.
related="adult.affairs, social.dating, 1=adult.*, adult.*, 0=clothing.*, clothing.*, 0=clothing.*,commerce.*,1=commerce.*, commerce.*, 0=information.culinary, commerce.food, 1"

