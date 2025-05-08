bin_PROGRAMS = rlwrap 

rlwrap_SOURCES =  main.c signals.c readline.c pty.c completion.c term.c ptytty.c  utils.c string_utils.c malloc_debug.c multibyte.c filter.c ../configure


AM_CFLAGS=-DDATADIR=\"@datadir@\" 

# use old-fashioned suffix rule for non-GNU make
.rb.c :
	rbgen $< $@

.rb.c:
   @[ $$? != 127 ] ||  echo "You don't seem to have rbgen (http://libredblack.sourceforge.net/). If you didn't change any .rb files, this is not a problem"   
 


