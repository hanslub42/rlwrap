
/*  rlwrap.h: includes, definitions, declarations */

/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
    e-mail:  hanslub42@gmail.com
*/

#include "../config.h"
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#ifndef WEXITSTATUS
#  define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif



#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>  /* stdint.h is not on AIX, inttypes.h is in ISO C 1999 */

#include <errno.h>
#include <stdarg.h>



/* #define __USE_XOPEN
   #define __USE_GNU */

#include <stdlib.h>

#include <sched.h>



#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif


#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#endif


#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif

#ifdef HAVE_CURSES_H
#  include <curses.h>
#  ifdef HAVE_TERM_H
#    include <term.h>
#  else 
#    ifdef HAVE_NCURSES_TERM_H /* cygwin? AIX? */
#      include <ncurses/term.h>
#    endif
#  endif
#else
#  ifdef HAVE_TERMCAP_H
#     include <termcap.h>
#  endif
#endif

#include <termios.h>


#ifdef HAVE_REGEX_H
#  include <regex.h>
#endif

#if STDC_HEADERS
#  include <string.h>
#else
#  ifndef HAVE_STRRCHR
#    define strrchr rindex
#  endif
char *strchr(), *strrchr();

#  ifndef HAVE_MEMMOVE
#    define memmove(d, s, n) bcopy ((s), (d), (n))
#  endif
#endif


#ifdef HAVE_PTY_H /* glibc (even if BSD) */
#  include <pty.h>
#elif HAVE_LIBUTIL_H /* BSD, non-glibc */
#  include <libutil.h>
#elif HAVE_UTIL_H /* BSD, other varriants */
#  include <util.h>
#endif




#define BUFFSIZE 512

#ifndef MAXPATHLEN
#define MAXPATHLEN 512
#endif


#ifdef  HAVE_SNPRINTF           /* don't rely on the compiler understanding variadic macros */
# define snprintf0(buf,bufsize,format)                           snprintf(buf,bufsize,format)
# define snprintf1(buf,bufsize,format,arg1)                      snprintf(buf,bufsize,format,arg1)
# define snprintf2(buf,bufsize,format,arg1,arg2)                 snprintf(buf,bufsize,format,arg1,arg2)
# define snprintf3(buf,bufsize,format,arg1,arg2,arg3)            snprintf(buf,bufsize,format,arg1,arg2,arg3)
# define snprintf4(buf,bufsize,format,arg1,arg2,arg3,arg4)       snprintf(buf,bufsize,format,arg1,arg2,arg3,arg4)
# define snprintf5(buf,bufsize,format,arg1,arg2,arg3,arg4,arg5)  snprintf(buf,bufsize,format,arg1,arg2,arg3,arg4,arg5)
# define snprintf6(buf,bufsize,format,arg1,arg2,arg3,arg4,arg5,arg6)  snprintf(buf,bufsize,format,arg1,arg2,arg3,arg4,arg5,arg6)

#else
# define snprintf0(buf,bufsize,format)                           sprintf(buf,format)
# define snprintf1(buf,bufsize,format,arg1)                      sprintf(buf,format,arg1)
# define snprintf2(buf,bufsize,format,arg1,arg2)                 sprintf(buf,format,arg1,arg2)
# define snprintf3(buf,bufsize,format,arg1,arg2,arg3)            sprintf(buf,format,arg1,arg2,arg3)
# define snprintf4(buf,bufsize,format,arg1,arg2,arg3,arg4)       sprintf(buf,format,arg1,arg2,arg3,arg4)
# define snprintf5(buf,bufsize,format,arg1,arg2,arg3,arg4,arg5)  sprintf(buf,format,arg1,arg2,arg3,arg4,arg5)
# define snprintf6(buf,bufsize,format,arg1,arg2,arg3,arg4,arg5,arg6)  sprintf(buf,format,arg1,arg2,arg3,arg4,arg5,arg6)
# define vsnprintf(buf,bufsize,format,ap)                        vsprintf(buf,format,ap)
#endif


#ifndef HAVE_STRNLEN
# define strnlen(s,l) strlen(s)
#endif



#include <readline/readline.h>
#include <readline/history.h>


#ifndef HAVE_RL_VARIABLE_VALUE
#  define rl_variable_value(s) "off"
#endif

#ifndef HAVE_RL_READLINE_VERSION
#  define rl_readline_version 0xbaddef
#endif

