/* main.c: main(), initialisation and cleanup
 * (C) 2000-2009 Hans Lub
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License , or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  You may contact the author by e-mail:  hanslub42@gmail.com
 */

#include "rlwrap.h"

/* global vars */



/* variables set via command line options */
int always_readline = FALSE;	             /* -a option: always be in readline mode             */
char *password_prompt_search_string = NULL;  /* (part of) password prompt (argument of -a option) */
int ansi_colour_aware = FALSE;               /* -A option: make readline aware of ANSI colour codes in prompt */
int bleach_the_prompt = FALSE;               /* -A!: remove all ANSI colour codes in prompt      */
int complete_filenames = FALSE;	             /* -c option: whether to complete file names        */
int debug = 0;			             /* -d option: debugging mask                        */
char *extra_char_after_completion = " ";     /* -e option: override readlines's default completion_append_char (space) */
int always_echo = FALSE;                     /* -E option: always echo, even if client has ECHO off */
int history_duplicate_avoidance_policy =  ELIMINATE_SUCCESIVE_DOUBLES;               /* -D option: whether and how to avoid duplicate history entries */
char *history_format = NULL;                 /* -F option: format to append to history entries            */
char *forget_regexp = NULL;                  /* -g option: keep matching input out of history           */
int pass_on_sigINT_as_sigTERM =  FALSE;      /* -I option: send a SIGTERM to client when a SIGINT is received */
char *multi_line_tmpfile_ext = NULL;         /* -M option: tmpfile extension for multi-line editor */
int nowarn = FALSE;		             /* -n option: suppress warnings */
int commands_children_not_wrapped =  FALSE;  /* -N option: always use direct mode when <command> is waiting */
int one_shot_rlwrap = FALSE;                 /* -o option: whether to close the pty after writing the first line to <command> */
char *prompt_regexp = NULL;		     /* -O option: only ever "cook" prompts matching this regexp */
bool regexp_means_prompt = FALSE;            /* -O! all candidate prompts that match rhe regexp are prompts */
int colour_the_prompt = FALSE;	             /* -p option: whether we should paint the prompt */
int renice = FALSE;                          /* -R option: whether to be nicer than command */
int mirror_arguments = FALSE;                /* -U option: whether to mirror command's arguments */
int wait_before_prompt =  40;	             /* -w option: how long we wait before deciding we have a cookable prompt (in msec)) */
int polling = FALSE;                         /* -W option: always give select() a small (=wait_before_prompt) timeout. */
int impatient_prompt = TRUE;                 /* show raw prompt as soon as possible, even before we cook it. may result in "flashy" prompt */
char *substitute_prompt = NULL;              /* -S option: substitute our own prompt for <command>s */
char *filter_command = NULL;                 /* -z option: pipe prompts, input, output, history and completion requests through an external filter */


/* variables for global bookkeeping */
int master_pty_fd;		             /* master pty (rlwrap uses this to communicate with client) */
FILE *debug_fp = NULL;  	             /* filehandle of debugging log */
char *program_name = "rlwrap", *command_name;/* "rlwrap" (or whatever has been symlinked to rlwrap) and (base-)name of command */
char *rlwrap_command_line = "";              /* rlwrap command line (rlwrap -options command <command_args> */
char *command_line = "";                     /* command <command_args> */
int within_line_edit = FALSE;	             /* TRUE while user is editing input */
int screen_is_alternate = FALSE;             /* TRUE after client has sent smcup, FALSE after rmcup */
pid_t command_pid = 0;		             /* pid of child (client), or 0 before child is born */
int i_am_child = FALSE;		             /* Am I child or parent? after forking, child will set this to TRUE */
int i_am_filter = FALSE;		     /* After forking, filter will set this to TRUE */
int ignore_queued_input = FALSE;             /* read and then ignore all characters in input queue until it is empty (i.e. read would block) */
int received_WINCH = FALSE;                  /* flag set in SIGWINCH signal handler: start line edit as soon as possible */
int prompt_is_still_uncooked = TRUE;         /* The main loop consults this variable to determine the select() timeout: when TRUE, it is
                                                a few millisecs, if FALSE, it is infinite.						
                                                TRUE just after receiving command output (when we still don't know whether we have a
                                                prompt), and, importantly, at startup (so that substitute prompts get displayed even with
                                                programs that don't have a startup message, such as cat)  */
						
int we_just_got_a_signal_or_EOF = FALSE;    /* When we got a signal or EOF, and the program sends something that ends in a newline, take it
					       as a response to user input - i.e. preserve a cooked prompt and just print the new output after it */
int rlwrap_already_prompted = FALSE;
int accepted_lines =  0; /* number of lines accepted (used for one-shot rlwrap) */


/* private variables */
static char *history_filename = NULL;
static int  histsize = 300;
static int  write_histfile = TRUE;
static char *completion_filename, *default_completion_filename;
static char *full_program_name;
static int  last_option_didnt_have_optional_argument = FALSE;
static int  last_opt = -1;
static char *client_term_name = NULL; /* we'll set TERM to this before exec'ing client command */
static int feed_history_into_completion_list = FALSE;


/*
 * Since version 0.24, rlwrap only writes to master_pty
 * asynchronously, keeping a queue of pending output. The readline
 * line handler calls put_in_output_queue(user_input) , while
 * main_loop calls flush_output_queue() as long as there is something
 * in the queue.
 */

static char *output_queue; /* NULL when empty */


/* private functions */
static void init_rlwrap(char *command_line);
static void fork_child(char *command_name, char **argv);
static char *read_options_and_command_name(int argc, char **argv);
static void main_loop(void);



/* options */
#ifdef GETOPT_GROKS_OPTIONAL_ARGS
static char optstring[] = "+:a::A::b:cC:d::D:e:Ef:F:g:hH:iIl:nNM:m::oO:p::P:q:rRs:S:t:TUvw:Wz:";
/* +: is not really documented. configure checks wheteher it works as expected
   if not, GETOPT_GROKS_OPTIONAL_ARGS is undefined. @@@ */
