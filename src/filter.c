/* filter.c : spawning a filter and using it to re-write input, output, history and 
 * (C) 2000-2007 Hans Lub
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



/* A filter is an external program run by rlwrap in order to examine
   and possibly re-write user input, command output, prompts, history
   entries and completion requests.  There can be at most one filter,
   but this can be a pipeline ('pipeline filter1 : filter2')
   
   The filter communicates with rlwrap by reading and writing messages
   on two pipes.

   A message is a byte sequence as follows:

   Tag     1 byte (can be TAG_INPUT, TAG_OUTPUT, TAG_HISTORY,
   TAG_COMPLETION, TAG_PROMPT, TAG_OUTPUT_OUT_OF_BAND, TAG_ERROR)
   Length  4 bytes (length of text + closing newline) 
   Text    <Length> bytes 
   '\n'    (so that the filter can be line buffered without
   hanging rlwrap)
            
            
   Communication is synchronous: after sending a message (and only
   then) rlwrap waits for an answer, which must have the same tag, but
   may be preceded by one or more "out of band" messages.  If the
   filter is slow or hangs, rlwrap does the same. A filter can (and
   should) signal an error by using TAG_ERROR (which will terminate
   rlwrap). Filter output on stderr is displayed normally, but will
   mess up the display.
 
   Length may be 0. (Example: If we have a prompt-less command, rlwrap
   will send an empty TAG_PROMPT message, and the filter can send a
   fancy prompt back. The converse is also possible, of course)
   
   A few environment variables are used to inform the filter about
   file descriptors etc.
   
*/



#include "rlwrap.h"




static int filter_input_fd = -1;
static int filter_output_fd = -1;
pid_t filter_pid = 0;
static int expected_tag = -1;




static char*read_from_filter(int tag);
static void write_message(int fd, int tag, const char *string, const char *description);
static void write_to_filter(int tag, const char *string);
static char* tag2description(int tag);
static char *read_tagless(void);
void handle_out_of_band(int tag, char *message);



static void mypipe(int filedes[2]) {
  int retval;
  retval = pipe(filedes);
  if (retval < 0)
    myerror(FATAL|USE_ERRNO, "Couldn't create pipe");
}       


void spawn_filter(const char *filter_command) {
  int input_pipe_fds[2];
  int output_pipe_fds[2];

  mypipe(input_pipe_fds);
  filter_input_fd = input_pipe_fds[1]; /* rlwrap writes filter input to this */ 
  
  mypipe(output_pipe_fds);
  filter_output_fd = output_pipe_fds[0]; /* rlwrap  reads filter output from here */
  DPRINTF1(DEBUG_FILTERING, "preparing to spawn filter <%s>", filter_command);
  assert(!command_pid || signal_handlers_were_installed);  /* if there is a command, then signal handlers are installed */

  fflush(NULL);
  if ((filter_pid = fork()) < 0)
    myerror(FATAL|USE_ERRNO, "Cannot spawn filter '%s'", filter_command); 
  else if (filter_pid == 0) { /* child */
    int signals_to_allow[] = {SIGPIPE, SIGCHLD, SIGALRM, SIGUSR1, SIGUSR2, 0}; 
    char **argv;

    i_am_filter = TRUE;
    my_fopen(&debug_fp, DEBUG_FILENAME, "a+", "debug log");
    unblock_signals(signals_to_allow);  /* when we run a pager from a filter we want to catch these */


    DEBUG_RANDOM_SLEEP;
    /* set environment for filter (it needs to know at least the file descriptors for its input and output) */
    DPRINTF1(DEBUG_FILTERING, "getenv{RLWRAP_FILTERDIR} = <%s>", STR(getenv("RLWRAP_FILTERDIR")));
    
    if ((! getenv("RLWRAP_FILTERDIR")) || (! *getenv("RLWRAP_FILTERDIR")))
      mysetenv("RLWRAP_FILTERDIR", add2strings(DATADIR,"/rlwrap/filters"));
    mysetenv("PATH", add3strings(getenv("RLWRAP_FILTERDIR"),":",getenv("PATH")));
    mysetenv("RLWRAP_VERSION", VERSION);
    mysetenv("RLWRAP_COMMAND_PID",  as_string(command_pid));
    mysetenv("RLWRAP_COMMAND_LINE", command_line); 
    if (impatient_prompt)
      mysetenv("RLWRAP_IMPATIENT", "1");
    mysetenv("RLWRAP_INPUT_PIPE_FD", as_string(input_pipe_fds[0]));
    mysetenv("RLWRAP_OUTPUT_PIPE_FD", as_string(output_pipe_fds[1]));
    mysetenv("RLWRAP_MASTER_PTY_FD", as_string(master_pty_fd));
    mysetenv("RLWRAP_BREAK_CHARS", rl_basic_word_break_characters);
    mysetenv("RLWRAP_DEBUG", as_string(debug));

    
    close(filter_input_fd);
    close(filter_output_fd);

    /* @@@TODO: split the command in words (possibly quoted when containing spaces). DONT use the shell (|, < and > are never used on filter command lines */
    if (scan_metacharacters(filter_command, "'|\"><"))  { /* if filter_command contains shell metacharacters, let the shell unglue them */
      char *exec_command = add3strings("exec", " ", filter_command);
      argv = list4("sh", "-c", exec_command, NULL);
      DPRINTF1(DEBUG_FILTERING, "exec_command = <%s>", exec_command);

    } else {                                              /* if not, split and feed to execvp directly (cheaper, better error message) */
      argv = split_with(filter_command, " ");
    }   
    assert(argv[0]);    
    if(execvp(argv[0], argv) < 0) {
      char *sorry = add3strings("Cannot exec filter '", argv[0], add2strings("': ", strerror(errno))); 
      write_message(output_pipe_fds[1], TAG_ERROR, sorry, "to stdout"); /* this will kill rlwrap */
      mymicrosleep(100 * 1000); /* 100 sec for rlwrap to go away should be enough */
      exit (-1);
    }
    assert(!"not reached");
    
  } else { /* parent */
    DEBUG_RANDOM_SLEEP;
    mysignal(SIGPIPE, SIG_IGN, NULL); /* ignore SIGPIPE - we have othere ways to deal with filter death */
    DPRINTF1(DEBUG_FILTERING, "spawned filter with pid %d", filter_pid);
    close (input_pipe_fds[0]);
    close (output_pipe_fds[1]);
  }
}


