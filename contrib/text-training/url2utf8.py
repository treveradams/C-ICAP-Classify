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

from urllib2 import Request, urlopen, URLError, HTTPError, unquote
import optparse
import codecs
import re
import ssl

def main(site, output_file, cookies):
	try:
		site = site.strip()
		req = Request(site, None, {'User-Agent' : 'Mozilla/5.0 (X11; Linux x86_64; rv:18.0) Gecko/20100101 Firefox/18.0', 'Accept-Language' : 'en-us,en;q=0.8,he;q=0.5,es;q=0.3', 'Accept' : 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8'})
		if cookies != None:
			output_cookies = ''
			for cookie in cookies:
				if len(output_cookies) > 0:
					output_cookies = output_cookies + '; '
				name, value = cookie
				output_cookies = output_cookies + name + '=' + value
			req.add_header('Cookie', output_cookies)
		output = urlopen(req)
		headers = output.info()
		output = output.read()
		matchObj = re.search( r'(<meta [^>]*?charset=\"?([^\">]*?)\")', output, flags = re.M + re.I + re.S)
		if matchObj is not None and matchObj.group(2):
			output = output.decode(matchObj.group(2))
		else:
			try:
				matchObj = re.search( r'(.*charset=([^;]*))', headers['Content-Type'], flags = re.M + re.I + re.S)
				output = output.decode(matchObj.group(2))
			except:
				output = output.decode('ISO-8859-1')
		output_file = unquote(output_file)
		file = codecs.open(output_file, "w", "utf-8")
		file.write(output)
		file.close()
	except HTTPError, e:
		print 'The server couldn\'t fulfill the request. For ' + site
		print 'Error code: ', e.code
	except URLError, e:
		print 'Skipping URI ' + site
		print 'Reason: ', e.reason
	except UnicodeDecodeError, e:
		print site + ': '
		print e

def __main__():
	try:
		p = optparse.OptionParser(description='Grabs Web Pages and Writes Out UTF-8 Files',
			prog='url2utf8.py',
			version='url2utf8 0.1',
			usage='%prog SITE SAVE_FILE')

		options, arguments = p.parse_args()

#	main(arguments[0], arguments[1])
	except IndexError:
		print
