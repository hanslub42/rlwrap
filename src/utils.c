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



#include "rlwrap.h"





static FILE *log_fp;


/* Give up the processor so that other processes (like command) can have their say. Especially useful with multi-line
   input: if we, after writing a line to the inferior command, would return to the main select() loop immediately, we
   would find more output to write, and keep blathering away without giving the poor command time to say something
   back */

void
yield()
{
  DPRINTF0(DEBUG_TERMIO, "yield...");
#if 0 /* If command writes some output and immediately starts a long computation, sched_yield() will not return for a
         long time,and rlwrap will not see the output for all that time. The "poor mans sched_yield" with select
         actually works better! */
  sched_yield();            
#else
  { struct timeval a_few_millisecs = { 0, 1000 }; 
    select(0, NULL, NULL, NULL, &a_few_millisecs); /* poor man's sched_yield() */
  }     
#endif
}


static volatile int signal_handled = FALSE;

#ifdef HAVE_REAL_PSELECT

void zero_select_timeout () {
  signal_handled = TRUE;
}

#else


#define until_hell_freezes  NULL ;
static struct timeval * volatile pmy_select_timeout_tv; /* The SIGCHLD handler sets this variable (see zero_select_timeout() below) to make
                                                          select() return immediately when a child has died. gcc (4.8 and higher) may optimize it 
                                                          into a register, which won't work: hence the "volatile" keyword */ 
static struct timeval my_select_timeout_tv;

void zero_select_timeout () {
  my_select_timeout_tv.tv_sec = 0;
  my_select_timeout_tv.tv_usec = 0;
  pmy_select_timeout_tv = &my_select_timeout_tv;
  signal_handled = TRUE;  
}
#endif

/* 
   Even though even older linux systems HAVE PSELECT, is is non-atomic: signal handlers may (and
   generally will) run between the unblocking of the signals and the select call (which will
   then wait untill hell freezes over) Therefore we always convert the contents op
   ptimeout_ts into to a static struct timeval my_select_timeout_tv, and use the function
   zero_select_timeout to set this to {0,0} from within a signal handler, which will then
   make select return immediately.  (Linus Torvalds mentions this trick in 
   http://lkml.indiana.edu/hypermail/linux/kernel/0506.1/1191.html),

   As I know of no reliable way to distiguish real pselect from fake pselect at configure
   time HAVE_REAL_PSELECT will be undefined on all systems, even those that do have a genuine
   pselect.
*/



int
my_pselect(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *ptimeout_ts, const sigset_t *sigmask) {

#ifdef HAVE_REAL_PSELECT
  return pselect(n, readfds, writefds, exceptfds, ptimeout_ts, sigmask);
#else
  int retval;
  sigset_t oldmask;
  if (ptimeout_ts) { /* convert timespec e.g {0,40000000} to timeval {0, 40000} */
    pmy_select_timeout_tv = &my_select_timeout_tv;
    pmy_select_timeout_tv -> tv_sec = ptimeout_ts -> tv_sec;
    pmy_select_timeout_tv -> tv_usec = ptimeout_ts -> tv_nsec / 1000;
  } else
    pmy_select_timeout_tv = until_hell_freezes;
  signal_handled = FALSE;
      
  sigprocmask(SIG_SETMASK, sigmask, &oldmask);
  /* most signals will be catched HERE (and their handlers will set my_select_timeout_tv to {0,0}) */
  retval = select(n, readfds, writefds, exceptfds, pmy_select_timeout_tv);
  /* but even if they are slow off the mark and get catched HERE the code 3 lines below will notice */
  sigprocmask(SIG_SETMASK, &oldmask, NULL);
  
                  
  if (signal_handled && retval >= 0) { 
    errno = EINTR;
    return -1;
  } else {
    return retval;   
  }
#endif
}       

struct termios *
my_tcgetattr(int fd, char *UNUSED(which)) 
{
  struct termios *pterm =
    (struct termios *)mymalloc(sizeof(struct termios));
  return tcgetattr(fd, pterm) < 0 ?  NULL : pterm;
}


/* empty signal handler */
void
do_nothing(int UNUSED(signal))
{
  /* yawn.... */
}



/* When using read (2) or write (2), one always has to be prepared to handle incomplete
   reads or writes. The {read,write}_patiently* routines below do just that: they repeat the read
   or write until the whole buffer has been filled, or an error occurs.
   The {read,write}_patiently2 functions are specialised for reading/writing the input/output pipe
   of a filter */

   
   