#else
static char optstring[] = "+:a:A:b:cC:d:D:e:Ef:F:g:hH:iIl:nNM:m:oO:p:P:q:rRs:S:t:TUvw:Wz:";	
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] = {
  {"always-readline", 		optional_argument, 	NULL, 'a'},
  {"ansi-colour-aware",         optional_argument,      NULL, 'A'},
  {"break-chars", 		required_argument, 	NULL, 'b'},
  {"complete-filenames", 	no_argument, 		NULL, 'c'},
  {"command-name", 		required_argument, 	NULL, 'C'},
  {"debug", 			optional_argument, 	NULL, 'd'},
  {"extra-char-after-completion", required_argument,    NULL, 'e'},
  {"always-echo",               no_argument,            NULL, 'E'},
  {"history-no-dupes", 		required_argument, 	NULL, 'D'},
  {"file", 			required_argument, 	NULL, 'f'},
  {"history-format", 		required_argument, 	NULL, 'F'},
  {"forget-matching",           required_argument, 	NULL, 'g'},
  {"help", 			no_argument, 		NULL, 'h'},
  {"history-filename", 		required_argument, 	NULL, 'H'},
  {"case-insensitive", 		no_argument, 		NULL, 'i'},
  {"pass-sigint-as-sigterm",    no_argument,            NULL, 'I'},
  {"logfile", 			required_argument, 	NULL, 'l'},
  {"multi-line", 		optional_argument, 	NULL, 'm'},
  {"multi-line-ext",            required_argument,      NULL, 'M'},
  {"no-warnings", 		no_argument, 		NULL, 'n'},
  {"no-children",               no_argument,            NULL, 'N'},
  {"one-shot",                  no_argument,            NULL, 'o'},
  {"only-cook",                 required_argument,      NULL, 'O'},
  {"prompt-colour",             optional_argument,      NULL, 'p'},
  {"pre-given", 		required_argument, 	NULL, 'P'},
  {"quote-characters",          required_argument,      NULL, 'q'},
  {"remember", 			no_argument, 		NULL, 'r'},
  {"renice", 			no_argument, 		NULL, 'R'},
  {"histsize", 			required_argument, 	NULL, 's'},
  {"substitute-prompt",    	required_argument, 	NULL, 'S'},           
  {"set-terminal-name",         required_argument,      NULL, 't'},        
  {"test-terminal",  		no_argument, 		NULL, 'T'},
  {"mirror-arguments",          no_argument, 		NULL, 'U'},
  {"version", 			no_argument, 		NULL, 'v'},
  {"wait-before-prompt",        required_argument,      NULL, 'w'},    
  {"polling",                   no_argument,            NULL, 'W'},
  {"filter",                    required_argument,      NULL, 'z'}, 
  {0, 0, 0, 0}
};
#endif



/* helper function to run a unit test whenever UNIT_TEST is #defined, e.g. by "make clean; make CFLAGS='-DUNIT_TEST=my_test'" */

#ifdef UNIT_TEST
  static void run_unit_test(int argc, char **argv, test_stage stage) {
    extern void UNIT_TEST(int argc, char **argv, test_stage stage);
    #define VALUE_AS_STRING(v) NAME_AS_STRING(v)
    #define NAME_AS_STRING(n) #n
  
    if(stage == TEST_AT_PROGRAM_START)
      myerror(WARNING|NOERRNO, "running unit test %s", VALUE_AS_STRING(UNIT_TEST)); 
    UNIT_TEST(argc, argv, stage);
  }
#else
  static void run_unit_test(int UNUSED(argc), char ** UNUSED(argv), test_stage UNUSED(stage)) {
    /*do nothing */
  }
#endif
/*
 * main function. initialises everything and calls main_loop(),
 * which never returns
 */

int
main(int argc, char **argv)
{
  char *command_name;

  if (!(setlocale (LC_ALL, "") && setlocale(LC_COLLATE, "C")))/* ANSI C says that all programs start by default in the standard `C' locale...   */ 
    myerror(WARNING|NOERRNO, "could not set locale");         /* ... To use the locales specified by the environment, we must call  setlocale.  */
                                                              /* LC_COLLATE = 'C' because we use character ranges  as byte ranges in regexps    */ 
  
  run_unit_test(argc, argv,TEST_AT_PROGRAM_START);
  rlwrap_command_line = unsplit_with(argc, argv, " ");     
  init_completer();

  /* Harvest options and leave optind pointing to first non-option argument: */
  command_name = read_options_and_command_name(argc, argv);


  /* by now, optind points to slave <command>, and &argv[optind] is <command>'s argv. Remember slave command line: */
  command_line = unsplit_with(argc - optind, argv + optind, " ");
  
  run_unit_test(argc - optind, argv + optind, TEST_AFTER_OPTION_PARSING); /* argv points at the first non-option rlwrap argument */

  /* if stdin is not a tty, or we're inside emacs, just execute <command>: */ 
  if (!isatty(STDIN_FILENO) || getenv("INSIDE_EMACS"))
    if (execvp(argv[optind], &argv[optind]) < 0)
      myerror(FATAL|USE_ERRNO, "Cannot execute %s", argv[optind]);
  
  init_rlwrap(rlwrap_command_line);
  install_signal_handlers();	
  block_all_signals();
  fork_child(command_name, argv); /* this will unblock most signals most of the time */

  init_readline("");
  last_minute_checks();
  run_unit_test(0,NULL, TEST_AFTER_READLINE_INIT); 

  if (filter_command)
    spawn_filter(filter_command);
  run_unit_test(argc - optind, argv + optind, TEST_AFTER_SPAWNING_SLAVE_COMMAND); /* argv points at the first non-option rlwrap argument */

  main_loop();
  return 42;			/* The Answer, but, sadly, we'll never get there.... */
}


/*
 * create pty pair and fork using my_pty_fork; parent returns immediately; child
 * executes the part of rlwrap's command line that remains after
 * read_options_and_command_name() has harvested rlwrap's own options
 */  