void kill_filter()  {
  int status;
  assert (filter_pid && filter_input_fd);
  close(filter_input_fd); /* filter will see EOF and should exit  */
  myalarm(40); /* give filter 0.04seconds to go away */
  if(!filter_is_dead &&                     /* filter's SIGCHLD hasn't been caught  */
     waitpid(filter_pid, &status, WNOHANG) < 0 && /* interrupted  .. */
     WTERMSIG(status) == SIGALRM) {         /*  .. by alarm (and not e.g. by SIGCHLD) */
     myerror(WARNING|NOERRNO, "filter didn't die - killing it now");
  }
  if (filter_pid)
    kill(filter_pid, SIGKILL); /* do this as a last resort */
  myalarm(0);
}       
        


char *filters_last_words() {
  assert (filter_is_dead);
  return read_from_filter(TAG_OUTPUT);
}       
    
int filter_is_interested_in(int tag) {
  static char *interests = NULL;
  assert(tag <= MAX_INTERESTING_TAG);
  if (!interests) {
    char message[MAX_INTERESTING_TAG + 2];
    int i;
    mymicrosleep(500); /* Kludge - shouldn't the filter talk first - so we know it's alive? */
    for (i=0; i <= MAX_INTERESTING_TAG; i++)
      message[i] = 'n';
    message[i] = '\0';
    interests = pass_through_filter(TAG_WHAT_ARE_YOUR_INTERESTS, message);
    if(interests[TAG_SIGNAL] == 'y')
      myerror(WARNING|NOERRNO, "this filter handles signals, which means that signals are blocked during filter processing\n"
              "if the filter hangs, you won't be able to interrupt with e.g. CTRL-C (use kill -9 %d instead)  ", getpid());

  }
  return (interests[tag] == 'y');
}
 
static int user_frustration_signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGALRM};

   
char *pass_through_filter(int tag, const char *buffer) {
  char *filtered;

  assert(!out_of_band(tag));
  if (filter_pid ==0 || (tag <  MAX_INTERESTING_TAG && !filter_is_interested_in(tag)))
    return mysavestring(buffer);

  if (tag == TAG_WHAT_ARE_YOUR_INTERESTS ||    /* only evaluate next alternative if interests are known                                                       */
      !filter_is_interested_in(TAG_SIGNAL))    /* signal handling filters will get an "unexpected tag" error when the signal arrives during filter processing */
    unblock_signals(user_frustration_signals); /* allow users to use CTRL-C, but only after uninterruptible_msec                                              */

  DPRINTF3(DEBUG_FILTERING, "to filter (%s, %d bytes) %s", tag2description(tag), (int) strlen(buffer), M(buffer)); 
  write_to_filter((expected_tag = tag), buffer);
  filtered = read_from_filter(tag);
  DPRINTF3(DEBUG_FILTERING, "from filter (%s, %d bytes) %s",  tag2description(tag), (int) strlen(filtered), M(filtered));

  block_all_signals();

  return filtered;
}

        


static char *read_from_filter(int tag) {
  uint8_t  tag8;
  DEBUG_RANDOM_SLEEP;
  assert (!out_of_band(tag));

  
  while (read_patiently2(filter_output_fd, &tag8, sizeof(uint8_t), 1000, "from filter"), out_of_band(tag8))
    handle_out_of_band(tag8, read_tagless());
  if (tag8 != tag)
    myerror(FATAL|NOERRNO, "Tag mismatch, expected %s from filter, but got %s", tag2description(tag), tag2description(tag8));
  
  return read_tagless();
}