int
write_patiently(int fd, const void *buffer, int count, const char *whither) {
  int nwritten = 0;
  int total_written =  0;
  assert(count >= 0);
  if (count == 0)
    return TRUE;
  while(1) { 
    if((nwritten = write(fd, (char *)buffer + total_written, count - total_written)) <= 0) {
      if (errno == EINTR)
        continue;
      else if (errno == EPIPE || nwritten == 0) {
        return FALSE;
      } else 
        myerror(FATAL|USE_ERRNO, "write error %s", whither);
    }   
    
    total_written += nwritten;
    if (total_written == count) /* done */      
      break;
  }      
  return TRUE;
}       



static int user_frustration_signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGALRM};

/* keep  reading from fd and write into buffer until count bytes have been read.
   for uninterruptible_msec, restart when interrupted by a signal, after this,
   bail out with an error. if count < 0, returns a 0-terminated copy of buffer  */


void
read_patiently2 (int fd,
                 void *buffer,
                 int count,
                 int uninterruptible_msec,
                 const char *whence) {
  int nread = 0;
  int total_read =  0;
  int interruptible = FALSE;

  assert (count >= 0);
  unblock_signals(user_frustration_signals); /* allow users to use CTRL-C, but only after uninterruptible_msec */
  if (count > 0) {
    myalarm(uninterruptible_msec);
    while (1) {
      assert(count > total_read);
      if((nread = read(fd, (char *) buffer + total_read, count - total_read)) <= 0) {
        if (nread < 0 && errno == EINTR) {
          if (interruptible) 
            myerror(FATAL|NOERRNO, "(user) interrupt reading %s, filter_hangs?", whence);
          
          if (received_sigALRM) {
            received_sigALRM = FALSE;
            interruptible = TRUE;
          }
          continue;
        } else if (nread == 0)  
           myerror(FATAL|NOERRNO, "EOF reading %s", whence);
         else  /* nread < 0 */        
           myerror(FATAL|USE_ERRNO, "error reading %s",  whence);
        
      }
      total_read += nread;
      if (total_read == count)  /* done */
        break;
      
    }
  }     
  myalarm(0); /* reset alarm */
  block_all_signals();
  DPRINTF2(DEBUG_AD_HOC, "read_patiently2 read %d bytes: %s", total_read, mangle_buffer_for_debug_log(buffer, total_read));
  return;
}      






void
write_patiently2(int fd,
                 const void *buffer,
                 int count,
                 int uninterruptible_msec,
                 const char* whither) {
  int nwritten = 0;
  int total_written =  0;
  int interruptible = FALSE;
  

  assert(count >= 0);
  if (count == 0)
    return; /* no-op */
  unblock_signals(user_frustration_signals); /* allow impatient users to hit CTRL-C, but only after uninterruptible_msec */
  myalarm(uninterruptible_msec);        
  while(1) { 
    if((nwritten = write(fd, (char *)buffer + total_written, count - total_written)) <= 0) {
      if (nwritten < 0 && errno == EINTR) {
        if (interruptible)
           myerror(FATAL|NOERRNO, "(user) interrupt - filter hangs?");
        if (received_sigALRM) {
          received_sigALRM = FALSE;
          interruptible = TRUE;
        }
        continue;
      } else  /* nwritten== 0 or < 0 with error other than EINTR */
        myerror(FATAL|USE_ERRNO, "error writing %s", whither);
      
    }
    total_written += nwritten;
    if (total_written == count) /* done */      
      break;
  }
  myalarm(0);
  DPRINTF2(DEBUG_AD_HOC, "write_patiently2 wrote %d bytes: %s", total_written, mangle_buffer_for_debug_log(buffer, total_written));
  block_all_signals();
  return;
}       



void
mysetenv(const char *name, const char *value)
{
  int return_value = 0;
  
  
#ifdef HAVE_SETENV
  return_value = setenv(name, value, TRUE);   
#elif defined(HAVE_PUTENV)
  char *name_is_value = add3strings (name, "=", value);
  return_value = putenv (name_is_value);
#else /* won't happen, but anyway: */      
  myerror(WARNING|NOERRNO, "setting environment variable %s=%s failed, as this system has neither setenv() nor putenv()", name, value);
#endif
     
  if (return_value != 0)
    myerror(WARNING|USE_ERRNO, "setting environment variable %s=%s failed%s", name, value,
           (errno ? "" : " (insufficient environment space?)"));     /* will setenv(...) = -1  set errno? */
}       
          
