##!/bin/bash

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


declare -A zeroset

zeroset["adult.affairs"]=2
zeroset["adult.artnudes"]=2
zeroset["adult.naturism"]=2
zeroset["adult.pinups"]=2
zeroset["adult.porn"]=2
zeroset["adult.sextoys"]=2
zeroset["adult.sexuality"]=2
zeroset["antisocial.bombmaking"]=2
zeroset["antisocial.hacking"]=2
zeroset["antisocial.hate"]=2
zeroset["antisocial.violence"]=2
zeroset["antisocial.warez"]=2
zeroset["childfriendly"]=2
zeroset["clothing"]=2
zeroset["clothing.intimates.female"]=2
zeroset["clothing.intimates.male"]=2
zeroset["clothing.swimsuit.female"]=2
zeroset["clothing.swimsuit.male"]=2
zeroset["commerce.auctions"]=2
zeroset["commerce.banking"]=2
zeroset["commerce.food"]=2
zeroset["commerce.jewelry"]=2
zeroset["commerce.payment"]=2
zeroset["commerce.shopping"]=2
zeroset["downloads.cellphones"]=2
zeroset["downloads.desktopsillies"]=2
zeroset["education"]=2
zeroset["e-mail"]=2
zeroset["entertainment"]=2
zeroset["entertainment.gambling"]=2
zeroset["entertainment.gossip"]=2
zeroset["entertainment.KidsTimeWasting"]=2
zeroset["entertainment.onlinegames"]=2
zeroset["entertainment.sports"]=2
zeroset["entertainment.vacation"]=2
zeroset["government"]=2
zeroset["health"]=2
zeroset["health.addictions"]=2
zeroset["health.sexual"]=2
zeroset["information.childcare"]=2
zeroset["information.culinary"]=2
zeroset["information.gardening"]=2
zeroset["information.homerepair"]=2
zeroset["information.news"]=2
zeroset["information.parenting"]=2
zeroset["information.pets"]=2
zeroset["information.technical"]=2
zeroset["information.weather"]=2
zeroset["instantmessaging"]=2
zeroset["intoxicants.BeerLiquor"]=2
zeroset["intoxicants.Drugs"]=2
zeroset["intoxicants.tobacco"]=2
zeroset["jobsearch"]=2
zeroset["nml"]=2
zeroset["personalfinance"]=2
zeroset["personalfinance.investing"]=2
zeroset["radio"]=2
zeroset["realestate"]=2
zeroset["religion"]=2
zeroset["social.blogs"]=2
zeroset["social.dating"]=2
zeroset["social.reconnect"]=2
zeroset["social.telephony"]=2
zeroset["weapons"]=2

#if [ -z ${zeroset["$1"]} ]; then
#	echo "No Zero Set, continuing without shrinking"
#else
#	../fnb_learn -o $2 -z ${zeroset["$1"]}
#fi