#if defined(SPY_ON_READLINE)
extern int _rl_eof_char; /* Spying on readline's private life .... */
extern int _rl_horizontal_scroll_mode;  
# if !defined(HOMEGROWN_REDISPLAY)
#  define MAYBE_MULTILINE 1
# endif
#else
# define _rl_eof_char 0;
#endif

#ifdef MAYBE_MULTILINE
#  define redisplay_multiple_lines (!_rl_horizontal_scroll_mode)
#else
#  define redisplay_multiple_lines (strncmp(rl_variable_value("horizontal-scroll-mode"),"off",3) == 0)
#endif






/* in main.c: */
extern int master_pty_fd;
extern int slave_pty_sensing_fd;
extern FILE *debug_fp;
extern char *program_name, *command_name;
extern int always_readline;
extern int complete_filenames;
extern pid_t command_pid;
extern char *rlwrap_command_line;
extern char *command_line;
extern char *extra_char_after_completion;
extern int i_am_child;
extern int nowarn;
extern int debug;
extern char *password_prompt_search_string;
extern int one_shot_rlwrap;
extern char *substitute_prompt;
extern char *history_format;
extern char *forget_regexp;
extern char *multi_line_tmpfile_ext;
extern char *prompt_regexp;
extern int renice;
extern int ignore_queued_input;
extern int history_duplicate_avoidance_policy;
extern int pass_on_sigINT_as_sigTERM;
/* now follow the possible values for history_duplicate_avoidance_policy: */
#define KEEP_ALL_DOUBLES                0
#define ELIMINATE_SUCCESIVE_DOUBLES     1
#define ELIMINATE_ALL_DOUBLES           2
extern int one_shot_rlwrap;
extern int ansi_colour_aware;
extern int colour_the_prompt;
extern int received_WINCH;
extern int prompt_is_still_uncooked;
extern int mirror_arguments;
extern int impatient_prompt;
extern int we_just_got_a_signal_or_EOF;
extern int remember_for_completion;
extern int commands_children_not_wrapped; 
extern int accepted_lines;
extern char *filter_command;
extern int polling;

void cleanup_rlwrap_and_exit(int status);
void put_in_output_queue(char *stuff);
int  output_queue_is_nonempty(void);
void flush_output_queue(void);

/* in readline.c: */
extern struct rl_state
{                               /* struct to save readline state while we're processing output from slave command*/
  char *input_buffer;           /* current input buffer */
  char *raw_prompt;             /* current prompt */  
  char *cooked_prompt;          /* ditto redefined by user, or with colour added */
  int point;                    /* cursor position within input buffer */
  int already_saved;            /* flag set when saved, cleared when restored */
} saved_rl_state;

void save_rl_state(void);
void restore_rl_state(void);
void message_in_echo_area(char *message);
void init_readline(char *);
void my_redisplay(void);
void initialise_colour_codes(char *colour);
void reprint_prompt(int coloured);
char *colourise (const char *prompt);
void move_cursor_to_start_of_prompt(int erase);
#define ERASE 1
#define DONT_ERASE 0
int prompt_is_single_line(void);
char *process_new_output(const char* buffer, struct rl_state* state);
int cook_prompt_if_necessary (void);

extern int within_line_edit, transparent; 
extern char *multiline_separator;
extern char *pre_given;
extern int leave_prompt_alone;



/* in signals.c */
extern volatile int command_is_dead;
extern int commands_exit_status;
extern int filter_is_dead;
extern int filters_exit_status;
extern int sigterm_received;
extern int deferred_adapt_commands_window_size;
extern int signal_handlers_were_installed;
extern int received_sigALRM;

#ifndef RETSIGTYPE
#define RETSIGTYPE void /* systems where RETSIGTYPE = int have died out, apparently */
#endif
typedef RETSIGTYPE (*sighandler_type)(int);       


void mysignal(int sig, sighandler_type handler);
void install_signal_handlers(void);
void block_signals(int *sigs);
void unblock_signals(int *sigs);
void block_all_passed_on_signals(void);
void block_all_signals(void);
void unblock_all_signals(void);
void ignore_sigchld(void);
void suicide_by(int sig, int status);
int  adapt_tty_winsize(int from_fd, int to_fd);
void myalarm(int msec);
void handle_sigALRM(int signo);
char *signal_name(int signal);