static void
fork_child(char *UNUSED(command_name), char **argv)
{
  char *arg = argv[optind], *p;
  int pid;

  if (mirror_arguments)
    mirror_args_init(&argv[optind]);
  
  pid = my_pty_fork(&master_pty_fd, &saved_terminal_settings, &winsize);
  if (pid > 0)			/* parent: */
    return;
  else {			/* child: */
    DPRINTF1(DEBUG_TERMIO, "preparing to execute %s", arg);
    close_open_files_without_writing_buffers();
    
    if (client_term_name)
      mysetenv("TERM", client_term_name);   
    if (execvp(argv[optind], &argv[optind]) < 0) {
      if (last_opt > 0 && last_option_didnt_have_optional_argument) { /* e.g. 'rlwrap -a Password: sqlpus' will try to exec 'Password:' */
	for (p=" '; !(){}"; *p; p++) /* does arg need shell quoting? */ 
	  if (strchr(arg,*p)) { 
            arg = add3strings("'", arg,"'"); /* quote it  */
            break;
	  }	
	fprintf(stderr, "Did you mean '%s' to be an option argument?\nThen you should write -%c%s, without the space(s)\n",
                argv[optind], last_opt, arg); 
      }
      myerror(FATAL|USE_ERRNO, "Cannot execute %s", argv[optind]);   	/* stillborn child, parent will live on and display child's last gasps */
    }
  }
}


/*
 * main loop: listen on stdin (for user input) and master pty (for command output),
 * and try to write output_queue to master_pty (if it is not empty)
 * This function never returns.
 */
