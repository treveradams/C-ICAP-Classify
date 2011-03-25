/*
 *  Copyright (C) 2008-2010 Trever L. Adams
 *
 *  This file is part of srv_classify c-icap module and accompanying tools.
 *
 *  srv_classify is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  srv_classify is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */


#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE

#define NOT_CICAP

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <langinfo.h>
#include <wchar.h>
#include <wctype.h>
#include <time.h>

#include "hash.c"
#include "hyperspace.c"

char *judge_file;

int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 7)
	{
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-i INPUT_FILE_TO_JUDGE\n");
		printf("Spaces and case matter.\n");
		return -1;
	}
	for(i=1; i<7; i+=2)
	{
		if(strcmp(argv[i], "-p") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED1);
		else if(strcmp(argv[i], "-s") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED2);
		else if(strcmp(argv[i], "-i") == 0)
		{
			judge_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", judge_file);
		}
	}
/*	printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
	printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
	printf("Learn File: %s\n", judge_file);*/
	return 0;
}

void checkMakeUTF8(void)
{
	setlocale(LC_ALL, "");
	int utf8_mode = (strcmp(nl_langinfo(CODESET), "UTF-8") == 0);
	if(!utf8_mode) setlocale(LC_ALL, "en_US.UTF-8");
}

wchar_t *makeData(char *input_file)
{
int data=0;
struct stat stat_buf;
char *tempData=NULL;
wchar_t *myData=NULL;
int32_t realLen;
	data=open(input_file, O_RDONLY);
	fstat(data, &stat_buf);
	tempData=malloc(stat_buf.st_size+1);
	if(tempData==NULL) exit(-1);
	read(data, tempData, stat_buf.st_size);
	close(data);
	tempData[stat_buf.st_size] = '\0';
	myData=malloc((stat_buf.st_size+1) * sizeof(wchar_t));
	realLen=mbstowcs(myData, tempData, stat_buf.st_size);
	if(realLen!=-1) myData[realLen] = L'\0';
	else printf("Bad character data in %s\n", input_file);
	free(tempData);
	return myData;
}

