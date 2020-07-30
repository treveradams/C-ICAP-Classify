#!/usr/bin/python
# coding=UTF-8
# coding: utf-8
# -*- coding: utf-8 -*-

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

import os
import optparse
import operator
import re
import math
from enum import IntEnum

data_set_name = u'Internet Classification Data'
non_maturity_categories = {"adult.artnudes" : 1, "childfriendly" : 1, "government" : 1,}

lang_codes = { "aa":"Afar",
"aar":"Afar",
"ab":"Abkhazian",
"abk":"Abkhazian",
"ace":"Achinese",
"ach":"Acoli",
"ada":"Adangme",
"ady":"Adyghe; Adygei",
"ae":"Avestan",
"afa":"Afro-Asiatic languages",
"af":"Afrikaans",
"afh":"Afrihili",
"afr":"Afrikaans",
"ain":"Ainu",
"aka":"Akan",
"ak":"Akan",
"akk":"Akkadian",
"alb":"Albanian",
"ale":"Aleut",
"alg":"Algonquian languages",
"alt":"Southern Altai",
"am":"Amharic",
"amh":"Amharic",
"an":"Aragonese",
"ang":"English, Old (ca.450-1100)",
"anp":"Angika",
"apa":"Apache languages",
"ara":"Arabic",
"ar":"Arabic",
"arc":"Official Aramaic (700-300 BCE); Imperial Aramaic (700-300 BCE)",
"arg":"Aragonese",
"arm":"Armenian",
"arn":"Mapudungun; Mapuche",
"arp":"Arapaho",
"art":"Artificial languages",
"arw":"Arawak",
"as":"Assamese",
"asm":"Assamese",
"ast":"Asturian; Bable; Leonese; Asturleonese",
"ath":"Athapascan languages",
"aus":"Australian languages",
"ava":"Avaric",
"av":"Avaric",
"ave":"Avestan",
"awa":"Awadhi",
"ay":"Aymara",
"aym":"Aymara",
"az":"Azerbaijani",
"aze":"Azerbaijani",
"ba":"Bashkir",
"bad":"Banda languages",
"bai":"Bamileke languages",
"bak":"Bashkir",
"bal":"Baluchi",
"bam":"Bambara",
"ban":"Balinese",
"baq":"Basque",
"bar":"Bavarian",
"bas":"Basa",
"bat":"Baltic languages",
"bcl":"Central Bikol",
"be":"Belarusian",
"bej":"Beja; Bedawiyet",
"bel":"Belarusian",
"bem":"Bemba",
"ben":"Bengali",
"ber":"Berber languages",
"bg":"Bulgarian",
"bh":"Bihari languages",
"bho":"Bhojpuri",
"bi":"Bislama",
"bih":"Bihari languages",
"bik":"Bikol",
"bin":"Bini; Edo",
"bis":"Bislama",
"bla":"Siksika",
"bm":"Bambara",
"bn":"Bengali",
"bnt":"Bantu languages",
"bod":"Tibetan",
"bos":"Bosnian",
"bo":"Tibetan",
"bra":"Braj",
"br":"Breton",
"bre":"Breton",
"bs":"Bosnian",
"btk":"Batak languages",
"bua":"Buriat",
"bug":"Buginese",
"bul":"Bulgarian",
"bur":"Burmese",
"bxr":"Buriat",
"byn":"Blin; Bilin",
"ca":"Catalan; Valencian",
"cad":"Caddo",
"cai":"Central American Indian languages",
"car":"Galibi Carib",
"cat":"Catalan; Valencian",
"cau":"Caucasian languages",
"ceb":"Cebuano",
"ce":"Chechen",
"cel":"Celtic languages",
"ces":"Czech",
"cdo":"Min Dong (Eastern Min Chinese)",
"cha":"Chamorro",
"chb":"Chibcha",
"ch":"Chamorro",
"che":"Chechen",
"chg":"Chagatai",
"chi":"Chinese",
"chk":"Chuukese",
"chm":"Mari",
"chn":"Chinook jargon",
"cho":"Choctaw",
"chp":"Chipewyan; Dene Suline",
"chr":"Cherokee",
"chu":"Church Slavic; Old Slavonic; Church Slavonic; Old Bulgarian; Old Church Slavonic",
"chv":"Chuvash",
"chy":"Cheyenne",
"ckb":"Kurdish, Central",
"cmc":"Chamic languages",
"co":"Corsican",
"cop":"Coptic",
"cor":"Cornish",
"cos":"Corsican",
"cpe":"Creoles and pidgins, English based",
"cpf":"Creoles and pidgins, French-based",
"cpp":"Creoles and pidgins, Portuguese-based",
"cr":"Cree",
"cre":"Cree",
"crh":"Crimean Tatar; Crimean Turkish",
"crp":"Creoles and pidgins",
"csb":"Kashubian",
"cs":"Czech",
"cu":"Church Slavic; Old Slavonic; Church Slavonic; Old Bulgarian; Old Church Slavonic",
"cus":"Cushitic languages",
"cv":"Chuvash",
"cym":"Welsh",
"cy":"Welsh",
"cze":"Czech",
"da":"Danish",
"dak":"Dakota",
"dan":"Danish",
"dar":"Dargwa",
"day":"Land Dayak languages",
"de":"German",
"del":"Delaware",
"den":"Slave (Athapascan)",
"deu":"German",
"dgr":"Dogrib",
"din":"Dinka",
"div":"Divehi; Dhivehi; Maldivian",
"dje":"Zarma",
"doi":"Dogri",
"dra":"Dravidian languages",
"dsb":"Lower Sorbian",
"dua":"Duala",
"dum":"Dutch, Middle (ca.1050-1350)",
"dut":"Dutch; Flemish",
"dv":"Divehi; Dhivehi; Maldivian",
"dyu":"Dyula",
"dz":"Dzongkha",
"dzo":"Dzongkha",
"ee":"Ewe",
"efi":"Efik",
"egy":"Egyptian (Ancient)",
"eka":"Ekajuk",
"el":"Greek, Modern (1453-)",
"ell":"Greek, Modern (1453-)",
"elx":"Elamite",
"eml":"Emiliano-Romagnolo",
"en":"English",
"eng":"English",
"enm":"English, Middle (1100-1500)",
"eo":"Esperanto",
"epo":"Esperanto",
"es":"Spanish; Castilian",
"est":"Estonian",
"et":"Estonian",
"eu":"Basque",
"eus":"Basque",
"ewe":"Ewe",
"ewo":"Ewondo",
"fan":"Fang",
"fao":"Faroese",
"fa":"Persian",
"fas":"Persian",
"fat":"Fanti",
"ff":"Fulah",
"fi":"Finnish",
"fij":"Fijian",
"fil":"Filipino; Pilipino",
"fin":"Finnish",
"fiu":"Finno-Ugrian languages",
"fj":"Fijian",
"fkv":"Finnish, Kven",
"fo":"Faroese",
"fon":"Fon",
"fra":"French",
"fre":"French",
"fr":"French",
"frm":"French, Middle (ca.1400-1600)",
"fro":"French, Old (842-ca.1400)",
"frp":"Arpitan/Francoprovencial",
"frr":"Northern Frisian",
"frs":"Eastern Frisian",
"fry":"Western Frisian",
"ful":"Fulah",
"fur":"Friulian",
"fy":"Western Frisian",
"gaa":"Ga",
"ga":"Irish",
"gan":"Gan Chinese",
"gay":"Gayo",
"gba":"Gbaya",
"gd":"Gaelic; Scottish Gaelic",
"gem":"Germanic languages",
"geo":"Georgian",
"ger":"German",
"gez":"Geez",
"gil":"Gilbertese",
"gla":"Gaelic; Scottish Gaelic",
"gle":"Irish",
"gl":"Galician",
"glg":"Galician",
"glk":"Gilaki",
"glv":"Manx",
"gmh":"German, Middle High (ca.1050-1500)",
"gn":"Guarani",
"goh":"German, Old High (ca.750-1050)",
"gon":"Gondi",
"gor":"Gorontalo",
"got":"Gothic",
"grb":"Grebo",
"grc":"Greek, Ancient (to 1453)",
"gre":"Greek, Modern (1453-)",
"grn":"Guarani",
"gsw":"Swiss German; Alemannic; Alsatian",
"gu":"Gujarati",
"guj":"Gujarati",
"gv":"Manx",
"gwi":"Gwich'in",
"ha":"Hausa",
"hai":"Haida",
"hak":"Hakka Chinese",
"hat":"Haitian; Haitian Creole",
"hau":"Hausa",
"haw":"Hawaiian",
"heb":"Hebrew",
"he":"Hebrew",
"her":"Herero",
"hi":"Hindi",
"hil":"Hiligaynon",
"him":"Himachali languages; Western Pahari languages",
"hin":"Hindi",
"hit":"Hittite",
"hmn":"Hmong; Mong",
"hmo":"Hiri Motu",
"hmr":"Hmar",
"ho":"Hiri Motu",
"hr":"Croatian",
"hrv":"Croatian",
"hsb":"Upper Sorbian",
"ht":"Haitian; Haitian Creole",
"hu":"Hungarian",
"hun":"Hungarian",
"hup":"Hupa",
"hy":"Armenian",
"hye":"Armenian",
"hz":"Herero",
"ia":"Interlingua (International Auxiliary Language Association)",
"iba":"Iban",
"ibo":"Igbo",
"ice":"Icelandic",
"id":"Indonesian",
"ido":"Ido",
"ie":"Interlingue; Occidental",
"ig":"Igbo",
"iii":"Sichuan Yi; Nuosu",
"ii":"Sichuan Yi; Nuosu",
"ijo":"Ijo languages",
"ik":"Inupiaq",
"iku":"Inuktitut",
"ile":"Interlingue; Occidental",
"ilo":"Iloko",
"ina":"Interlingua (International Auxiliary Language Association)",
"inc":"Indic languages",
"ind":"Indonesian",
"ine":"Indo-European languages",
"inh":"Ingush",
"io":"Ido",
"ipk":"Inupiaq",
"ira":"Iranian languages",
"iro":"Iroquoian languages",
"is":"Icelandic",
"isl":"Icelandic",
"ita":"Italian",
"it":"Italian",
"iu":"Inuktitut",
"ja":"Japanese",
"jav":"Javanese",
"jbo":"Lojban",
"jpn":"Japanese",
"jpr":"Judeo-Persian",
"jrb":"Judeo-Arabic",
"jv":"Javanese",
"kaa":"Kara-Kalpak",
"kab":"Kabyle",
"kac":"Kachin; Jingpho",
"ka":"Georgian",
"kal":"Kalaallisut; Greenlandic",
"kam":"Kamba",
"kan":"Kannada",
"kar":"Karen languages",
"kas":"Kashmiri",
"kat":"Georgian",
"kau":"Kanuri",
"kaw":"Kawi",
"kaz":"Kazakh",
"kbd":"Kabardian",
"kek":"Kekchi",
"kff":"Koya",
"kg":"Kongo",
"kha":"Khasi",
"khi":"Khoisan languages",
"khm":"Central Khmer",
"kho":"Khotanese; Sakan",
"ki":"Kikuyu; Gikuyu",
"kik":"Kikuyu; Gikuyu",
"kin":"Kinyarwanda",
"kir":"Kirghiz; Kyrgyz",
"kj":"Kuanyama; Kwanyama",
"kk":"Kazakh",
"kl":"Kalaallisut; Greenlandic",
"kmb":"Kimbundu",
"km":"Central Khmer",
"kn":"Kannada",
"kok":"Konkani",
"ko":"Korean",
"koi":"Komi-Permyak",
"kom":"Komi",
"kon":"Kongo",
"kor":"Korean",
"kos":"Kosraean",
"kpe":"Kpelle",
"krc":"Karachay-Balkar",
"kr":"Kanuri",
"krl":"Karelian",
"kro":"Kru languages",
"kru":"Kurukh",
"ksh":u'Kölsch, Ripuarian',
"ks":"Kashmiri",
"kua":"Kuanyama; Kwanyama",
"ku":"Kurdish",
"kum":"Kumyk",
"kur":"Kurdish",
"kut":"Kutenai",
"kv":"Komi",
"kw":"Cornish",
"ky":"Kirghiz; Kyrgyz",
"lad":"Ladino",
"lah":"Lahnda",
"la":"Latin",
"lam":"Lamba",
"lao":"Lao",
"lat":"Latin",
"lav":"Latvian",
"lb":"Luxembourgish; Letzeburgesch",
"lbe":"Lak",
"lez":"Lezghian",
"lg":"Ganda",
"li":"Limburgan; Limburger; Limburgish",
"lim":"Limburgan; Limburger; Limburgish",
"lin":"Lingala",
"lit":"Lithuanian",
"ln":"Lingala",
"lo":"Lao",
"lol":"Mongo",
"loz":"Lozi",
"lt":"Lithuanian",
"ltg":"Latgalian",
"ltz":"Luxembourgish; Letzeburgesch",
"lua":"Luba-Lulua",
"lub":"Luba-Katanga",
"lug":"Ganda",
"lui":"Luiseno",
"lu":"Luba-Katanga",
"lun":"Lunda",
"luo":"Luo (Kenya and Tanzania)",
"lus":"Lushai",
"lv":"Latvian",
"mac":"Macedonian",
"mad":"Madurese",
"mag":"Magahi",
"mah":"Marshallese",
"mai":"Maithili",
"mak":"Makasar",
"mal":"Malayalam",
"man":"Mandingo",
"mao":"Maori",
"map":"Austronesian languages",
"mar":"Marathi",
"mas":"Masai",
"may":"Malay",
"mdf":"Moksha",
"mdr":"Mandar",
"men":"Mende",
"mga":"Irish, Middle (900-1200)",
"mg":"Malagasy",
"mh":"Marshallese",
"mic":"Mi'kmaq; Micmac",
"mi":"Maori",
"min":"Minangkabau",
"mis":"Uncoded languages",
"mkd":"Macedonian",
"mkh":"Mon-Khmer languages",
"mk":"Macedonian",
"mlg":"Malagasy",
"ml":"Malayalam",
"mlt":"Maltese",
"mnc":"Manchu",
"mni":"Manipuri",
"mn":"Mongolian",
"mno":"Manobo languages",
"moh":"Mohawk",
"mon":"Mongolian",
"mos":"Mossi",
"mri":"Maori",
"mr":"Marathi",
"mrw":"Maranao",
"msa":"Malay",
"ms":"Malay",
"mt":"Maltese",
"mul":"Multiple languages",
"mun":"Munda languages",
"mus":"Creek",
"mwl":"Mirandese",
"mwr":"Marwari",
"mya":"Burmese",
"my":"Burmese",
"myn":"Mayan languages",
"myv":"Erzya",
"mzn":"Mazanderani",
"nah":"Nahuatl languages",
"nai":"North American Indian languages",
"na":"Nauru",
"nan":"Nanggu",
"nap":"Neapolitan",
"nau":"Nauru",
"nav":"Navajo; Navaho",
"nbl":"Ndebele, South; South Ndebele",
"nb":u'Bokmål, Norwegian; Norwegian Bokmål',
"nde":"Ndebele, North; North Ndebele",
"nd":"Ndebele, North; North Ndebele",
"ndo":"Ndonga",
"nds":"Low German; Low Saxon; German, Low; Saxon, Low",
"ne":"Nepali",
"nep":"Nepali",
"new":"Nepal Bhasa; Newari",
"ng":"Ndonga",
"nia":"Nias",
"nic":"Niger-Kordofanian languages",
"niu":"Niuean",
"nld":"Dutch; Flemish",
"nl":"Dutch; Flemish",
"nn":"Norwegian Nynorsk; Nynorsk, Norwegian",
"nno":"Norwegian Nynorsk; Nynorsk, Norwegian",
"nob":u'Bokmål, Norwegian; Norwegian Bokmål',
"nog":"Nogai",
"non":"Norse, Old",
"no":"Norwegian",
"nor":"Norwegian",
"nqo":"N'Ko",
"nr":"Ndebele, South; South Ndebele",
"nso":"Pedi; Sepedi; Northern Sotho",
"nub":"Nubian languages",
"nv":"Navajo; Navaho",
"nwc":"Classical Newari; Old Newari; Classical Nepal Bhasa",
"nya":"Chichewa; Chewa; Nyanja",
"ny":"Chichewa; Chewa; Nyanja",
"nym":"Nyamwezi",
"nyn":"Nyankole",
"nyo":"Nyoro",
"nzi":"Nzima",
"oci":"Occitan (post 1500)",
"oc":"Occitan (post 1500)",
"oji":"Ojibwa",
"oj":"Ojibwa",
"om":"Oromo",
"ori":"Oriya",
"orm":"Oromo",
"or":"Oriya",
"osa":"Osage",
"os":"Ossetian; Ossetic",
"oss":"Ossetian; Ossetic",
"ota":"Turkish, Ottoman (1500-1928)",
"oto":"Otomian languages",
"paa":"Papuan languages",
"pag":"Pangasinan",
"pal":"Pahlavi",
"pam":"Pampanga; Kapampangan",
"pan":"Panjabi; Punjabi",
"pa":"Panjabi; Punjabi",
"pap":"Papiamento",
"pau":"Palauan",
"pck":"Paite",
"pdc":"Pennsylvania German",
"peo":"Persian, Old (ca.600-400 B.C.)",
"per":"Persian",
"phi":"Philippine languages",
"phn":"Phoenician",
"pi":"Pali",
"pih":"Pitcairn-Norfolk",
"pli":"Pali",
"pl":"Polish",
"pms":"Piedmontese",
"pnt":"Pontic",
"pol":"Polish",
"pon":"Pohnpeian",
"por":"Portuguese",
"pot":"Potawatomi",
"pra":"Prakrit languages",
"pro":"Provençal, Old (to 1500);Occitan, Old (to 1500)",
"prs":"Persian-Dari",
"ps":"Pushto; Pashto",
"pt":"Portuguese",
"pus":"Pushto; Pashto",
"qaa-qtz":"Reserved for local use",
"quc":"Qatzijob'al",
"que":"Quechua",
"qu":"Quechua",
"raj":"Rajasthani",
"rap":"Rapanui",
"rar":"Rarotongan; Cook Islands Maori",
"rm":"Romansh",
"rn":"Rundi",
"roa":"Romance languages",
"roh":"Romansh",
"rom":"Romany",
"ron":"Romanian; Moldavian; Moldovan",
"ro":"Romanian; Moldavian; Moldovan",
"rum":"Romanian; Moldavian; Moldovan",
"run":"Rundi",
"rup":"Aromanian; Arumanian; Macedo-Romanian",
"ru":"Russian",
"rus":"Russian",
"rw":"Kinyarwanda",
"sad":"Sandawe",
"sag":"Sango",
"sah":"Yakut",
"sai":"South American Indian languages",
"sal":"Salishan languages",
"sam":"Samaritan Aramaic",
"san":"Sanskrit",
"sa":"Sanskrit",
"sas":"Sasak",
"sat":"Santali",
"scn":"Sicilian",
"sco":"Scots",
"sc":"Sardinian",
"sd":"Sindhi",
"sel":"Selkup",
"sem":"Semitic languages",
"se":"Northern Sami",
"sga":"Irish, Old (to 900)",
"sgn":"Sign Languages",
"sg":"Sango",
"sgs":"Samogitian",
"shn":"Shan",
"sh":"Serbo-Croatian",
"sid":"Sidamo",
"sin":"Sinhala; Sinhalese",
"sio":"Siouan languages",
"si":"Sinhala; Sinhalese",
"sit":"Sino-Tibetan languages",
"sk":"Slovak",
"sla":"Slavic languages",
"slk":"Slovak",
"slo":"Slovak",
"sl":"Slovenian",
"slv":"Slovenian",
"sma":"Southern Sami",
"sme":"Northern Sami",
"smi":"Sami languages",
"smj":"Lule Sami",
"smn":"Inari Sami",
"smo":"Samoan",
"sm":"Samoan",
"sms":"Skolt Sami",
"sna":"Shona",
"snd":"Sindhi",
"snk":"Soninke",
"sn":"Shona",
"sog":"Sogdian",
"som":"Somali",
"son":"Songhai languages",
"so":"Somali",
"sot":"Sotho, Southern",
"spa":"Spanish; Castilian",
"sq":"Albanian",
"sqi":"Albanian",
"srd":"Sardinian",
"srn":"Sranan Tongo",
"srp":"Serbian",
"srr":"Serer",
"sr":"Serbian",
"ssa":"Nilo-Saharan languages",
"ss":"Swati",
"ssw":"Swati",
"st":"Sotho, Southern",
"stq":"Saterland Frisian",
"suk":"Sukuma",
"sun":"Sundanese",
"sus":"Susu",
"su":"Sundanese",
"sux":"Sumerian",
"sv":"Swedish",
"swa":"Swahili",
"swe":"Swedish",
"sw":"Swahili",
"syc":"Classical Syriac",
"syr":"Syriac",
"szl":"Silesian",
"tah":"Tahitian",
"tai":"Tai languages",
"tam":"Tamil",
"ta":"Tamil",
"tat":"Tatar",
"tel":"Telugu",
"tem":"Timne",
"ter":"Tereno",
"te":"Telugu",
"tet":"Tetum",
"tgk":"Tajik",
"tgl":"Tagalog",
"tg":"Tajik",
"tha":"Thai",
"th":"Thai",
"tib":"Tibetan",
"tig":"Tigre",
"tir":"Tigrinya",
"ti":"Tigrinya",
"tiv":"Tiv",
"tkl":"Tokelau",
"tk":"Turkmen",
"tlh":"Klingon; tlhIngan-Hol",
"tli":"Tlingit",
"tl":"Tagalog",
"tmh":"Tamashek",
"tn":"Tswana",
"tog":"Tonga (Nyasa)",
"ton":"Tonga (Tonga Islands)",
"to":"Tonga (Tonga Islands)",
"tpi":"Tok Pisin",
"tr":"Turkish",
"tsi":"Tsimshian",
"tsn":"Tswana",
"tso":"Tsonga",
"ts":"Tsonga",
"tt":"Tatar",
"tuk":"Turkmen",
"tum":"Tumbuka",
"tup":"Tupi languages",
"tur":"Turkish",
"tut":"Altaic languages",
"tvl":"Tuvalu",
"twi":"Twi",
"tw":"Twi",
"ty":"Tahitian",
"tyv":"Tuvinian",
"udm":"Udmurt",
"uga":"Ugaritic",
"ug":"Uighur; Uyghur",
"uig":"Uighur; Uyghur",
"ukr":"Ukrainian",
"uk":"Ukrainian",
"uma":"Umatilla",
"umb":"Umbundu",
"und":"Undetermined",
"urd":"Urdu",
"ur":"Urdu",
"uzb":"Uzbek",
"uz":"Uzbek",
"vai":"Vai",
"vec":"Venetian",
"ven":"Venda",
"ve":"Venda",
"vep":"Veps; Vepsian",
"vie":"Vietnamese",
"vi":"Vietnamese",
"vls":"Vlaams",
"vol":"Volapük",
"vot":"Votic",
"vo":"Volapük",
"vro":u'Võro',
"wak":"Wakashan languages",
"wal":"Wolaitta; Wolaytta",
"war":"Waray",
"was":"Washo",
"wa":"Walloon",
"wel":"Welsh",
"wen":"Sorbian languages",
"wln":"Walloon",
"wol":"Wolof",
"wo":"Wolof",
"wuu":"Wu Chinese",
"xal":"Kalmyk; Oirat",
"xho":"Xhosa",
"xh":"Xhosa",
"xmf":"Mingrelian",
"yao":"Yao",
"yap":"Yapese",
"yid":"Yiddish",
"yi":"Yiddish",
"yor":"Yoruba",
"yo":"Yoruba",
"ypk":"Yupik languages",
"yue":"Yue Chinese",
"zap":"Zapotec",
"za":"Zhuang; Chuang",
"zbl":"Blissymbols; Blissymbolics; Bliss",
"zea":"Zeelandic",
"zen":"Zenaga",
"zha":"Zhuang; Chuang",
"zh":"Chinese",
"zho":"Chinese",
"znd":"Zande languages",
"zul":"Zulu",
"zun":"Zuni",
"zu":"Zulu",
"zxx":"No linguistic content; Not applicable",
"zza":"Zaza; Dimili; Dimli; Kirdki; Kirmanjki; Zazaki" }