void
main_loop()
{				
  int nfds;			
  fd_set readfds;	
  fd_set writefds;
  int nread;		
  char buf[BUFFSIZE], *timeoutstr, *old_raw_prompt, *new_output_minus_prompt;
  int promptlen = 0;
  int leave_prompt_alone;
  sigset_t no_signals_blocked;
  int seen_EOF = FALSE;     
   
  struct timespec         select_timeout, *select_timeoutptr;
  struct timespec immediately = { 0, 0 }; /* zero timeout when child is dead */
  struct timespec  wait_a_little = {0, 0xBadf00d }; /* tv_usec field will be filled in when initialising */
  struct timespec  *forever = NULL;
  wait_a_little.tv_nsec = 1000 * 1000 * wait_before_prompt;

  
  sigemptyset(&no_signals_blocked);
  


  pass_through_filter(TAG_OUTPUT,""); /* If something is wrong with filter, get the error NOW */
  set_echo(FALSE);		      /* This will also put the terminal in CBREAK mode */
  /* ------------------------------  main loop  -------------------------------*/
  while (TRUE) {
    /* listen on both stdin and pty_fd */
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(master_pty_fd, &readfds);

    /* try to write output_queue to master_pty (but only if it is nonempty) */
    FD_ZERO(&writefds);
    if (output_queue_is_nonempty())
      FD_SET(master_pty_fd, &writefds);



    DPRINTF1(DEBUG_AD_HOC, "prompt_is_still_uncooked =  %d", prompt_is_still_uncooked);
    if (command_is_dead || ignore_queued_input) {
      select_timeout = immediately;
      select_timeoutptr = &select_timeout;
      timeoutstr = "immediately";
    } else if (prompt_is_still_uncooked || polling) {
      select_timeout = wait_a_little;
      select_timeoutptr = &select_timeout;
      timeoutstr = "wait_a_little";
    } else {
      select_timeoutptr = forever; /* NULL */
      timeoutstr = "forever";
    }
     
    DPRINTF2(DEBUG_TERMIO, "calling select() with timeout %s %s ...",  timeoutstr, within_line_edit ? "(within line edit)" : "");
    

    nfds = my_pselect(1 + master_pty_fd, &readfds, &writefds, NULL, select_timeoutptr, &no_signals_blocked);

    /* DPRINTF3(DEBUG_TERMIO, "select() returned  %d (stdin|pty in|pty out = %03d), within_line_edit=%d", nfds, */
    /*          100*(FD_ISSET(STDIN_FILENO, &readfds)?1:0) + 10*(FD_ISSET(master_pty_fd, &readfds)?1:0) + (FD_ISSET(master_pty_fd, &writefds)?1:0),  */
    /*          within_line_edit); */

    
    DPRINTF5(DEBUG_TERMIO, "... returning %d%s %s %s %s"
             , nfds
             , nfds > 0 ? ": " : "."
	     , nfds > 0 && FD_ISSET(STDIN_FILENO, &readfds)   ? "stdin ready for input" : ""
             , nfds > 0 && FD_ISSET(master_pty_fd, &readfds)  ? "pty master ready for input": ""
             , nfds > 0 && FD_ISSET(master_pty_fd, &writefds) ? "output queue nonempty and pty master ready for output" : "");

    assert(!filter_pid || filter_is_dead || kill(filter_pid,0) == 0); 
    assert(command_is_dead || kill(command_pid,0) == 0);
    
    /* check flags that may have been set by signal handlers */
    if (filter_is_dead) 
      filters_last_words(); /* will call myerror with last words */
      	
    if (received_WINCH) {  /* received_WINCH flag means we've had a WINCH while within_line_edit was FALSE */
      DPRINTF0(DEBUG_READLINE, "Starting line edit as a result of WINCH ");
      within_line_edit = TRUE;
      restore_rl_state();
      received_WINCH = FALSE;
      continue;
    }	
    
    if (nfds < 0) {		/* exception  */	
      if (errno == EINTR || errno == 0) {	/* interrupted by signal, or by a cygwin bug (errno == 0) :-( */
	continue;
      }	else
	myerror(FATAL|USE_ERRNO, "select received exception");
    } else if (nfds == 0) {
      
      /* timeout, which can only happen when .. */
      if (ignore_queued_input) {       /* ... we have read all the input keystrokes that should
					  be ignored (i.e. those that accumulated on stdin while we
				          were calling an external editor) */
	ignore_queued_input = FALSE;
	continue;
      } else if (command_is_dead) {                         /* ... or else, if child is dead, ... */
	DPRINTF2(DEBUG_SIGNALS,
		 "select returned 0, command_is_dead=%d, commands_exit_status=%d",
		 command_is_dead, commands_exit_status);
	cleanup_rlwrap_and_exit(EXIT_SUCCESS);
      }	else if (prompt_is_still_uncooked) { /* cooking time? */
	if (we_just_got_a_signal_or_EOF) {
	  we_just_got_a_signal_or_EOF = FALSE;              /* 1. If we got a signal/EOF before cooking time, we don't need special action
                                                                  to preserve the cooked prompt.
							       2. Reset we_just_got_a_signal_or_EOF  after a signal or EOF that didn't kill command */
          continue;
	}	
	if (!skip_rlwrap()) {                        /* ... or else, it is time to cook the prompt */
	  if (pre_given && accepted_lines == 0) {
            /* input_buffer and point have already been set in init_readline() */
	    DPRINTF0(DEBUG_READLINE, "Starting line edit (because of -P option)");
	    within_line_edit = TRUE;
	    restore_rl_state();
	    continue;
	  }
			  
	  move_cursor_to_start_of_prompt(ERASE); /* cooked prompt may be shorter than raw prompt, hence the ERASE */
	  /* move and erase before cooking, as we need to move/erase according
	     to the raw prompt */
          cook_prompt_if_necessary();
	  DPRINTF2(DEBUG_READLINE,"After cooking, raw_prompt=%s, cooked=%s", M(saved_rl_state.raw_prompt), M(saved_rl_state.cooked_prompt));
	  my_putstr(saved_rl_state.cooked_prompt);
	  rlwrap_already_prompted = TRUE;
	}
	prompt_is_still_uncooked = FALSE;
      } else if (polling) {
        completely_mirror_slaves_special_characters();
        if (mirror_arguments)
          mirror_args(command_pid);
        continue;
      } else {
	myerror(FATAL|NOERRNO, "unexpected select() timeout");
      }
    } else if (nfds > 0) {	/* Hah! something to read or write */ 

      /* -------------------------- read pty --------------------------------- */
      /* Always first read and process the slave command's output, even if there is input waiting on stdin 
         (which may happen when pasting a lot of text). E.g. when pasting "a\nb\nc" into "rlwrap cat" we want
         a 
         a
         b
         b
         c
         c

         and not
  
         a
         b
         c
         a
         b
         c
      */ 
      if (FD_ISSET(master_pty_fd, &readfds)) { /* there is something (or nothing, if EOF) to read on master pty: */
        nread = read(master_pty_fd, buf, BUFFSIZE - 1); /* read it */
        DPRINTF1(DEBUG_AD_HOC, "nread: %d", nread);
	if (nread <= 0) { 
	  if (command_is_dead || nread == 0) { /*  we catched a SIGCHLD,  or slave command has closed its stdout */
	    if (promptlen > 0)	/* commands dying words were not terminated by \n ... */
	      my_putchar('\n');	/* provide the missing \n */
	    cleanup_rlwrap_and_exit(EXIT_SUCCESS);
	  } else  if (errno == EINTR) {	/* interrupted by signal ...*/	                     
	    continue;                   /* ... don't worry */
	  } else  if (! seen_EOF) {     /* maybe command has just died (and SIGCHLD, whose handler sets command_is_dead is not  */     
            mymicrosleep(50);           /* yet caught) Therefore we wait a bit,                                                 */
            seen_EOF = TRUE;            /* set a flag                                                                           */   
            continue;                   /* and try one more time (hopefully catching the signal this time round                 */
          } else {
            myerror(FATAL|USE_ERRNO, "read error on master pty"); 
          }
	}
        remove_padding_and_terminate(buf, nread);
	completely_mirror_slaves_output_settings(); /* some programs (e.g. joe) need this. Gasp!! */	
        mirror_args(command_pid);        
	check_cupcodes(buf);
        if (skip_rlwrap()) { /* Race condition here! The client may just have finished an emacs session and
			        returned to cooked mode, while its ncurses-riddled output is stil waiting for us to be processed. */
	  write_patiently(STDOUT_FILENO, buf, nread, "to stdout");

	  DPRINTF2(DEBUG_TERMIO, "read from pty and wrote to stdout  %d  bytes in direct mode  <%s>",  nread, M(buf));
	  yield();
	  continue;
	}

	DPRINTF2(DEBUG_TERMIO, "read %d bytes from pty into buffer: %s", nread, M(buf));
        
        
	write_logfile(buf);
	if (within_line_edit)	/* client output arrives while we're editing keyboard input:  */
	  save_rl_state();      /* temporarily disable readline and restore the screen state before readline was called */
  

	assert(saved_rl_state.raw_prompt != NULL);


        /* We *always* compute the printable part and the new raw prompt, and *always* print the printable part
           There are four possibilities:
           1. impatient before cooking.         The raw prompt has been printed,  write the new output after it
           2. patient before cooking            No raw prompt has been printed yet, don't print anything
           3. impatient after cooking
             3a  no current prompt              print the new output
             3b  some current prompt            erase it, replace by current raw prompt and print new output
           4. patient after cooking             don't print anything
        */
        
        /* sometimes we want to leave the prompt standing, e.g. after accepting a line, or when a signal arrived */
	leave_prompt_alone =
	     *saved_rl_state.raw_prompt == '\0' /* saved_rl_state.raw_prompt = "" in two distinct cases: when there is actually no prompt,
						   or just after accepting a line, when the cursor is at the end of the prompt. In both
						   cases, we dont't want to move the cursor */
          || prompt_is_still_uncooked /* in this case no prompt has been displayed yet */
          || command_is_dead                    
          || (we_just_got_a_signal_or_EOF && strrchr(buf, '\n')); /* a signal followed by output with a newline in it: treat it as
                                                                     response to user input, so leave the prompt alone */

        DPRINTF3(DEBUG_READLINE, "leave_prompt_alone: %s (raw prompt: %s, prompt_is_still_uncooked: %d)",
                 (leave_prompt_alone? "yes" : "no"), M(saved_rl_state.raw_prompt), prompt_is_still_uncooked);
	
        if (!leave_prompt_alone) /* && (!impatient_prompt || !saved_rl_state.cooked_prompt)) */
	  move_cursor_to_start_of_prompt(ERASE);  
	else if (we_just_got_a_signal_or_EOF) {
	  free (saved_rl_state.raw_prompt);
	  saved_rl_state.raw_prompt =  mysavestring(""); /* prevent reprinting the prompt */
	}	

        if (impatient_prompt && !leave_prompt_alone)
          old_raw_prompt =  mysavestring(saved_rl_state.raw_prompt);

        new_output_minus_prompt = process_new_output(buf, &saved_rl_state);	/* chop off the part after the last newline and put this in
										   saved_rl_state.raw_prompt (or append buf if  no newline found)*/

	if (impatient_prompt) {   /* in impatient mode, ALL command output is passed through the OUTPUT filter, including the prompt The
				     prompt, however, is filtered separately at cooking time and then displayed */
	  char *filtered = pass_through_filter(TAG_OUTPUT, buf);
          if(!leave_prompt_alone) {
            my_putstr(old_raw_prompt);
            free(old_raw_prompt);
          }

	  my_putstr(filtered); 
	  free (filtered);
          if (regexp_means_prompt && prompt_regexp && match_regexp(saved_rl_state.raw_prompt, prompt_regexp, FALSE)) {
            /* user specified -O!.... so any natching candidate prompt will be cooked and output immediately: */
            move_cursor_to_start_of_prompt(ERASE);  /* erase already printed raw prompt */
            cook_prompt_if_necessary();             
            my_putstr(saved_rl_state.cooked_prompt);
          }     
            
	  rlwrap_already_prompted = TRUE;
	} else {
	  my_putstr(new_output_minus_prompt);
	  rlwrap_already_prompted = FALSE;
	}	
	   
	free(new_output_minus_prompt);	

		    
	prompt_is_still_uncooked = TRUE; 
       

	if (within_line_edit)
	  restore_rl_state();

	yield();  /* wait for what client has to say .... */ 
	continue; /* ... and don't attempt to process keyboard input as long as it is talking ,
		     in order to avoid re-printing the current prompt (i.e. unfinished output line) */
      }

      
      /* ----------------------------- key pressed: read stdin -------------------------*/
      if (FD_ISSET(STDIN_FILENO, &readfds)) {	/* key pressed */
	unsigned char byte_read;                /* the readline function names and documentation talk about "characters" and "keys",
						   but we're reading bytes (i.e. unsigned chars) here, and those may very well be
						   part of a multi-byte character. Example: hebrew "aleph" in utf-8 is 0xd790; pressing this key
						   will make us read 2 bytes 0x90 and then 0xd7, (or maybe the other way round depending on endianness??)
						   The readline library hides all this complexity and allows one to just "pass the bytes around" */
	nread = read(STDIN_FILENO, &byte_read, 1);  /* read next byte of input   */
	assert(sizeof(unsigned char) == 1);         /* gets optimised away       */

	if (nread <= 0) 
	  DPRINTF1(DEBUG_TERMIO, "read from stdin returned %d", nread); 
	if (nread < 0)
	  if (errno == EINTR)
	    continue;
	  else
	    myerror(FATAL|USE_ERRNO, "Unexpected error");
	else if (nread == 0)	/* EOF on stdin */
	  cleanup_rlwrap_and_exit(EXIT_SUCCESS);
        else if (ignore_queued_input)
	  continue;             /* do nothing with it*/
	assert(nread == 1);
	DPRINTF2(DEBUG_TERMIO, "read from stdin: byte 0x%02x (%s)", byte_read, mangle_char_for_debug_log(byte_read, TRUE)); 
	if (skip_rlwrap()) {	/* direct mode, just pass it on */
	                        /* remote possibility of a race condition here: when the first half of a multi-byte char is read in
				   direct mode and the second half in readline mode. Oh well... */
	  DPRINTF0(DEBUG_TERMIO, "passing it on (in transparent mode)");	
	  completely_mirror_slaves_terminal_settings(); /* this is of course 1 keypress too late: we should
							   mirror the terminal settings *before* the user presses a key.
							   (maybe using rl_event_hook??)   @@@FIXME  @@@ HOW?*/
          write_patiently(master_pty_fd, &byte_read, 1, "to master pty");
	} else {		/* hand it over to readline */
	  if (!within_line_edit) {	/* start a new line edit    */
	    DPRINTF0(DEBUG_READLINE, "Starting line edit");
	    within_line_edit = TRUE;
	    restore_rl_state();
	  } 
	                                        
	  if (term_eof && byte_read == term_eof && strlen(rl_line_buffer) == 0) {	/* hand a term_eof (usually CTRL-D) directly to command */ 
	    char *sent_EOF = mysavestring("?");
	    *sent_EOF = term_eof;
	    put_in_output_queue(sent_EOF);
	    we_just_got_a_signal_or_EOF = TRUE;
	    free(sent_EOF);
	  }	
	  else {
	    rl_stuff_char(byte_read);  /* stuff it back in readline's input queue */
	    DPRINTF0(DEBUG_TERMIO, "passing it to readline");	
	    DPRINTF2(DEBUG_READLINE, "rl_callback_read_char() (_rl_eof_char=%d, term_eof=%d)", _rl_eof_char, term_eof);
            message_in_echo_area(NULL);     
            rl_callback_read_char();
	  }
	}
      }
    
      /* -------------------------- write pty --------------------------------- */
      if (FD_ISSET(master_pty_fd, &writefds)) {
	flush_output_queue();
        if(output_queue) {   /* there was more than one line in the queue - probably pasted input    */
          mymicrosleep(10);  /* give slave some time to respond                                      */
	  yield();           /*  If we woudn't do this, nothing bad would happen, but the            */
                             /*  "dialogue" on screen will be out of order (which can still happen)  */
        }
      }
    }				/* if (ndfs > 0)         */
  }				/* while (1)             */
}				/* void main_loop()      */


