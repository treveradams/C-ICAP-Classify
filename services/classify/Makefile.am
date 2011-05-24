
INCLUDES = -I$(top_srcdir)
CONFIGDIR = @CONFDIR@

cicapmoduledir=@MODSDIR@
cicapmodule_LTLIBRARIES=srv_classify.la

srv_classify_la_LIBADD = @MODULES_LIBADD@
srv_classify_la_CFLAGS=  -I../../include/ -std=gnu99
srv_classify_la_LDFLAGS=  -module -avoid-version -lm -ltre
srv_classify_la_SOURCES = srv_classify.c bayes.c hyperspace.c html.c hash.c

#if USERTRE
#srv_classify_la_LIBADD += @trelib@ -ltre
#endif

if USEOPENCV
srv_classify_la_CFLAGS += -I @opencvinc@
srv_classify_la_SOURCES += srv_classify_image.c
srv_classify_la_LIBADD += @OPENCV_ADD_LDFLAG@
endif

EXTRA_DIST = srv_classify.h hyperspace.h currency.h html.h