# _buildDic, buildDic and lookUp routines are NOT under the license of the rest of this file
# They are believed to be in the Public Domain
def _buildDic(keyList, val, dic):
    """
    If performance becomes a problem this function
    would benefit from stackless python.

    """
    if len(keyList)==1:
        dic[keyList[0]]=val
        return

    newDic=dic.get(keyList[0],{})
    dic[keyList[0]]=newDic
    _buildDic(keyList[1:], val, newDic)


def buildDic(buf, dic=None):
    """
    """
    if dic==None:
        dic={}

    lines= string.split(buf,'\n')
    for l in lines:
        key=string.split(l)
        if len(key) < 2:
            continue
        keyList=string.split(key[0],':')
        _buildDic(keyList,string.join(key[1:]),dic)

    return dic

def lookUp(dic,key):
    """
    """
        # Not sure about using "." and ":" for separators
    keys=string.split(key,'.')
    val=dic
    for k in keys:
        val= val[k]
    return val

def walklevel(some_dir, level=0):
    some_dir = some_dir.rstrip(os.path.sep)
    assert os.path.isdir(some_dir)
    num_sep = some_dir.count(os.path.sep)
    for root, dirs, files in os.walk(some_dir):
        yield root, dirs, files
        num_sep_this = root.count(os.path.sep)
        if num_sep + level <= num_sep_this:
            del dirs[:]