/* Read history and completion word lists */
static void
init_rlwrap(char *command_line)
{

  char *homedir, *histdir, *homedir_prefix, *hostname;
  struct stat statbuf;
  time_t now;
  

  hostname = getenv("HOSTNAME") ? getenv("HOSTNAME") : "?";
  now = time(NULL);
  DPRINTF0(DEBUG_ALL, "-*- mode: grep -*-");
  DPRINTF1(DEBUG_ALL, "command line: %s", command_line);
  DPRINTF3(DEBUG_ALL, "rlwrap version %s, host: %s, time: %s", VERSION, hostname, ctime(&now));
  
  init_terminal();

  
  
  /* Determine rlwrap home dir and prefix for default history and completion filenames */
  homedir = (getenv("RLWRAP_HOME") ? getenv("RLWRAP_HOME") : getenv("HOME"));
  if (!homedir) {
    homedir = ".";
    myerror(WARNING | NOERRNO, "No HOME, using '%s'", homedir);
  }
  homedir_prefix = (getenv("RLWRAP_HOME") ?                    /* is RLWRAP_HOME set?                */
		    add2strings(getenv("RLWRAP_HOME"), "/") :  /* use $RLWRAP_HOME/<command>_history */
		    add2strings(homedir, "/."));	       /* if not, use ~/.<command>_history   */

  /* Determine history file name and check its existence and permissions */

  if (history_filename) {
    histdir = mydirname(history_filename);
  } else {
    histdir = homedir;
    history_filename = add3strings(homedir_prefix, command_name, "_history");
  }


  
  if (write_histfile) {
    if (access(history_filename, F_OK) == 0) {	/* already exists, can we read/write it? */
      if (access(history_filename, R_OK | W_OK) != 0) {
	myerror(FATAL|USE_ERRNO, "cannot read and write %s", history_filename);
      }
      /* OK, we can read and write it, but do we want to? Not if our effective UID
        doesn't match the owner of the history file (like after sudo rlwrap on Ubuntu) */
      assert(!stat(history_filename, &statbuf));
      if(statbuf.st_uid != geteuid()) {
        myerror(WARNING | NOERRNO, "Owner of %s and your effective UID don't match. History will be read-only", history_filename);
        write_histfile = FALSE;
      } 
    } else {			        /* doesn't exist, can we create it? */
      if(access(histdir, W_OK) != 0) {
        if (errno == ENOENT) {
          mode_t oldmask = umask(0);
          if (mkdir(histdir, 0700))       /* rwx------ */
            myerror(FATAL|USE_ERRNO, "cannot create directory %s", histdir);
          umask(oldmask);
        } else {
          myerror(FATAL|USE_ERRNO, "cannot create history file %s", history_filename);
        }
      }
    }
  } 

  /* Initialize history */
  using_history();
  stifle_history(histsize);
  read_history(history_filename);	/* ignore errors here: history file may not yet exist, but will be created on exit */

  if (feed_history_into_completion_list)
    feed_file_into_completion_list(history_filename);
  /* Determine completion file name (completion files are never written to,
     and ignored when unreadable or non-existent) */

  completion_filename =
    add3strings(homedir_prefix, command_name, "_completions");
  default_completion_filename =
    add3strings(DATADIR, "/rlwrap/completions/", command_name);

  rl_readline_name = command_name;
  DPRINTF1(DEBUG_READLINE, "Setting readline command name to '%s'", command_name);

  /* Initialise completion list (if <completion_filename> is readable) */
  if (access(completion_filename, R_OK) == 0) {
    feed_file_into_completion_list(completion_filename);
  } else if (access(default_completion_filename, R_OK) == 0) {
    feed_file_into_completion_list(default_completion_filename);
  }

  
}

