
INCLUDES = -I$(top_srcdir)
CONFIGDIR = @CONFDIR@
cicapdatadir=@DATADIR@
cicapmoduledir=@MODSDIR@
cicapmodule_LTLIBRARIES=srv_classify.la

srv_classify_la_LIBADD = @MODULES_LIBADD@
srv_classify_la_CFLAGS=  -I../../include/ -std=gnu99
srv_classify_la_LDFLAGS=  -module -avoid-version -lm -ltre
srv_classify_la_SOURCES = srv_classify.c bayes.c hyperspace.c html.c hash.c

bin_PROGRAMS = fhs_judge fhs_learn fhs_makepreload fnb_judge fnb_learn fnb_makepreload
# fhs_findtolearn fnb_findtolearn 

fhs_judge_SOURCES = fhs_judge.c html.c train_common.c
fhs_judge_CFLAGS = -DNOT_CICAP -std=gnu99
fhs_judge_LDFLAGS = -ltre -lm

fhs_learn_SOURCES = fhs_learn.c html.c train_common.c
fhs_learn_CFLAGS = -DNOT_CICAP -std=gnu99
fhs_learn_LDFLAGS = -ltre -lm

fhs_findtolearn_SOURCES = fhs_findtolearn.c html.c train_common.c train_common_threads.c
fhs_findtolearn_CFLAGS = -DNOT_CICAP -std=gnu99
fhs_findtolearn_LDFLAGS = -ltre -lm

fhs_makepreload_SOURCES = fhs_makepreload.c html.c
fhs_makepreload_CFLAGS = -DNOT_CICAP -std=gnu99
fhs_makepreload_LDFLAGS = -ltre -lm

fnb_judge_SOURCES = fnb_judge.c html.c train_common.c
fnb_judge_CFLAGS = -DNOT_CICAP -std=gnu99
fnb_judge_LDFLAGS = -ltre -lm

fnb_learn_SOURCES = fnb_learn.c html.c train_common.c train_common_threads.c
fnb_learn_CFLAGS = -DNOT_CICAP -std=gnu99
fnb_learn_LDFLAGS = -ltre -lm -lpthread

fnb_findtolearn_SOURCES = fnb_findtolearn.c html.c train_common.c train_common_threads.c
fnb_findtolearn_CFLAGS = -DNOT_CICAP -std=gnu99
fnb_findtolearn_LDFLAGS = -ltre -lm -lpthread

fnb_makepreload_SOURCES = fnb_makepreload.c html.c
fnb_makepreload_CFLAGS = -DNOT_CICAP -std=gnu99
fnb_makepreload_LDFLAGS = -ltre -lm

#if USERTRE
#srv_classify_la_LIBADD += @trelib@ -ltre
#endif

if USEOPENCV
srv_classify_la_CFLAGS += -I @opencvinc@
srv_classify_la_SOURCES += srv_classify_image.c
srv_classify_la_LIBADD += @OPENCV_ADD_LDFLAG@
endif

EXTRA_DIST = srv_classify.h hyperspace.h currency.h html.h
