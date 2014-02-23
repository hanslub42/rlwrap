man_MANS = rlwrap.1

# EXTRA_DIST = $(man_MANS)

CLEANFILES = $(man_MANS)

rlwrap.1: rlwrap.man
	sed -e 's#@DATADIR@#$(datadir)#'  rlwrap.man > rlwrap.1