/*
 * On systems where getopt doens't handle optional argments, warn the user whenever an
 * argument of the form -<letter> is seen, or whenever the argument is the last item on the command line
 * (e.g. 'rlwrap -a command', which will be parsed as 'rlwrap --always-readline=command')
 */

static char *
check_optarg(char opt, int remaining, bool isdummy, const char *suggestion)
{

  MAYBE_UNUSED(opt); MAYBE_UNUSED(remaining);
  
  if (!optarg)
    last_option_didnt_have_optional_argument = TRUE; /* if this variable is set, and  if command is not found,
							suggest that it may have been meant
						        as optional argument (e.g. 'rlwrap -a password sqlplus' will try to
						        execute 'password sqlplus' ) */
#ifndef GETOPT_GROKS_OPTIONAL_ARGS
  if (optarg &&         /* is there an optional arg? have a look at it: */
      ((optarg[0] == '-' && (optarg[1] == '-' || isalpha(optarg[1]))) || /* looks like next option */
       remaining == 0)) /* or is last item on command line */

    myerror(WARNING|NOERRNO, "on this system, the getopt() library function doesn't\n"
       "grok optional arguments, so '%s' is taken as an argument to the -%c option\n"
            "Is this what you meant? If not, please provide %s  argument like '%s'", optarg, opt, isdummy ? "a dummy" : "an", suggestion);
#endif
  
  return optarg;
}


/* find name of current option
 */
static const char *
current_option(int opt, int longindex)
{
  static char buf[BUFFSIZE];
#ifdef HAVE_GETOPT_LONG
  if (longindex >=0) {
    sprintf(buf, "--%s", longopts[longindex].name);
    return buf;
  }	
#endif
  sprintf(buf, "-%c", opt);
  return buf;
}