/* in utils.c */
void  yield(void);
void  zero_select_timeout(void);
int   my_pselect(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *ptimeout_ts, const sigset_t *sigmask);
struct termios *my_tcgetattr(int fd, char *which);
int   read_patiently(int fd, void *buffer, int count, char *whence);
int   write_patiently(int fd, const void *buffer, int count, const char *whither);
void  read_patiently2(int fd, void *buffer, int count, int uninterruptible_msec, const char *whence);
void  write_patiently2(int fd, const void *buffer, int count, int uninterruptible_msec, const char *whither);
void  mysetenv(const char *name, const char *value);
void  set_ulimit(int resource, long value);
void  usage(int status);
int   open_unique_tempfile(const char *suffix, char **tmpfile_name);
void  mirror_args_init(char **argv);
void  mirror_args(pid_t command_pid);

/* flags to use for the error_flags argument to myerror */
#define FATAL     2
#define WARNING   0
#define USE_ERRNO 1
#define NOERRNO   0

/* constant to signify the end of a free_multiple() argument list (NULL would't work) */
#define FMEND  ((void *) -1)

void  myerror(int error_flags, const char *message, ...);
void  *mymalloc(size_t size);
void  free_multiple(void *ptr, ...);
void  mysetsid(void);
void  close_open_files_without_writing_buffers(void);
size_t filesize(const char *filename);
void  open_logfile(const char *filename);
void  write_logfile(const char *str);
void  close_logfile(void);
void  timestamp(char *buf, int size);
unsigned long hash_multiple(int n, ...);
int   killed_by(int status);
void  change_working_directory(void);
void  log_terminal_settings(struct termios *terminal_settings);
void  log_fd_info(int fd);
void  last_minute_checks(void);
void  mymicrosleep(int msec);
void  do_nothing(int unused);
extern char slaves_working_directory[];


/* in string_utils.c */
char *mybasename(const char *filename);
char *mydirname(const char *filename);
void  mystrlcpy(char *dst, const char *src, size_t size);
void  mystrlcat(char *dst, const char *src, size_t size);
char *mystrstr(const char *haystack, const char *needle);
char *mysavestring(const char *string);
char *add3strings(const char *str1, const char *str2, const char *str3);
#define add2strings(a,b)  add3strings(a,b,"")
int my_atoi(const char *nptr);
char *mystrtok(const char *s, const char *delim);
char **split_with(const char *string, const char *delim);
char *unsplit_with(int n, char ** strings, const char *delim);
char **split_on_single_char(const char *string, char c, int expected_count);
int scan_metacharacters(const char* string, const char *metacharacters);
char **list4 (char *el0, char *el1, char *el2, char *el3);
void free_splitlist (char **list);
char *append_and_free_old(char *str1, const char *str2);
char *mangle_char_for_debug_log(char c, int quote_me);
char *mangle_string_for_debug_log(const char *string, int maxlen);
char *mangle_buffer_for_debug_log(const char *buffer, int length);
char *mem2str(const char *mem, int size);
char *search_and_replace(char *patt, const char *repl, const char *string,
                         int cursorpos, int *line, int *col);
char *first_of(char **strings);
char *as_string(int i);
char *append_and_expand_history_format(char *line);
void remove_padding_and_terminate(char *buf, int length);
void unbackspace(char* buf);
void test_unbackspace (const char *input, const char *expected_result);
char *mark_invisible(const char *buf);
char *copy_and_unbackspace(const char *original);
int colourless_strlen(const char *str, char **pcopy_without_ignore_markers, int termwidth);
int colourless_strlen_unmarked (const char *str, int termwidth);
char *get_last_screenline(char *long_line, int termwidth);
char *lowercase(const char *str);
char *colour_name_to_ansi_code(const char *colour_name);
int match_regexp(const char *string, const char *regexp, int case_insensitive);
int isnumeric(char *string);
#define END_FIELD (char*)NULL /* marker object to terminate vargs */
char *append_field_and_free_old(char *message, const char *field);
char *merge_fields(char *field, ...);
char **split_filter_message(char *message, int *count);

/* in pty.c: */
pid_t my_pty_fork(int *, const struct termios *, const struct winsize *);
int slave_is_in_raw_mode(void);
struct termios *get_pterm_slave(void);
void mirror_slaves_echo_mode(void);
void completely_mirror_slaves_terminal_settings(void);
void completely_mirror_slaves_output_settings(void);
void completely_mirror_slaves_special_characters(void);
void write_EOF_to_master_pty(void);
void write_EOL_to_master_pty(char *);
int dont_wrap_command_waits(void);
int skip_rlwrap(void);

