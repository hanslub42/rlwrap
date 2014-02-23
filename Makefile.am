## Process this file with automake to produce Makefile.in


EXTRA_DIST = BUGS COPYING TODO INSTALL README PLEA src/rlwrap.h src/redblack.h src/malloc_debug.h\
             src/completion.rb doc/rlwrap.man.in test/testclient test/testit\
             completions/testclient completions/coqtop\
             filters/README filters/RlwrapFilter.pm filters/RlwrapFilter.3pm filters/count_in_prompt filters/pipeto\
             filters/logger filters/null filters/unbackspace filters/pipeline filters/ftp_filter filters/history_format\
             filters/simple_macro filters/template filters/scrub_prompt filters/paint_prompt filters/censor_passwords filters/listing



TESTS      = test/testit
SUBDIRS    = doc src filters

nobase_pkgdata_DATA  = completions/testclient completions/coqtop\
		       filters/README filters/RlwrapFilter.pm filters/RlwrapFilter.3pm filters/count_in_prompt filters/pipeto\
                       filters/logger filters/null filters/unbackspace filters/pipeline filters/ftp_filter filters/history_format\
                       filters/simple_macro filters/template filters/scrub_prompt filters/paint_prompt filters/censor_passwords filters/listing



clean-local: 
	rm -f *~ 


install-data-hook:
	chmod a+x $(DESTDIR)$(pkgdatadir)/filters/* 
