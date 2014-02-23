bin_PROGRAMS = rlwrap 
#EXTRA_PROGRAMS=rltest1 rltest2 rltest3 kaboom
rlwrap_SOURCES =  main.c signals.c readline.c pty.c completion.c term.c ptytty.c  utils.c string_utils.c malloc_debug.c filter.c
#rltest1_SOURCES=rltest1.c
#rltest2_SOURCES=rltest2.c
#rltest3_SOURCES=rltest3.c
#kaboom_SOURCES=kaboom.c


AM_CFLAGS=-DDATADIR=\"@datadir@\" 

# use old-fashioned suffix rule for non-GNU make
.rb.c :
	rbgen $< $@ 

#clean-local: 
#	rm -f *~ kaboom rltest1 rltest2 rltest3


#all-local: kaboom rltest1 rltest2 rltest3