/* in ptytty.c: */
int ptytty_get_pty(int *fd_tty, const char **ttydev);
int ptytty_get_tty(const char *ttydev);
int ptytty_control_tty(int fd_tty, const char *ttydev);
int ptytty_openpty(int *amaster, int *aslave, const char **name);

/* in completion.rb: */
void init_completer(void);
void feed_file_into_completion_list(const char *completions_file);
void feed_line_into_completion_list(const char *line);
void add_word_to_completions(const char *word);
void remove_word_from_completions(const char *word);
char *my_completion_function(char *prefix, int state);

extern int completion_is_case_sensitive;


/* in term.c: */
extern int redisplay;                  /* TRUE when user input should be readable (instead of *******)  */
void init_terminal(void);
void set_echo(int);
void prepare_terminal(void);
void cr(void);
void backspace(int);
void clear_line(void);
void clear_the_screen(void);
void curs_up(void);
void curs_down(void);
void curs_left(void);
void test_terminal(void);
int my_putchar(TPUTS_PUTC_ARGTYPE c);
void my_putstr(const char *string);
int cursor_hpos(int col);
extern struct termios saved_terminal_settings;
extern int terminal_settings_saved;
extern struct winsize winsize;
extern char *term_name;
extern char *term_backspace, term_eof, term_stop, *term_cursor_hpos,
  *term_cursor_up, *term_cursor_down, *term_cursor_left, *term_rmcup, *term_rmkx;
extern int newline_came_last;

/* in filter.c */

#define MAX_TAG 255
#define TAG_INPUT 0
#define TAG_OUTPUT 1
#define TAG_HISTORY 2
#define TAG_COMPLETION 3
#define TAG_PROMPT 4
#define TAG_HOTKEY 5
#define MAX_INTERESTING_TAG 5 /* max tag for which the filter can have a handler */

#define TAG_WHAT_ARE_YOUR_INTERESTS 127


#define TAG_IGNORE 251
#define TAG_ADD_TO_COMPLETION_LIST 252
#define TAG_REMOVE_FROM_COMPLETION_LIST 253
#define TAG_OUTPUT_OUT_OF_BAND  254
#define TAG_ERROR 255

#define out_of_band(tag) (tag & 128)


extern pid_t filter_pid;
extern int filter_is_dead;
void spawn_filter(const char *filter_command);
void kill_filter(void);
char *pass_through_filter(int tag, const char *buffer);
char *filters_last_words(void);
void filter_test(void);




/* some handy macros */

#ifndef TRUE
#  define TRUE 1
#endif

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
# define max(a,b) ((a) < (b) ? (b) : (a))
#endif




#include "malloc_debug.h" /* malloc_debug.{c,h} not ready for prime time */

#define DEBUG_FILENAME "/tmp/rlwrap.debug"
#define KA_BOOM  {char *p = (char *) 1; *p = 'c';} /* dump core right here  */ 
#define KA_SCRUNCH {volatile int x=1, y=0; x = x/y;} /* force a SIGFPE */
#define KA_SCREECH kill(getpid(),SIGTRAP);        /* enter the debugger - use it to set (conditional) breakpoints from within C code: if (condition) KA_SCREECH; */

/* DPRINTF0 and its ilk  doesn't produce any output except when DEBUG is #defined (via --enable-debug configure option) */

#ifdef  DEBUG


#  define DEBUG_TERMIO                           1
#  define DEBUG_SIGNALS                          2
#  define DEBUG_READLINE                         4
#  define DEBUG_MEMORY_MANAGEMENT                8   /* used with malloc_debug.c */
#  define DEBUG_FILTERING                        16
#  define DEBUG_COMPLETION                       32
#  define DEBUG_AD_HOC                           64  /* only used during rlwrap development */
#  define DEBUG_WITH_TIMESTAMPS                  128 /* add timestamps to every line in debug log    */
#  define FORCE_HOMEGROWN_REDISPLAY              256 /* force use of my_homegrown_redisplay()        */
#  define DEBUG_LONG_STRINGS                     512 /* log all strings completely, however long they are */ 
#  define DEBUG_RACES                            1024 /* introduce random delays */  
#  define DEBUG_RANDOM_FAIL                      2048 /* fail tests randomly */
#  define DEBUG_TEST_MAIN                        4096 /* run test_main and exit  */