p = optparse.OptionParser(description=' Creates statistics on training data',
	prog='report.py',
	version='report 0.1',
	usage='%prog TOP_LEVEL_DIRECTORY')

options, arguments = p.parse_args()

languages = dict()
categories = dict()
language_maturity = dict()

testreg = re.compile("(.*?)[_-].*")
for root, dirs, files in walklevel(arguments[0]):
	for dir_name in dirs:
		if dir_name == "possible" or dir_name == "maybe" or dir_name == "nml":
			continue
		for root2, dirs2, files2 in walklevel(root + '/' + dir_name):
			for name in files2:
				test = testreg.match(name)
				if test and not name.startswith("."):
					if test.group(1) == '':
						continue
					try:
						if languages[test.group(1)]:
							val = languages[test.group(1)]
							try:
								val[dir_name] = val[dir_name] + 1
							except:
								val[dir_name] = 1
#							print(test.group(1), dir_name, val[dir_name])
#							languages[test.group(1)] = val
						else:
							languages[test.group(1)].update(dict([(dir_name, 1)]))
					except KeyError:
							languages[test.group(1)] = dict([(dir_name, 1)])
					try:
						if categories[dir_name]:
							val = categories[dir_name]
							try:
								val[test.group(1)] = val[test.group(1)] + 1
							except KeyError:
								val[test.group(1)] = 1