static char *read_tagless() {
  uint32_t length32;
  char *buffer;
         
  read_patiently2(filter_output_fd, &length32, sizeof(uint32_t), 1000, "from filter");
  buffer = mymalloc(length32);
  read_patiently2(filter_output_fd, buffer, length32, 1000,"from filter");
  
  if (buffer[length32 -1 ] != '\n')
    myerror(FATAL|USE_ERRNO, "filter output without closing newline");
  buffer[length32 -1 ] = '\0';
        
  return buffer;
}

static bool starts_with(const char *str, const char *prefix) {
  return mystrstr(str, prefix) == str;
}

static void maybe_tweak_readline(const char*message) { /* a bit kludgey, but easy to extend  */
  char **words;
  if (message[0] != '@')
    return;
  words = split_with(message, "::"); /* words and its elements are allocated on the heap */
  /* parameter checking should be done  in the {python,perl} modules - if not, the assertions below may fail */
  assert(words[1] != NULL);
  if (starts_with(message, "@rl_completer_word_break_characters::")) 
      rl_completer_word_break_characters = words[1];
  if (starts_with(message, "@rl_completer_quote_characters::"))
    rl_completer_quote_characters = words[1];
  if (starts_with(message, "@rl_filename_completion_desired::"))
    rl_filename_completion_desired = my_atoi(words[1]);
  if (starts_with(message, "@rl_variable_bind::")) {  /* "@rl_variable_bind::rl_variable_name::value::\n" */
    assert(words[2] != NULL);
    rl_variable_bind(words[1], words[2]); /* no need for error handling: readline will complain if necessary */
  }
  /* feel free to extend this list (but make sure to modify the {perl,python} modules accordingly! */
}     
  
void handle_out_of_band(int tag, char *message) {
  int split_em_up = FALSE;

  DPRINTF3(DEBUG_FILTERING, "received out-of-band (%s, %d bytes) %s", tag2description(tag),
           (int) strlen(message), M(message)); 
  switch (tag) {
  case TAG_ERROR:
    if (expected_tag == TAG_COMPLETION) /* start new line when completing (looks better) */
      fprintf(stderr, "\n"); /* @@@ error reporting (still) uses buffered I/O */
    WONTRETURN(myerror(FATAL|NOERRNO, message));
  case TAG_OUTPUT_OUT_OF_BAND: 
    my_putstr(message);
    break;
  case TAG_ADD_TO_COMPLETION_LIST:
  case TAG_REMOVE_FROM_COMPLETION_LIST:
    split_em_up = TRUE;
    break;
  case TAG_IGNORE:
    maybe_tweak_readline(message); 
    break;
  default:
    WONTRETURN(myerror(FATAL|USE_ERRNO, "out-of-band message with unknown tag %d: <%20s>", tag, message));
  }     
  if (split_em_up) {
    char **words = split_with(message, " \n\t");
    char **plist, *word;
    for(plist = words;(word = *plist); plist++)
      if (tag == TAG_ADD_TO_COMPLETION_LIST)
        add_word_to_completions(word);
      else
        remove_word_from_completions(word);
    free_splitlist(words);
  }     
  free(message);
}


static void write_to_filter(int tag, const char *string) {
  write_message(filter_input_fd, tag, string, "to filter");
}


static void write_message(int fd, int tag,  const char *string, const char *description) {
  uint8_t  tag8     = tag;
  uint32_t length32 = strlen(string) + 1;  
  write_patiently2(fd, &tag8, sizeof (uint8_t) , 1000, description);
  write_patiently2(fd, &length32, sizeof(uint32_t), 1000, description);
  write_patiently2(fd, string, length32 - 1 , 1000, description);
  write_patiently2(fd, "\n", 1 , 1000, description);
}
          



static char* tag2description(int tag) {
  switch (tag) {
  case TAG_INPUT:                      return "INPUT";
  case TAG_OUTPUT:                     return "OUTPUT";
  case TAG_HISTORY:                    return "HISTORY";
  case TAG_COMPLETION:                 return "COMPLETION";
  case TAG_PROMPT:                     return "PROMPT";
  case TAG_HOTKEY:                     return "HOTKEY";
  case TAG_SIGNAL:                     return "SIGNAL";
  case TAG_WHAT_ARE_YOUR_INTERESTS:    return "WHAT_ARE_YOUR_INTERESTS";
  case TAG_IGNORE:                     return "TAG_IGNORE";
  case TAG_ADD_TO_COMPLETION_LIST:     return "ADD_TO_COMPLETION_LIST";
  case TAG_REMOVE_FROM_COMPLETION_LIST:return "REMOVE_FROM_COMPLETION_LIST";
  case TAG_OUTPUT_OUT_OF_BAND:         return "OUTPUT_OUT_OF_BAND";
  case TAG_ERROR:                      return "ERROR";
  default:                             return as_string(tag);
  }
}
  