char *
read_options_and_command_name(int argc, char **argv)
{
  int c;
  char *opt_C = NULL;
  int option_count = 0;
  int opt_b = FALSE;
  int opt_f = FALSE;
  int remaining = -1; /* remaining number of arguments on command line */
  int longindex = -1; /* index of current option in longopts[], set by getopt_long */
  
  
  full_program_name = mysavestring(argv[0]);
  program_name = mybasename(full_program_name);	/* normally "rlwrap"; needed by myerror() */
  rl_basic_word_break_characters = " \t\n\r(){}[],'+-=&^%$#@\";|\\";

  opterr = 0;			/* we do our own error reporting */

  while (1) {
#ifdef HAVE_GETOPT_LONG
    c = getopt_long(argc, argv, optstring, longopts, &longindex);
#else
    c = getopt(argc, argv, optstring);
#endif

    if (c == EOF)
      break;
    option_count++;
    last_option_didnt_have_optional_argument = FALSE;
    remaining = argc - optind;
    last_opt = c;    

    switch (c) {
    case 'a':
      always_readline = TRUE;
      if (check_optarg('a', remaining, TRUE, "brumbleslurgh"))
        password_prompt_search_string = mysavestring(optarg);
      break;
    case 'A':
      ansi_colour_aware = TRUE;
      if (check_optarg('A', remaining, TRUE, " ") && strings_are_equal(optarg, "!"))
          bleach_the_prompt = TRUE;
      break;
    case 'b':
      rl_basic_word_break_characters = add3strings("\r\n \t", optarg, "");
      opt_b = TRUE;
      break;
    case 'c':   complete_filenames = TRUE;
#ifndef CAN_FOLLOW_COMMANDS_CWD
                myerror(WARNING|NOERRNO, "On this system rlwrap cannot follow the rlwrapped command's working directory:\n"
                                         "filename completion will only be relative to rlwrap's own working directory ");
#endif
                break;
    case 'C':	opt_C = mysavestring(optarg); break;
    case 'd':
#ifdef DEBUG
      if (option_count > 1)
        myerror(FATAL|NOERRNO, "-d or --debug option has to be the *first* rlwrap option\n"
                               "in order to be able to follow the processing  of all subsequent options");
      debug = check_optarg('d', remaining, FALSE, "7") ? my_atoi(optarg) : DEBUG_DEFAULT;
      my_fopen(&debug_fp, DEBUG_FILENAME, "w+", "debug log"); /* w+, not w: both parent and child write to the same logfile, and need to fseek beyond where the other may have written stuff */
#else
      myerror(FATAL|NOERRNO, "To use -d( for debugging), configure %s with --enable-debug and rebuild",program_name);
#endif
      break;

    case 'D': 
      history_duplicate_avoidance_policy=my_atoi(optarg);
      if (history_duplicate_avoidance_policy < 0 || history_duplicate_avoidance_policy > 2)
        myerror(FATAL|NOERRNO, "%s option with illegal value %d, should be 0, 1 or 2",
                current_option('D', longindex), history_duplicate_avoidance_policy);
      break;
    case 'e':
      extra_char_after_completion = mysavestring(optarg); 
      if (strlen(extra_char_after_completion) > 1) 
        myerror(FATAL|NOERRNO, "-e (--extra-char-after-completion) argument should be at most one character");
      break;
    case 'E': always_echo = TRUE; break;
    case 'f':
      if (strcmp(optarg, ".") == 0)
        feed_history_into_completion_list =  TRUE;
      else
        feed_file_into_completion_list(optarg);
      opt_f = TRUE;
      break;
    case 'F': WONTRETURN(myerror(FATAL|NOERRNO, "The -F (--history-format) option is obsolete. Use -z \"history_format '%s'\" instead", optarg));
    case 'g': forget_regexp = mysavestring(optarg);  match_regexp("complain NOW if regexp is wrong", forget_regexp, 1); break;
    case 'h': WONTRETURN(usage(EXIT_SUCCESS));	  
    case 'H': history_filename = mysavestring(optarg); break;
    case 'i': 
      if (opt_f)
        myerror(FATAL|NOERRNO, "-i option has to precede -f options");
      completion_is_case_sensitive = FALSE;
      break;
    case 'I':	pass_on_sigINT_as_sigTERM = TRUE; break;
    case 'l':	open_logfile(optarg); break;
    case 'n':	nowarn = TRUE; break;
    case 'm':
#ifndef HAVE_SYSTEM
      myerror(WARNING|NOERRNO, "the -m option doesn't work on this system");
#endif
      multiline_separator = /* \\\\ will be printed as \\ which is correct if we want ' \ ' to be the multiline separator */
        (check_optarg('m', remaining, FALSE, " \\\\ ") ? mysavestring(optarg) : " \\ ");
      break;
    case 'M': multi_line_tmpfile_ext = mysavestring(optarg); break;
    case 'N': commands_children_not_wrapped = TRUE; break;
    case 'o': 
      one_shot_rlwrap = TRUE; break;
    case 'O': prompt_regexp = mysavestring(optarg);
      if (*prompt_regexp == '!') {
        regexp_means_prompt = TRUE;
        prompt_regexp += 1;
      }
      match_regexp("complain NOW if regexp is wrong", prompt_regexp, 1);
      break;
    case 'p':
      colour_the_prompt = TRUE;
      initialise_colour_codes(check_optarg('p', remaining, FALSE, "Red") ?
                              colour_name_to_ansi_code(optarg) :
                              colour_name_to_ansi_code("Red"));
      break;
    case 'P':
      pre_given = mysavestring(optarg);
      always_readline = TRUE; /* pre_given does not work well with transparent mode */
	
      break;
    case 'q': rl_basic_quote_characters = mysavestring(optarg); break;
    case 'r': remember_for_completion = TRUE;	break;
    case 'R': renice = TRUE;	break;
    case 's':
      histsize = my_atoi(optarg);
      if (histsize < 0 || *optarg == '-') {
        write_histfile = 0;
        histsize = -histsize;
      }
      break;
    case 'S': substitute_prompt =  mysavestring(optarg);break;
    case 't': client_term_name=mysavestring(optarg);break;
#ifdef DEBUG
    case 'T': test_terminal(); exit(EXIT_SUCCESS);
#endif
    case 'U': mirror_arguments = TRUE; break;
    case 'v': printf("rlwrap %s\n",  VERSION); exit(EXIT_SUCCESS);
    case 'w':
      wait_before_prompt = my_atoi(optarg);
      if (wait_before_prompt < 0) {
        wait_before_prompt *= -1;
        impatient_prompt =  FALSE;
      }
      break;
    case 'W': 
      polling = TRUE; break;
    case 'z': filter_command = mysavestring(optarg);	break;
    case '?':
      assert(optind > 0);
      WONTRETURN(myerror(FATAL|NOERRNO, "unrecognised option %s\ntry '%s --help' for more information", argv[optind-1], full_program_name));
    case ':':
      assert(optind > 0);
      WONTRETURN(myerror(FATAL|NOERRNO, "option %s requires an argument \ntry '%s --help' for more information", argv[optind-1], full_program_name));

    default:
      usage(EXIT_FAILURE);
    }
  }
  
  if (!complete_filenames && !opt_b) {	/* use / and . as default breaking characters whenever we don't complete filenames */
    rl_basic_word_break_characters =
      add2strings(rl_basic_word_break_characters, "/.");
  }

  
  if (optind >= argc) { /* rlwrap -a -b -c with no command specified */
    if (filter_command) { /* rlwrap -z filter with no command specified */
      mysignal(SIGALRM, HANDLER(handle_sigALRM)); /* needed for read_patiently2 */
      spawn_filter(filter_command);
      pass_through_filter(TAG_OUTPUT,""); /* ignore result but allow TAG_OUTPUT_OUT_OF_BAND */
      cleanup_rlwrap_and_exit(EXIT_SUCCESS);
    } else {
      usage(EXIT_FAILURE); 
    }
  }
  if (opt_C) {
    int countback = 0;
    if (isnumeric(opt_C)) countback = my_atoi(opt_C);	/* investigate whether -C option argument is numeric */

    if (countback > 0) {	/* e.g -C 1 or -C 12 */
      if (argc - countback < optind)	/* -C 666 */
	myerror(FATAL|NOERRNO, "when using -C %d you need at least %d non-option arguments",
		countback, countback);
      else if (argv[argc - countback][0] == '-')	/* -C 2 perl -d blah.pl */
	myerror(FATAL|NOERRNO, "the last argument minus %d appears to be an option!",
		countback);
      else {			/* -C 1 perl test.cgi */
	command_name = mysavestring(mybasename(argv[argc - countback]));

      }
    } else if (countback == 0) {	/* -C name1 name2 or -C 0 */
      if (opt_C[0] == '0' && opt_C[1] == '\0')	/* -C 0 */
	myerror(FATAL|NOERRNO, "-C 0 makes no sense");
      else if (strlen(mybasename(opt_C)) != strlen(opt_C))	/* -C dir/name */
	myerror(FATAL|NOERRNO, "-C option argument should not contain directory components");
      else if (opt_C[0] == '-')	/* -C -d  (?) */
	myerror(FATAL|NOERRNO, "-C option needs argument");
      else			/* -C name */
	command_name = opt_C;
    } else {			/* -C -2 */
      myerror (FATAL|NOERRNO, "-C option needs string or positive number as argument, perhaps you meant -C %d?",
	 -countback);
    }
  } else {			/* no -C option given, use command name */
    command_name = mysavestring(mybasename(argv[optind]));
  }
  assert(command_name != NULL);
  return command_name;
}