#  define DEBUG_MAX                              DEBUG_TEST_MAIN
#  define MANGLE_LENGTH                          ((debug_saved & DEBUG_LONG_STRINGS) ? 0 : 20) /* debug_saved is defined within DPRINTF macro */ 
#  define DEBUG_DEFAULT                          (DEBUG_TERMIO | DEBUG_SIGNALS | DEBUG_READLINE)
#  define DEBUG_ALL                              (2*DEBUG_MAX-1)

# define MAYBE_UNUSED(x)                         (void) (x)
# define MAYBE_UNUSED2(x,y)                      (void) (x); (void) (y)

# ifdef __GNUC__
#   define __MYFUNCTION__ __extension__ __FUNCTION__
#   define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#   define UNUSED_FUNCTION(f) __attribute__((__unused__)) UNUSED_ ## f
# else
#   define __MYFUNCTION__ ""
#   define UNUSED(x) UNUSED_ ## x
#   define UNUSED_FUNCTION(f) UNUSED_ ## f  
# endif

# define WHERE_AND_WHEN \
  int debug_saved = debug; char file_line[100], when[100];              \
  if(debug & DEBUG_WITH_TIMESTAMPS) timestamp(when, sizeof(when)); else *when='\0'; \
  debug = 0; /* don't debug while evaluating the DPRINTF arguments */ \
  snprintf2(file_line, sizeof(file_line),"%.15s:%d:",__FILE__,__LINE__); \
  fprintf(debug_fp, "%-20s %s %-25.25s ", file_line, when, __MYFUNCTION__);\


#  define NL_AND_FLUSH           fputc('\n', debug_fp) ; fflush(debug_fp); debug = debug_saved;

#  define DPRINTF0(mask, format)                                        \
  if ((debug & mask) && debug_fp) {WHERE_AND_WHEN; fprintf(debug_fp, format); NL_AND_FLUSH; }

#  define DPRINTF1(mask, format,arg)                                    \
  if ((debug & mask) && debug_fp) {WHERE_AND_WHEN; fprintf(debug_fp, format, arg); NL_AND_FLUSH; }

#  define DPRINTF2(mask, format,arg1, arg2)                             \
  if ((debug & mask) && debug_fp) {WHERE_AND_WHEN; fprintf(debug_fp, format, arg1, arg2); NL_AND_FLUSH; }

#  define DPRINTF3(mask, format,arg1, arg2, arg3)                       \
  if ((debug & mask) && debug_fp) {WHERE_AND_WHEN; fprintf(debug_fp, format, arg1, arg2, arg3); NL_AND_FLUSH; }

#  define DPRINTF4(mask, format,arg1, arg2, arg3, arg4)                 \
  if ((debug & mask) && debug_fp) {WHERE_AND_WHEN; fprintf(debug_fp, format, arg1, arg2, arg3,arg4); NL_AND_FLUSH; }

#  define ERRMSG(b)              (b && (errno != 0) ? add3strings("(", strerror(errno), ")") : "" )

#  define SHOWCURSOR(c)          if (debug & DEBUG_READLINE) {my_putchar(c); mymicrosleep(1200); curs_left();} /* (may work incorrectly at last column!)*/

#  define DEBUG_RANDOM_SLEEP        if (debug & DEBUG_RACES) {int sleeptime=rand()&31; DPRINTF1(DEBUG_RACES,"sleeping for %d msecs", sleeptime); mymicrosleep(sleeptime);}


#else
#  define UNUSED(x) x
#  define UNUSED_FUNCTION(f) f
#  define MAYBE_UNUSED(x)
#  define MAYBE_UNUSED2(x,y)
#  define MANGLE_LENGTH          0
#  define MAYBE_UNUSED(x)           
#  define MAYBE_UNUSED2(x,y)                     
#  define DPRINTF0(mask, format)
#  define DPRINTF1(mask, format,arg)
#  define DPRINTF2(mask, format,arg1, arg2)
#  define DPRINTF3(mask, format,arg1, arg2, arg3)
#  define DPRINTF4(mask, format,arg1, arg2, arg3, arg4)
#  define ERRMSG(b)
#  define SHOWCURSOR
#  define DEBUG_RANDOM_SLEEP  

/* Don't #define NDEBUG!  There are assertions that cannot be skipped, as in assert(important_function_call()) */
/* Todo (maybe) #define myassert(x) if(DEBUG){assert(x)} for the other (skippable) assertions                  */

#endif

#include <assert.h>
