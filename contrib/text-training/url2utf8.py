#!/usr/bin/python

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

from urllib.request import urlopen, Request, build_opener, HTTPCookieProcessor, HTTPErrorProcessor
from urllib.error import HTTPError, URLError
from urllib.parse import unquote
from http.cookiejar import CookieJar
from urllib.parse import urlencode
import optparse
import codecs
import re
import ssl

class NoRedirection(HTTPErrorProcessor):

    def http_response(self, request, response):
        return response

    https_response = http_response

def main(site, output_file, cookies, redirect_only):
	try:
		site = site.strip()
		req = Request(site, None, {'User-Agent' : 'Mozilla/5.0 (X11; Fedora; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0', 'Accept-Language' : 'en-us,en;q=0.8,he;q=0.5,es;q=0.3', 'Accept' : 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8'})
		if cookies != None:
			output_cookies = ''
			for cookie in cookies:
				if len(output_cookies) > 0:
					output_cookies = output_cookies + '; '
				name, value = cookie
				output_cookies = output_cookies + name + '=' + value
			req.add_header('Cookie', output_cookies)
		if redirect_only == True:
			cj = CookieJar()
			opener = build_opener(NoRedirection, HTTPCookieProcessor(cj))
			data = ()
			req.get_method = lambda: "GET"
			output = opener.open(req, data)
		else:
			output = urlopen(req)
		headers = output.info()
		code = output.code
		output = output.read()
		matchObj = re.search( r'(<meta [^>]*?charset=\"?([^\">]*?)\")', output.decode(), flags = re.M + re.I + re.S)
		if matchObj is not None and matchObj.group(2):
			output = output.decode(matchObj.group(2))
		else:
			try:
				matchObj = re.search( r'(.*charset=([^;]*))', headers['Content-Type'], flags = re.M + re.I + re.S)
				output = output.decode(matchObj.group(2))
			except:
				output = output.decode('ISO-8859-1')
		output_file = unquote(output_file)
		if code in [301, 302, 303, 307, 308] and redirect_only is True:
		    print("Writing " + site + " to " + output_file)
		if not (redirect_only is True and code not in [301, 302, 303, 307, 308]):
			file = codecs.open(output_file, "w", "utf-8")
			file.write(output)
			file.close()
		if code in [301, 302, 303, 307, 308] and redirect_only is True:
			return headers['Location']
	except HTTPError as e:
		print('The server couldn\'t fulfill the request. For ' + site)
		print('Error code: ', e.code)
	except URLError as e:
		print('Skipping URI ' + site)
		print('Reason: ', e.reason)
	except UnicodeDecodeError as e:
		print(site + ': ')
		print(e)
	except Exception as e:
		print(e)

def __main__():
	try:
		p = optparse.OptionParser(description='Grabs Web Pages and Writes Out UTF-8 Files',
			prog='url2utf8.py',
			version='url2utf8 0.1',
			usage='%prog SITE SAVE_FILE')

		options, arguments = p.parse_args()

#	main(arguments[0], arguments[1])
	except IndexError:
		print()
