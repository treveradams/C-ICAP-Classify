#!/usr/bin/python

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

import url2utf8
import optparse
import re
import time
import sys


parser = optparse.OptionParser(description="Grabs pages from any site and writes out UTF-8",
	prog="anysite-singlelanguage.py",
	version="anysite 0.1",
	usage="%prog URL")

parser.add_option("-k", "--keyboard",
                  action="store_true", dest="keyboard", default=False,
                  help="If found, get urls from keyboard, not command line")

parser.add_option("-l", "--language",
                  dest="language", default='en_US',
                  help="Sets language and country code (i.e. en_US)")

(options, arguments) = parser.parse_args()

if options.keyboard == True:
	print "Enter the first url (or just enter to quit):"
	chosenurl = sys.stdin.readline()
	arguments = set()
	arguments.add(chosenurl)

	while (len(chosenurl) > 1):
		chosenurl = sys.stdin.readline()
		if len(chosenurl) > 1:
			arguments.add(chosenurl)

for url in arguments:
	try:
		url = url.strip()
		matchObj = re.match( r'(https?://(www.)?(.*?))/(.*/)?(.*)', url)
# matchObj.group(3) is host matchObj.group(5) is file
		my_time = time.localtime(time.time())
		output_file = options.language + '.UTF-8#' + matchObj.group(3) + '-' + str(my_time[0]) + str(my_time[1]).zfill(2) + str(my_time[2]).zfill(2) + '-' + str(time.time()) + '.html'
		url2utf8.main(url, output_file, [])
		time.sleep(2)
	except:
		print "Error " + url
	
