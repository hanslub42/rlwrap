## Process this file with automake to produce Makefile.in



EXTRA_DIST = BUGS COPYING INSTALL README.md  src/rlwrap.h src/redblack.h src/malloc_debug.h\
             src/completion.rb doc/rlwrap.man.in test/testclient test/testit\
             completions/testclient completions/coqtop\
             filters/README filters/RlwrapFilter.pm filters/RlwrapFilter.3pm filters/count_in_prompt filters/pipeto\
             filters/logger filters/null filters/unbackspace filters/pipeline filters/ftp_filter filters/handle_hotkeys filters/history_format\
             filters/simple_macro filters/template filters/scrub_prompt filters/paint_prompt filters/censor_passwords filters/listing\
             filters/paint_prompt.py filters/handle_hotkeys.py filters/logger.py filters/pipeto.py\
             filters/rlwrapfilter.py filters/null.py filters/censor_passwords.py filters/edit_history\
             filters/count_in_prompt.py filters/ftp_filter.py  filters/debug_null filters/handle_sigwinch filters/outfilter\
             filters/makefilter filters/dissect_prompt



TESTS      = test/testit
SUBDIRS    = doc src filters

nobase_pkgdata_DATA  = completions/testclient completions/coqtop\
		       filters/README filters/RlwrapFilter.pm filters/RlwrapFilter.3pm filters/count_in_prompt filters/pipeto\
                       filters/logger filters/null filters/unbackspace filters/pipeline filters/ftp_filter filters/handle_hotkeys filters/history_format\
                       filters/simple_macro filters/template filters/scrub_prompt filters/paint_prompt filters/censor_passwords filters/listing\
                       filters/paint_prompt.py filters/handle_hotkeys.py filters/logger.py filters/pipeto.py\
                       filters/rlwrapfilter.py filters/null.py filters/censor_passwords.py filters/edit_history\
                       filters/count_in_prompt.py filters/ftp_filter.py  filters/debug_null filters/handle_sigwinch filters/outfilter\
                       filters/makefilter filters/dissect_prompt



clean-local:
	rm -f *~


install-data-hook:
	chmod a+x $(DESTDIR)$(pkgdatadir)/filters/*