#							print(dir_name, test.group(1), val[test.group(1)])
#							categories[dir_name] = val
						else:
							categories[dir_name] = dict([(test.group(1), 1)])
					except KeyError:
							categories[dir_name] = dict([(test.group(1), 1)])


sorted_languages = languages.keys()
sorted_languages.sort()

sorted_categories = categories.keys()
sorted_categories.sort()

print("<html><head><meta charset=\"UTF-8\" /><title>%s Training Statistics</title>" % data_set_name)

print("<link href=\"style/sortableTable.css\" rel=\"stylesheet\" type=\"text/css\" />")

print("<script type=\"text/javascript\" src=\"scripts/mootools-core-1.4.0-full-compat-yc.js\"></script>")
print("<script type=\"text/javascript\" src=\"scripts/sortableTable.js\"></script>")
print("</head><body>")

print("<P>Languages:</P>")
for language in sorted_languages:
	print("<a href=\"#" + language + "\">" + lang_codes[language].encode('utf-8') + "</a>")

print("<P>Executive Summary of Languages (Guessed Maturity):</P>")
for language in sorted_languages:
	print("<a href=\"#LM_" + language + "\">" + lang_codes[language].encode('utf-8') + "</a>")

print("<P>Categories:</P>")
for category in sorted_categories:
	print("<a href=\"#" + category + "\">" + category + "</a>")