void loadMassCategories(void)
{
	preLoadHyperSpace("fhs_files/preload.fhs");
	loadHyperSpaceCategory("fhs_files/adult.affairs.fhs", "adult.affairs.fhs");
	loadHyperSpaceCategory("fhs_files/adult.artnudes.fhs", "adult.artnudes.fhs");
	loadHyperSpaceCategory("fhs_files/adult.fhs", "adult.fhs");
	loadHyperSpaceCategory("fhs_files/adult.naturism.fhs", "adult.naturism.fhs");
	loadHyperSpaceCategory("fhs_files/adult.porn.fhs", "adult.porn.fhs");
	loadHyperSpaceCategory("fhs_files/adult.sexuality.fhs", "adult.sexuality.fhs");
	loadHyperSpaceCategory("fhs_files/antisocial.bombmaking.fhs", "antisocial.bombmaking.fhs");
	loadHyperSpaceCategory("fhs_files/antisocial.hacking.fhs", "antisocial.hacking.fhs");
	loadHyperSpaceCategory("fhs_files/antisocial.hate.fhs", "antisocial.hate.fhs");
	loadHyperSpaceCategory("fhs_files/antisocial.spyware.fhs", "antisocial.spyware.fhs");
	loadHyperSpaceCategory("fhs_files/antisocial.violence.fhs", "antisocial.violence.fhs");
	loadHyperSpaceCategory("fhs_files/antisocial.warez.fhs", "antisocial.warez.fhs");
	loadHyperSpaceCategory("fhs_files/childfriendly.fhs", "childfriendly.fhs");
	loadHyperSpaceCategory("fhs_files/clothing.fhs", "clothing.fhs");
	loadHyperSpaceCategory("fhs_files/clothing.intimates.female.fhs", "clothing.intimates.female.fhs");
	loadHyperSpaceCategory("fhs_files/clothing.intimates.male.fhs", "clothing.intimates.male.fhs");
	loadHyperSpaceCategory("fhs_files/clothing.swimsuit.female.fhs", "clothing.swimsuit.female.fhs");
	loadHyperSpaceCategory("fhs_files/clothing.swimsuit.male.fhs", "clothing.swimsuit.male.fhs");
	loadHyperSpaceCategory("fhs_files/commerce.auctions.fhs", "commerce.auctions.fhs");
	loadHyperSpaceCategory("fhs_files/commerce.banking.fhs", "commerce.banking.fhs");
	loadHyperSpaceCategory("fhs_files/commerce.jewelry.fhs", "commerce.jewelry.fhs");
	loadHyperSpaceCategory("fhs_files/commerce.payment.fhs", "commerce.payment.fhs");
	loadHyperSpaceCategory("fhs_files/commerce.shopping.fhs", "commerce.shopping.fhs");
	loadHyperSpaceCategory("fhs_files/downloads.cellphones.fhs", "downloads.cellphones.fhs");
	loadHyperSpaceCategory("fhs_files/downloads.desktopsillies.fhs", "downloads.desktopsillies.fhs");
	loadHyperSpaceCategory("fhs_files/downloads.dialers.fhs", "downloads.dialers.fhs");
	loadHyperSpaceCategory("fhs_files/education.fhs", "education.fhs");
	loadHyperSpaceCategory("fhs_files/e-mail.fhs", "e-mail.fhs");
	loadHyperSpaceCategory("fhs_files/entertainment.fhs", "entertainment.fhs");
	loadHyperSpaceCategory("fhs_files/entertainment.gambling.fhs", "entertainment.gambling.fhs");
	loadHyperSpaceCategory("fhs_files/entertainment.gossip.fhs", "entertainment.gossip.fhs");
	loadHyperSpaceCategory("fhs_files/entertainment.KidsTimeWasting.fhs", "entertainment.KidsTimeWasting.fhs");
	loadHyperSpaceCategory("fhs_files/entertainment.onlinegames.fhs", "entertainment.onlinegames.fhs");
	loadHyperSpaceCategory("fhs_files/entertainment.sports.fhs", "entertainment.sports.fhs");
	loadHyperSpaceCategory("fhs_files/entertainment.vacation.fhs", "entertainment.vacation.fhs");
	loadHyperSpaceCategory("fhs_files/government.fhs", "government.fhs");
	loadHyperSpaceCategory("fhs_files/health.addictions.fhs", "health.addictions.fhs");
	loadHyperSpaceCategory("fhs_files/health.fhs", "health.fhs");
	loadHyperSpaceCategory("fhs_files/health.sexual.fhs", "health.sexual.fhs");
	loadHyperSpaceCategory("fhs_files/information.childcare.fhs", "information.childcare.fhs");
	loadHyperSpaceCategory("fhs_files/information.culinary.fhs", "information.culinary.fhs");
	loadHyperSpaceCategory("fhs_files/information.gardening.fhs", "information.gardening.fhs");
	loadHyperSpaceCategory("fhs_files/information.homerepair.fhs", "information.homerepair.fhs");
	loadHyperSpaceCategory("fhs_files/information.news.fhs", "information.news.fhs");
	loadHyperSpaceCategory("fhs_files/information.pets.fhs", "information.pets.fhs");
	loadHyperSpaceCategory("fhs_files/information.weather.fhs", "information.weather.fhs");
	loadHyperSpaceCategory("fhs_files/instantmessaging.fhs", "instantmessaging.fhs");
	loadHyperSpaceCategory("fhs_files/intoxicants.BeerLiquor.fhs", "intoxicants.BeerLiquor.fhs");
	loadHyperSpaceCategory("fhs_files/intoxicants.Drugs.fhs", "intoxicants.Drugs.fhs");
	loadHyperSpaceCategory("fhs_files/jobsearch.fhs", "jobsearch.fhs");
	loadHyperSpaceCategory("fhs_files/objectionable.HomoBiSexuality.fhs", "objectionable.HomoBiSexuality.fhs");
	loadHyperSpaceCategory("fhs_files/personalfinance.fhs", "personalfinance.fhs");
	loadHyperSpaceCategory("fhs_files/personalfinance.investing.fhs", "personalfinance.investing.fhs");
	loadHyperSpaceCategory("fhs_files/radio.fhs", "radio.fhs");
	loadHyperSpaceCategory("fhs_files/realestate.fhs", "realestate.fhs");
	loadHyperSpaceCategory("fhs_files/religion.fhs", "religion.fhs");
	loadHyperSpaceCategory("fhs_files/social.blogs.fhs", "social.blogs.fhs");
	loadHyperSpaceCategory("fhs_files/social.dating.fhs", "social.dating.fhs");
	loadHyperSpaceCategory("fhs_files/social.reconnect.fhs", "social.reconnect.fhs");
	loadHyperSpaceCategory("fhs_files/social.telephony.fhs", "social.telephony.fhs");
	loadHyperSpaceCategory("fhs_files/weapons.fhs", "weapons.fhs");
}

int main (int argc, char *argv[])
{
regexHead myRegexHead;
wchar_t *myData;
HashList myHashes;
clock_t start, end;
hsClassification classification;
	checkMakeUTF8();
	initHTML();
	initHyperSpaceClassifier();
	if(readArguments(argc, argv)==-1) exit(-1);
	myData=makeData(judge_file);

	printf("Loading hashes -- be patient!\n");
	loadMassCategories();

	printf("Classifying\n");
	start=clock();
	mkRegexHead(&myRegexHead, myData);
	removeHTML(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);
	normalizeCurrency(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);

//	printf("%ld: %.*ls\n", myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.main_memory);

	myHashes.hashes = malloc(sizeof(hyperspaceFeature) * HYPERSPACE_MAX_FEATURE_COUNT);
	myHashes.slots = HYPERSPACE_MAX_FEATURE_COUNT;
	myHashes.used = 0;
	computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);

	classification=doHSPrepandClassify(&myHashes);
	end=clock();
	printf("Classification took %lf milliseconds\n", (double)((end-start)/(CLOCKS_PER_SEC/1000)));
	printf("Best match: %s prob: %lf pR: %lf\n", classification.name, classification.probability, classification.probScaled);

	free(myHashes.hashes);
	freeRegexHead(&myRegexHead);
	free(judge_file);
	deinitHyperSpaceClassifier();
	deinitHTML();
	return 0;
}