void set_ulimit(int resource, long value) {
  #ifdef HAVE_SETRLIMIT
  struct rlimit limit;
  int result;
  
  limit.rlim_cur = value;
  result = setrlimit(resource, &limit);
  DPRINTF4(DEBUG_ALL, "setrlim() used to set resource #%d to value %ld, result = %d (%s)",
          resource, value, result, (result == 0 ? "success" : "failure"));
  #endif
}



int open_unique_tempfile(const char *suffix, char **tmpfile_name) {
  char **tmpdirs = list4(getenv("TMPDIR"), getenv("TMP"), getenv("TEMP"), "/tmp");
  char *tmpdir = first_of(tmpdirs);
  int tmpfile_fd;

  if (!suffix) 
    suffix = "";
 
  *tmpfile_name = mymalloc(MAXPATHLEN+1);

#ifdef HAVE_MKSTEMPS
  snprintf4(*tmpfile_name, MAXPATHLEN, "%s/%s_%s_XXXXXX%s", tmpdir, program_name, command_name, suffix); 
  tmpfile_fd = mkstemps(*tmpfile_name, strlen(suffix));  /* this will write into *tmpfile_name */
#else
  {   static int tmpfile_counter = 0;
      snprintf6(*tmpfile_name, MAXPATHLEN, "%s/%s_%s_%d_%d%s", tmpdir, program_name, command_name, command_pid, tmpfile_counter++, suffix);
      tmpfile_fd = open(*tmpfile_name, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  }
#endif
  if (tmpfile_fd < 0)
    myerror(FATAL|USE_ERRNO, "could not create readable/writable temporary file %s", tmpfile_name);
  free(tmpdirs);
  return tmpfile_fd;
}  


  

static char*
markup(const char*str)
{
  if (isatty(STDOUT_FILENO) && (ansi_colour_aware || colour_the_prompt))
    return add3strings("\033[1;31m", str,"\033[0m");
  else
    return mysavestring(str);
}       


/* print error or warning message. There are two error flags,
   defined in rlwrap.h
  
   FATAL:     exit(EXIT_FAILURE) after printing the message)
   USE_ERRNO: print perror(errno) after the message
*/


void
myerror(int error_flags, const char *message_format, ...)
{
  int saved_errno = errno;
  char contents[BUFFSIZE];
  int is_warning = !(error_flags & FATAL);
  char *warning_or_error = is_warning ? "warning: " : "error: ";
  static int warnings_given = 0;  
  char *message = add2strings(program_name, ": ");

  va_list ap;
  va_start(ap, message_format);
  vsnprintf(contents, sizeof(contents) - 1, message_format, ap);
  va_end(ap);

  message = append_and_free_old(message, markup(warning_or_error)); 
  message = append_and_free_old(message, contents);
                             
  if ((error_flags & USE_ERRNO) && saved_errno) {
    message = append_and_free_old(message, ": ");
    message = append_and_free_old(message, strerror(saved_errno));
  }                                
  message = append_and_free_old(message,"\n");                            
    
  fflush(stdout);
  DPRINTF2(DEBUG_ALL, "%s %s", warning_or_error, contents);
 

  if (! (is_warning && nowarn))
    fputs(message, stderr); /* @@@ error reporting (still) uses buffered I/O */
  if (is_warning && !warnings_given++ && !nowarn) 
    fputs("\nwarnings can be silenced by the --no-warnings (-n) option\n", stderr);
  
  fflush(stderr);
  free(message);
  errno =  saved_errno;

  if (error_flags & FATAL) { 
    if (!i_am_child)
      cleanup_rlwrap_and_exit(EXIT_FAILURE);
    else /* child: die and let parent clean up */
      exit(EXIT_FAILURE);
  }
  
}


    

void
open_logfile(const char *filename)
{
  time_t now;

  log_fp = fopen(filename, "a");
  if (!log_fp)
    myerror(FATAL|USE_ERRNO, "Cannot write to logfile %s", filename);
  now = time(NULL);
  fprintf(log_fp, "\n\n[rlwrap] %s\n", ctime(&now));
}

void
write_logfile(const char *str)
{
  if (log_fp)
    fputs(str, log_fp);
}


size_t
filesize(const char *filename)
{
  struct stat buf;

  if (stat(filename, &buf))
    myerror(FATAL|USE_ERRNO, "couldn't stat file %s", filename);
  return (size_t) buf.st_size;
}

void
close_logfile()
{
  if (log_fp)
    fclose(log_fp);
}

void
close_open_files_without_writing_buffers() /* called from child just before exec(command) */
{
  if(log_fp)
    close(fileno(log_fp));  /* don't flush buffers to avoid avoid double double output output */
  if (debug)
    close(fileno(debug_fp));
}


void
timestamp(char *buf, int size)
{
  struct timeval now;
  static struct timeval firsttime; /* remember when first called */
  static int never_called = 1;
  long diff_usec;
  float diff_sec;
  
  gettimeofday(&now, NULL);
  if (never_called) {
    firsttime = now; 
    never_called = 0;
  }
  diff_usec = 1000000 * (now.tv_sec -firsttime.tv_sec) + (now.tv_usec - firsttime.tv_usec);
  diff_sec = diff_usec / 1000000.0;
  
  snprintf1(buf, size, "%f ", diff_sec);
}



/* Dan Bernsteins djb2, hashing n strings in one go */
unsigned long
hash_multiple(int n, ...)
{
  unsigned long hash = 5381;
  int i, c;
  char *str;

  va_list ap;
  va_start(ap, n);
  for(i = 0; i < n; i++) {
    str = va_arg(ap, char *);
    assert(str != NULL);
    while ((c = (unsigned char) *str++))
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  va_end(ap);
  return hash;
}


int killed_by(int status) {
#ifdef WTERMSIG
  if (WIFSIGNALED(status))
    return WTERMSIG(status);
#endif
  return 0;
}       



/* get_new_slave_cwd(&cwd) tries to find the rlwrapped command's current working directory. If this
   differs from cwd, free(*cwd) and set *cwd to (a copy of) the new working directory. Return value: 0, or 1
   if rlwrap needs to do a chdir(cwd) to again have the same working dir as the rlwrapped command.
*/
static int
get_new_slave_cwd(char **cwd)
{
  char *possibly_new_cwd = NULL;
  int return_value = 0;

  
#if defined(HAVE_PROC_PID_CWD) /* Linux, Solaris, and FreeBSD with the proc filesystem */

  static char *proc_pid_cwd = NULL;
  char readlink_buffer[MAXPATHLEN+1];
    
  if (!proc_pid_cwd)
    proc_pid_cwd = add3strings(PROC_MOUNTPOINT, "/", add2strings(as_string(command_pid), "/cwd"));

# ifdef HAVE_READLINK
  if  (readlink(proc_pid_cwd, readlink_buffer, MAXPATHLEN) > 0)
    possibly_new_cwd = mysavestring(readlink_buffer);
# else
  /* readlink unavailable, use /proc/nnn/cwd ... */
  possibly_new_cwd = mysavestring(proc_pid_cwd);
  return_value = 1; /* always do a chdir(): the symlink /proc/nnn/cwd may point somewhere else now... */
#  endif /* HAVE_READLINK */
  
#elif HAVE_DECL_PROC_PIDVNODEPATHINFO /* OS X */

  struct proc_vnodepathinfo vpi;

  if (proc_pidinfo(command_pid, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi)) <= 0)
    DPRINTF2(DEBUG_COMPLETION, "proc_pidinfo(%d, .. failed: %s", command_pid, strerror(errno));
  possibly_new_cwd = mysavestring(vpi.pvi_cdir.vip_path);

#elif HAVE_FREEBSD_LIBPROCSTAT /* FreeBSD without the proc filesystem */

  unsigned int count = 0;
  struct procstat *procstat = procstat_open_sysctl();
  struct kinfo_proc *kip = procstat_getprocs(procstat, KERN_PROC_PID, command_pid, &count);
  struct filestat_list *head;
  struct filestat *fst;
  
  if (count == 1) {
    head = procstat_getfiles(procstat, kip, 0);
  
    STAILQ_FOREACH(fst, head, next) 
      if (fst->fs_uflags & PS_FST_UFLAG_CDIR) {
        possibly_new_cwd = mysavestring(fst->fs_path);
        break;
      }     
    procstat_freefiles(procstat, head);
    procstat_freeprocs(procstat, kip);
    procstat_close(procstat);
  }     

#else
  /* no HAVE_PROC_PID_CWD, HAVE_DECL_PROC_PIDVNODEPATHINFO or HAVE_FREEBSD_LIBPROCSTAT: do nothing */
#endif
  
  if (possibly_new_cwd && strcmp(*cwd, possibly_new_cwd)) {
    return_value = 1;
    free(*cwd);
    *cwd = mysavestring(possibly_new_cwd);
  }
  free(possibly_new_cwd);
  return return_value;
}     
 


/* change_working_directory() tries to change rlwrap's working directory to the rlwrapped command's current working directory   */
void
change_working_directory()
{
  static char *slaves_working_directory = NULL;
  if (!slaves_working_directory)
    slaves_working_directory = mysavestring(".");
  if(command_pid > 0 && get_new_slave_cwd(&slaves_working_directory)) {
    if (chdir(slaves_working_directory)) {
      DPRINTF2(DEBUG_COMPLETION, "chdir(%s) failed: %s", slaves_working_directory, strerror(errno));
    } else {
      DPRINTF1(DEBUG_COMPLETION, "chdir(%s): success", slaves_working_directory);
    }   
  }
}       




#undef isset
#define isset(flag) ((flag) ? "set" : "unset")

/* print info about terminal settings */
void log_terminal_settings(struct termios *terminal_settings) {
  if (!terminal_settings)
    return;
  DPRINTF3(DEBUG_TERMIO, "terminal settings: clflag.ISIG: %s, cc_c[VINTR]=%d, cc_c[VEOF]=%d",
           isset(terminal_settings->c_lflag | ISIG),
           terminal_settings->c_cc[VINTR],
           terminal_settings->c_cc[VEOF]);
}

void log_fd_info(int fd) {
  struct termios terminal_settings;
  if (isatty(fd)) {
    if (tcgetattr(fd, &terminal_settings) < 0) {
      DPRINTF1(DEBUG_TERMIO, "could not get terminal settings for fd %d", fd);
    } else {
      DPRINTF1(DEBUG_TERMIO, "terminal settings for fd %d:", fd);
    }   
    log_terminal_settings(&terminal_settings);
  }
}       


/* some last-minute checks before we can start */
void
last_minute_checks()
{
  /* flag unhealthy option combinations */
  if (multiline_separator && filter_command)
    myerror(WARNING|NOERRNO, "Filters don't work very well with multi-line rlwrap!");
}


/* sleep a little (for debugging cursor movement with the SHOWCURSOR macro) */
void mymicrosleep(int msec) {
  int sec = msec / 1000;
  struct timeval timeout;
  msec -= (1000*sec);

  timeout.tv_sec = sec;
  timeout.tv_usec =  1000 * msec;
  select (0,NULL,NULL,NULL,&timeout);
}       



/* print info about option, considering whether we HAVE_GETOPT_LONG and whether GETOPT_GROKS_OPTIONAL_ARGS */
static void print_option(char shortopt, char *longopt, char*argument, int optional, char *comment) {
  int long_opts, optional_args;
  char *format;
  char *maybe_optional = "";
  char *longoptional = "";

  
#ifdef HAVE_GETOPT_LONG
  long_opts = TRUE;
#else
  long_opts = FALSE;
#endif

#ifdef GETOPT_GROKS_OPTIONAL_ARGS
  optional_args = TRUE;
#else
  optional_args = FALSE;
#endif

  if (argument) {
    maybe_optional = (optional_args && optional ? add3strings("[", argument,"]") :  add3strings("  <", argument,">"));
    longoptional = (optional ? add3strings("[=", argument,"]") : add3strings("=<", argument, ">"));
  }
  
  /* if we cannot use long options, use the long option as a reminder (no warnings) instead of "--no-warnings" */ 
  if (!long_opts)
    longopt = search_and_replace("-"," ", longopt, 0, NULL,NULL);
  format = add2strings ("  -%c%-24.24s", (long_opts  ? " --%s%s" : "(%s)")); 
  fprintf(stderr, format, shortopt, maybe_optional, longopt, longoptional);
  if (comment)
    fprintf(stderr, " %s", comment);
  fprintf(stderr, "\n");
  /* don't free allocated strings: we'll exit() soon */
}

static void
print_debug_flag(int flag, char *explanation) {
  fprintf(stderr, "    %4d    %s\n", flag, explanation);
}

void
usage(int status)
{
  fprintf(stderr, "Usage: %s [options] command ...\n"
          "\n"
          "Options:\n", program_name);

  print_option('a', "always-readline", "password prompt", TRUE, NULL);
  print_option('A', "ansi-colour-aware", NULL, FALSE, NULL);
  print_option('b', "break-chars", "chars", FALSE, NULL);
  print_option('c', "complete-filenames", NULL, FALSE, NULL);
  print_option('C', "command-name", "name|N", FALSE, NULL);
  print_option('D', "history-no-dupes", "0|1|2", FALSE, NULL);
  print_option('e', "extra-char-after-completion", "char|''", FALSE, NULL);   
  print_option('f', "file", "completion list", FALSE,NULL);
  print_option('g', "forget-matching", "regexp", FALSE,NULL);
  print_option('h', "help", NULL, FALSE, NULL);
  print_option('H', "history-filename", "file", FALSE, NULL);
  print_option('i', "case-insensitive", NULL, FALSE, NULL);
  print_option('I', "pass-sigint-as-sigterm", NULL, FALSE, NULL);
  print_option('l', "logfile", "file", FALSE, NULL);
  print_option('m', "multi-line", "newline substitute", TRUE, NULL);
  print_option('M', "multi-line-ext", ".ext", FALSE, NULL);
  print_option('n', "no-warnings", NULL, FALSE, NULL);
  print_option('N', "no-children", NULL, FALSE, NULL);
  print_option('o', "one-shot", NULL, FALSE, NULL);
  print_option('O', "only-cook", "regexp", FALSE, NULL);
  print_option('p', "prompt-colour", "colour", TRUE, NULL);
  print_option('P', "pre-given","input", FALSE, NULL);
  print_option('q', "quote-characters", "chars", FALSE, NULL);
  print_option('r', "remember", NULL, FALSE, NULL);
  print_option('R', "renice", NULL, FALSE, NULL);
  print_option('s', "histsize", "N", FALSE,"(negative: readonly)");
  print_option('S', "substitute-prompt", "prompt", FALSE, NULL);
  print_option('t', "set-term-name", "name", FALSE, NULL);
  print_option('U', "mirror-arguments", NULL, FALSE, NULL);
  print_option('v', "version", NULL, FALSE, NULL);
  print_option('w', "wait-before-prompt", "N", FALSE, "(msec, <0  : patient mode)");
  print_option('W', "polling", NULL, FALSE, NULL);
  print_option('z', "filter", "filter command", FALSE, "('rlwrap -z listing' writes a list of installed filters)");  
  
 
#ifdef DEBUG
  fprintf(stderr, "\n");
  print_option('T', "test-terminal", NULL, FALSE, NULL);
  print_option('d', "debug", "mask", TRUE, add3strings("(output sent to ", DEBUG_FILENAME,")"));
  fprintf(stderr,
          "             \n"
          "The -d or --debug option *must* come first\n"
          "The debugging mask is a bitmask obtained by adding:\n");

  print_debug_flag (DEBUG_TERMIO, "to debug termio,");
  print_debug_flag (DEBUG_SIGNALS, "signal handling,");
  print_debug_flag (DEBUG_READLINE, "readline,");
  
  print_debug_flag (DEBUG_MEMORY_MANAGEMENT, "memory management,");
  print_debug_flag (DEBUG_FILTERING, "filtering,");
  print_debug_flag (DEBUG_COMPLETION, "and completion.");
  print_debug_flag (DEBUG_AD_HOC, "to see your own DEBUG_AD_HOC results"); 
  print_debug_flag (DEBUG_WITH_TIMESTAMPS, "to add (relative) timestamps,");
  print_debug_flag (FORCE_HOMEGROWN_REDISPLAY, "to force the use of my_homegrown_redisplay(),");
  print_debug_flag (DEBUG_LONG_STRINGS, "to not limit the length of strings in debug log (sloooow!)");
  print_debug_flag (DEBUG_RACES, "add random delays to expose race conditions.");
  fprintf(stderr,  "    default debug mask = %d (debug termio, signals and readline handling)\n"
                   "    use the shell construct $[ ] to calculate the mask, e.g. -d$[%d+%d+%d]\n",
          DEBUG_DEFAULT, DEBUG_DEFAULT, DEBUG_WITH_TIMESTAMPS, DEBUG_RACES);
  
#endif

  fprintf(stderr,
          "\n"
          "bug reports, suggestions, updates:\n"
          "https://github.com/hanslub42/rlwrap\n");

  exit(status);
}           
  

#ifdef DEBUG
#undef mymalloc
#endif

/* malloc with simplistic error handling: just bail out when out of memory */
void *
mymalloc(size_t size)
{                       
  void *ptr;
  ptr = malloc(size);
  if (ptr == NULL) {
    /* don't call myerror(), as this calls mymalloc() again */
    fprintf(stderr, "Out of memory: tried in vain to allocate %d bytes\n", (int) size);
    exit(EXIT_FAILURE);
  }     
  return ptr;
}


/* free() with variable number of arguments. To show where the argumets end, the last argument should be special,
   (and never a legitimate pointer) but we cannot use NULL (as the to-be-freed pointers may legitimately be NULL)
   We now use FMEND (#defined as ((void *) -1) in rlwrap.h, but @@@ is this fool-proof?
 */

void
free_multiple(void *ptr, ...)
{
  void *p;
  va_list ap;
  free(ptr);
  va_start(ap, ptr);
  while((p = va_arg(ap, void *)) != FMEND) {
    free(p);
  }
  va_end(ap);
}       




void mysetsid() {
# ifdef HAVE_SETSID /* c'mon, this is POSIX! */
  pid_t ret = setsid();

  DPRINTF2(DEBUG_TERMIO, "setsid() returned %d %s", (int)ret,
           ERRMSG(ret < 0));
# endif
}



/* mirror_args(): look up command's command line and copy it to our own
   important for commands that re-write their command lines e.g. to hide
   passwords.
*/



static char ** rlwrap_command_argv; /* The slice of rlwrap's argv after all rlwrap options */
static char *argv_buffer;
static int argv_len;
static char *stored_cmdline_filename;




void mirror_args_init(char**argv) {  
#ifdef ENABLE_MIRROR_ARGS
  int i;
  rlwrap_command_argv = argv;
  stored_cmdline_filename = mymalloc(MAXPATHLEN);
  *stored_cmdline_filename = '\0';  

  for (i = 0; argv[i]; i++) {  
    argv_len += strlen(argv[i]) + 1;
  }        
  argv_buffer = mymalloc(argv_len * sizeof(char) + 1);
#else
  stored_cmdline_filename = NULL;
  myerror(WARNING|NOERRNO, "On this system, the -U (--mirror-arguments) option doesn't work");
#endif
}      

/* C standard: "The parameters argc and argv and the strings pointed to by the argv
                array shall be modifiable by the program, and retain their last-stored
                values between program startup and program termination.

   This doesn't guarantee that those changed values will be visible to e.g. the ps (1) command
*/


      

void mirror_args(pid_t command_pid) {
  int cmdline_fd;
  long cmdline_length;
  static int been_warned = 0;

  if (!stored_cmdline_filename)
    return;
  if (!*stored_cmdline_filename) 
     snprintf2(stored_cmdline_filename, MAXPATHLEN , "%s/%d/cmdline", PROC_MOUNTPOINT, command_pid);
  if((cmdline_fd = open(stored_cmdline_filename, O_RDONLY)) < 1) {
    stored_cmdline_filename = NULL;
    if (been_warned++ == 0)
      myerror(WARNING|USE_ERRNO, "cannot mirror command's command line, as %s is unreadable", stored_cmdline_filename); 
    return;
  }     
  cmdline_length = read(cmdline_fd, argv_buffer,argv_len);
  /*  argv_buffer[cmdline_length] = '\0'; */
  DPRINTF2(DEBUG_TERMIO,"read %d bytes from %s", (int) cmdline_length, stored_cmdline_filename);
  
  if (memcmp(*rlwrap_command_argv, argv_buffer, cmdline_length)) {
    char *rlwrap_argstr = mem2str(*rlwrap_command_argv, cmdline_length);
    char *command_argstr = mem2str(argv_buffer, cmdline_length);
    DPRINTF2(DEBUG_TERMIO, "discrepancy: rlwarp_args: %s, command_args %s", rlwrap_argstr, command_argstr);
    free(rlwrap_argstr); 
    free(command_argstr);
  
    memcpy(*rlwrap_command_argv, argv_buffer, cmdline_length);
  }   
}