print("<P>Languages:</P>")

class Maturity(IntEnum):
	zilch = 0
	prealpha = 1
	prealpha2 = 2
	prealpha3 = 3
	alpha = 4
	alpha2 = 5
	alpha3 = 6
	beta = 7
	beta2 = 8
	beta3 = 9

	def describe(self):
		# self is the member here
		return self.name

for language in sorted_languages:
	cat_item=0
	how_mature=Maturity.beta3
	sorted_category = languages[language].keys()
	sorted_category.sort()
	print("<P><table><thead><th><a name=\"" + language + "\">%s</a></th></thead></table>" % lang_codes[language].encode('utf-8'))
	print("<table id=\"" + language + "\" cellpadding=\"0\"><thead>")
	print("<th axis=\"string\">Category Name</th><th axis=\"number\">Relative Training Level (1-3 is good)</th></thead><tbody>")
	for category in sorted_category:
		training_level = math.log10(languages[language][category])
		print("<tr id=\"%d\"><td>%s</td><td>%0.02f</td></tr>" % (cat_item, category, training_level))
		cat_item = cat_item + 1
		try:
			if(non_maturity_categories[category]):
#				print("skipping " + category + " for language " + language)
				pass
		except:
			if how_mature > Maturity.alpha and training_level < .30:
				how_mature = Maturity.alpha
			elif how_mature > Maturity.alpha2 and training_level < .48:
				how_mature = Maturity.alpha2
			elif how_mature > Maturity.alpha3 and training_level < .6:
				how_mature = Maturity.alpha3
			elif how_mature > Maturity.beta and training_level < .78:
				how_mature = Maturity.beta
			elif how_mature > Maturity.beta2 and training_level < .9:
				how_mature = Maturity.beta2
	total_categories = len(categories.keys()) - len(non_maturity_categories.keys())
	language_total_categories = len(languages[language].keys())
	for category in categories:
		if 	language_total_categories < total_categories:
			categories_covered = float(language_total_categories) / float(total_categories)
			if categories_covered > .33:
				try:
					if languages[language][category]:
						pass
					else:
						print("<tr id=\"%d\"><td>%s</td><td>-1</td></tr>" % (cat_item, category))
						how_mature = Maturity.prealpha2
				except KeyError:
						print("<tr id=\"%d\"><td>%s</td><td>-1</td></tr>" % (cat_item, category))
						how_mature = Maturity.prealpha2
			elif language_total_categories <= 3 and categories_covered < .1:
				how_mature = Maturity.zilch
			else:
				how_mature = Maturity.prealpha
			if categories_covered >= .90:
				how_mature = Maturity.alpha3
			elif categories_covered >= .75:
				how_mature = Maturity.alpha2
			elif categories_covered >= .65:
				how_mature = Maturity.alpha
			elif categories_covered >= .50:
				how_mature = Maturity.prealpha3
	language_maturity[language] = how_mature
	print("</tbody><tfoot><tr><td colspan=\"2\">Guessed Maturity Level: " + str(how_mature.describe()) + "</td></tr></tfoot></table>")
	print("<script type=\"text/javascript\"> var myTable = {}; window.addEvent('domready', function(){ myTable = new sortableTable('%s', {overCls: 'over', onClick: function(){alert(this.id)}});});</script></P>" % language)

