
CONFIGDIR=@sysconfdir@
PKGLIBDIR=@pkglibdir@
MODULESDIR=$(pkglibdir)/
SERVICESDIR=$(pkglibdir)/
LOGDIR=$(localstatedir)/log/

do_subst=sed -e 's%[@]SYSCONFDIR[@]%$(CONFIGDIR)%g' \
	     -e 's%[@]PACKAGE_STRING[@]%$(PACKAGE_STRING)%g'


manpages = fhs_findtolearn.8 fhs_makepreload.8 fnb_learn.8 fhs_judge.8 \
           fnb_findtolearn.8 fnb_makepreload.8 fhs_learn.8 fnb_judge.8

manpages_src = $(manpages:.8=.8.in)

CLEANFILES = $(manpages)

SUFFIXES =  .8.in .8

.8.in.8:
	$(do_subst) < $< > $@

man_MANS = $(manpages)

EXTRA_DIST = $(manpages_src)