int
output_queue_is_nonempty()
{
  return (output_queue ? TRUE : FALSE);
}

void
put_in_output_queue(char *stuff)
{
  
  output_queue = append_and_free_old(output_queue, stuff); 
  DPRINTF3(DEBUG_TERMIO,"put %d bytes in output queue (which now has %d bytes): %s", (int) strlen(stuff), (int) strlen(output_queue), M(stuff));

}


/*
 * flush the output queue, writing its contents to master_pty_fd
 * never write more than one line, or BUFFSIZE in one go
 */

void
flush_output_queue()
{
  int nwritten, queuelen, how_much;
  char *old_queue = output_queue;
  char *nl;

  if (!output_queue)
    return;
  queuelen = strlen(output_queue);
  nl       = strchr(output_queue, '\n');
  how_much = min(BUFFSIZE, nl ? 1+ nl - output_queue : queuelen); /* never write more than one line, and never more than BUFFSIZE in one go */
  nwritten = write(master_pty_fd, output_queue, how_much);

  assert(nwritten <= (int) strlen(output_queue));
  if (debug) {
    char scratch = output_queue[nwritten];
    output_queue[nwritten] = '\0'; /* temporarily replace the last written byte + 1 by a '\0' */
    DPRINTF3(DEBUG_TERMIO,"flushed %d of %d bytes from output queue to pty: %s", nwritten, queuelen, M(output_queue));
    output_queue[nwritten] =  scratch;
  }	
  
  if (nwritten < 0) {
    switch (nwritten) {
    case EINTR:
    case EAGAIN:
      return;
    default:
      myerror(FATAL|USE_ERRNO, "write to master pty failed");
    }
  }

  if (!output_queue[nwritten]) /* nothing left in queue */
    output_queue = NULL;
  else
    output_queue = mysavestring(output_queue + nwritten);	/* this much is left to be written */

  free(old_queue);
}


void
cleanup_rlwrap_and_exit(int status)
{
  unblock_all_signals();
  DPRINTF0(DEBUG_TERMIO, "Cleaning up");

  if (write_histfile && (histsize==0 ||  history_total_bytes() > 0)) /* avoid creating empty .speling_eror_history file after typo */
    write_history(history_filename);	/* ignore errors */

  close_logfile();
  
  DPRINTF4(DEBUG_SIGNALS, "command_pid: %d, commands_exit_status: %x, filter_pid: %d, filters_exit_status: %x",
           command_pid, commands_exit_status, filter_pid, filters_exit_status);
  mymicrosleep(10); /* we may have got an EOF or EPIPE because the filter or command died, but this doesn't mean that
                       SIGCHLD has been caught already. Taking a little nap now improves the chance that we will catch it
                       (no grave problem if we miss it, but diagnostics, exit status and transparent signal handling depend on it) */
  if (filter_pid) 
    kill_filter();
  else if (filter_is_dead) {
    int filters_killer = killed_by(filters_exit_status);
    myerror(WARNING|NOERRNO, (filters_killer ? "filter was killed by signal %d (%s)" : 
                              WEXITSTATUS(filters_exit_status) ? "filter died" : "filter exited"), filters_killer, signal_name(filters_killer));
  }     
  if (debug) 
    debug_postmortem();
  
  if (terminal_settings_saved)
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_terminal_settings) < 0)  /* ignore errors (almost dead anyway) */ 
      { /* nothing ... */ } /* fprintf(stderr, "Arggh\n"); don't use myerror!!*/

  if (!newline_came_last) /* print concluding newline, if necessary */
    my_putstr("\n");
     

  if (status != EXIT_SUCCESS)  /* rlwrap itself has failed, rather than the wrapped command */
    exit(status);              
  else {                      
    int commands_killer = killed_by(commands_exit_status);
    if (commands_killer)
      suicide_by(commands_killer, commands_exit_status); /* command terminated by signal, make rlwrap's
                                                            parent believe rlwrap was killed by it */ 
    else
      exit(WEXITSTATUS(commands_exit_status));           /* propagate command's exit status */
  }
}