print("<P>Categories:</P>")

for category in sorted_categories:
	lang_item=0
	sorted_languages = categories[category].keys()
	sorted_languages.sort()
	print("<P><table><thead><th><a name=\"" + category + "\">%s</a></th></thead></table>" % category)
	print("<table id=\"" + category + "\" cellpadding=\"0\" cellpadding=\"0\"><thead>")
	print("<th axis=\"string\">Language Name</th><th axis=\"number\">Relative Training Level (1-3 is good)</th></thead><tbody>")
	for language in sorted_languages:
		print("<tr id=\"%d\"><td>%s</td><td>%0.02f</td></tr>" % (lang_item, lang_codes[language].encode('utf-8'),math.log10(categories[category][language])))
		lang_item = lang_item + 1
	print("</tbody><tfoot><tr><td></td><td></td></tr></tfoot></table>")
	print("<script type=\"text/javascript\"> var myTable = {}; window.addEvent('domready', function(){ myTable = new sortableTable('%s', {overCls: 'over', onClick: function(){alert(this.id)}});});</script></P>" % category)

print("<P><table><thead><th><a name=\"LM\">Language Maturity</a></th></thead></table>")
print("<table id=\"LM\" cellpadding=\"0\" cellpadding=\"0\"><thead>")
print("<th axis=\"string\">Language Name</th><th axis=\"string\">Guessed Maturity Level</th></thead><tbody>")
lang_item=0
for language in language_maturity:
	print("<tr id=\"%d\"><td><a name=\"LM_%s\">%s</a></td><td>%s</td></tr>" % (lang_item, language, lang_codes[language].encode('utf-8'), language_maturity[language].describe()))
	lang_item = lang_item + 1
