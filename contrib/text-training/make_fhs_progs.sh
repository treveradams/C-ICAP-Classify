cp c-icap-classify/services/classify/fhs_* .
cp c-icap-classify/services/classify/fnb_* .
cp c-icap-classify/services/classify/hyperspace.[ch] .
cp c-icap-classify/services/classify/bayes.[ch] .
cp c-icap-classify/services/classify/currency.h .
cp c-icap-classify/services/classify/html.[ch] .
cp c-icap-classify/services/classify/htmlentities.h .
cp c-icap-classify/services/classify/hash.[ch] .
cp c-icap-classify/services/classify/findtolearn_util.c .
cp c-icap-classify/services/classify/train_common.[ch] .
cp c-icap-classify/services/classify/train_common_threads.[ch] .

gcc -g -O2 --std=gnu99 -DHAVE_LIBICU -DNOT_CICAP html.c -c -o html.o -DTRAINER `icu-config --ldflags`
gcc -g -O2 --std=gnu99 -DHAVE_LIBICU -DNOT_CICAP train_common.c -c -o train_common.o `icu-config --ldflags`
gcc -g -O2 --std=gnu99 -DHAVE_LIBICU -DNOT_CICAP hash.c -c -o hash.o `icu-config --ldflags`
gcc -g -O2 --std=gnu99 -DHAVE_LIBICU -DNOT_CICAP train_common_threads.c -c -o train_common_threads.o `icu-config --ldflags`
#gcc -g -O2 --std=gnu99 -DHAVE_LIBICU fhs_judge.c -ltre html.o train_common.o -lm -o fhs_judge -Wall `icu-config --ldflags`
#gcc -g -O2 --std=gnu99 -DHAVE_LIBICU -DTRAINER fhs_learn.c -ltre html.o train_common.o -lm -o fhs_learn -Wall `icu-config --ldflags`
#gcc -g -O2 --std=gnu99 -DHAVE_LIBICU fhs_findtolearn.c -ltre train_common.o train_common_threads.o html.o -lpthread -lm -o fhs_findtolearn -Wall -DNOT_AUTOCONF `icu-config --ldflags`
#gcc -g -O2 --std=gnu99 -DHAVE_LIBICU -DTRAINER fhs_makepreload.c -ltre html.o -lm -o fhs_makepreload -Wall `icu-config --ldflags`
gcc -g -O2 --std=gnu99 -DHAVE_LIBICU fnb_judge.c -ltre html.o train_common.o -lm -o fnb_judge -Wall `icu-config --ldflags`
gcc -g -O2 --std=gnu99 -DHAVE_LIBICU -DTRAINER fnb_learn.c -ltre html.o train_common.o train_common_threads.o -lpthread -lm -o fnb_learn -Wall -DNOT_AUTOCONF `icu-config --ldflags`
gcc -g -O2 --std=gnu99 -DHAVE_LIBICU -DTRAINER fnb_findtolearn.c -ltre train_common.o train_common_threads.o html.o -lpthread -lm -o fnb_findtolearn -Wall -DNOT_AUTOCONF `icu-config --ldflags`
gcc -g -O2 --std=gnu99 -DHAVE_LIBICU -DTRAINER fnb_makepreload.c -ltre html.o -lm -o fnb_makepreload -Wall `icu-config --ldflags`
#gcc -g -O2 --std=gnu99 -DHAVE_LIBICU htmlentities_test.c -lm -o htmlentities_test `icu-config --ldflags`

rm -rf *.o