print("</tbody><tfoot><tr><td></td><td></td></tr></tfoot></table>")
print("<script type=\"text/javascript\"> var myTable = {}; window.addEvent('domready', function(){ myTable = new sortableTable('LM', {overCls: 'over', onClick: function(){alert(this.id)}});});</script></P>")

print("<P>Understanding the above table: Prealpha is less than 1/3 of the categories having samples that train. Prealpha2 is up to 1/2 of the categories having samples that train. Prealpha 3 is up to 65%, Alpha, Alpha2, and Alpha3 are 75%, 90% and 100% respectively. If all categories are trained, a different logic kicks in that has to do with the number of the samples (as quality can only be measured by empirical testing). Prealpha may not even be useful as a proof of concept, depending on which categories are trained and how many. Prealpha2 and 3 will generally begin to show promise. Each additional sample will make a large difference. Alpha and Beta will change quickly, in general, with every additional sample. If you are considering using Alpha live, it may be worth a go. Beta most definitely should see at least some limited live testing. Beta here may actually be production quality. Again, the only way to know the quality of the results is empirical testing. Zilch means there are three or less categories populated; more work needs to be done.</P>")

print("<P>Categories excluded from maturity (likely to be removed due to difficulty in training, using, or ambiguity):<br/>")
for key, value in non_maturity_categories.items():
	print(key + "<br/>")
print("</P>")

print("</body></html>")
