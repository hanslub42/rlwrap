/*  pty.c: pty handling */

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
    e-mail:  hlub@knoware.nl

*/


#include "rlwrap.h"



/*local vars */
static int always_echo = FALSE;

/* global var */
int slave_pty_sensing_fd = -1;
static char *sensing_pty = "uninitialized"; 

pid_t
my_pty_fork(int *ptr_master_fd,
            const struct termios *slave_termios,
            const struct winsize *slave_winsize)
{
  int fdm, fds = -1;
  int ttyfd;
  pid_t pid;
  const char *slave_name;
  struct termios pterm;
  int only_sigchld[] = { SIGCHLD, 0 };
  


  ptytty_openpty(&fdm, &fds, &slave_name);


  block_signals(only_sigchld);  /* block SIGCHLD until we have had a chance to install a handler for it after the fork() */

  if ((pid = fork()) < 0) {
    myerror(FATAL|USE_ERRNO, "Cannot fork");
    return(42); /* the compiler may not know that myerror() won't return */
  } else if (pid == 0) {                /* child */
    DEBUG_RANDOM_SLEEP;
    i_am_child = TRUE;          /* remember who I am */
    unblock_all_signals();
    
    close(fdm);                 /* fdm not used in child */
    ptytty_control_tty(fds, slave_name);

    if (dup2(fds, STDIN_FILENO) != STDIN_FILENO) /* extremely unlikely */
      myerror(FATAL|USE_ERRNO, "dup2 to stdin failed");
    if (isatty(STDOUT_FILENO) && dup2(fds, STDOUT_FILENO) != STDOUT_FILENO)
      myerror(FATAL|USE_ERRNO, "dup2 to stdout failed");
    if (isatty(STDERR_FILENO) && dup2(fds, STDERR_FILENO) != STDERR_FILENO)
      myerror(FATAL|USE_ERRNO, "dup2 to stderr failed");
    if (fds > STDERR_FILENO)
      close(fds);


    if (slave_termios != NULL)
      if (tcsetattr(STDIN_FILENO, TCSANOW, slave_termios) < 0)
        myerror(FATAL|USE_ERRNO, "tcsetattr failed on slave pty");


    return (0);
  } else {                      /* parent */
    srand(pid); DEBUG_RANDOM_SLEEP;
    command_pid = pid;            /* the SIGCHLD signal handler needs this global variable */
 
    *ptr_master_fd = fdm;



    if (tcgetattr(fdm, &pterm) == 0) {        /* if we can do a tcgetattr on the master, assume that it reflects the slave's
                                                 terminal settings (at least on Linux and FreeBSD, this works), and use it as
                                                 slave_pty_sensing_fd, to keep tabs on slave terminal settings. In this case we can 
                                                 close fds (the slave), avoiding problems with lost output on FreeBSD  when the slave dies */
      slave_pty_sensing_fd = fdm; 
      sensing_pty = "master";     
      close(fds);
    } else if (tcgetattr(fds, &pterm) == 0) { /* we'll have to keep the slave pty open to get its terminal settings */
      slave_pty_sensing_fd = fds;
      sensing_pty = "slave";
    } else  {                                 /* Running out of options:                                                */
        fprintf(stderr,                       /* don't use mywarn() because of the strerror() message *within* the text */
                "Warning: %s cannot determine terminal mode of %s\n"
                "(because: %s).\n"
                "Readline mode will always be on (as if -a option was set);\n"
                "passwords etc. *will* be echoed and saved in history list!\n\n",
                program_name, command_name, strerror(errno));
        always_echo = TRUE;  
        sensing_pty = "no"; 
    }
    DPRINTF1(DEBUG_TERMIO, "Using %s pty to sense slave settings in parent", sensing_pty);


    if (!isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO)) {     /* stdout or stderr redirected? */
      ttyfd = open("/dev/tty", O_WRONLY);                       /* open users terminal          */
      DPRINTF1(DEBUG_TERMIO, "stdout or stderr are not a terminal, onpening /dev/tty with fd=%d", ttyfd);       
      if (ttyfd <0)     
        myerror(FATAL|USE_ERRNO, "Could not open /dev/tty");
      if (dup2(ttyfd, STDOUT_FILENO) != STDOUT_FILENO) 
        myerror(FATAL|USE_ERRNO, "dup2 of stdout to ttyfd failed");  
      if (dup2(ttyfd, STDERR_FILENO) != STDERR_FILENO)
        myerror(FATAL|USE_ERRNO, "dup2 of stderr to ttyfd failed");
      close (ttyfd);
    }

    errno = 0;
    if (renice) {
      int retval = nice(1);
      if (retval < 0 && errno)  
        myerror(FATAL|USE_ERRNO, "could not increase my own niceness"); 
    }

    if (slave_winsize != NULL)
      if (ioctl(slave_pty_sensing_fd, TIOCSWINSZ, slave_winsize) < 0) 
        myerror(FATAL|USE_ERRNO, "TIOCSWINSZ failed on %s pty", sensing_pty); /* This is done in parent and not in child as that would fail on Solaris (why?) */ 

    return (pid); 
  }
}


int
slave_is_in_raw_mode()
{
  struct termios *pterm_slave;
  static int been_warned = 0;
  int in_raw_mode;
  if (always_echo)
    return FALSE;

  if (command_is_dead)
    return FALSE; /* filter last words  too (even if ncurses-ridden) */
  if (!(pterm_slave = my_tcgetattr(slave_pty_sensing_fd, "slave pty"))) {
    if (been_warned++ == 1)     /* only warn once, but not the first time (as this usually means that the rlwrapped command has just died)
                                   - this is still a race when signals get delivered very late*/
      myerror(WARNING|USE_ERRNO, "tcgetattr error on slave pty (from parent process)");
    return TRUE;
  }     
 
  in_raw_mode = !(pterm_slave -> c_lflag & ICANON);
  myfree(pterm_slave);
  return in_raw_mode;
  
}

void
mirror_slaves_echo_mode()
{                               /* important e.g. when slave command asks for password  */
  struct termios *pterm_slave = NULL;
  int should_echo_anyway = always_echo || (always_readline && !dont_wrap_command_waits());

  if ( !(pterm_slave = my_tcgetattr(slave_pty_sensing_fd, "slave pty")) ||
       command_is_dead ||
       always_echo 
       )
    /* race condition here: SIGCHLD may not yet have been caught */
    return;

  assert (pterm_slave != NULL);
  
  if (tcsetattr(STDIN_FILENO, TCSANOW, pterm_slave) < 0 && errno != ENOTTY) /* @@@ */
    myerror(FATAL|USE_ERRNO, "cannot prepare terminal (tcsetattr error on stdin)");

  term_eof = pterm_slave -> c_cc[VEOF];
  
  /* if the --always-readline option is set with argument "assword:", determine whether prompt ends with "assword:\s" */
  if (should_echo_anyway && password_prompt_search_string) {
    char *p, *q;
    DPRINTF2(DEBUG_READLINE, "matching prompt <%s> and password search string <%s>..", saved_rl_state.raw_prompt, password_prompt_search_string); 
    assert(strlen(saved_rl_state.raw_prompt) < BUFFSIZE);
    p = saved_rl_state.raw_prompt + strlen(saved_rl_state.raw_prompt) - 1;
    q =
      password_prompt_search_string + strlen(password_prompt_search_string) -
      1;
    while (*p == ' ')           /* skip trailing spaces in prompt */
      p--;
    while (p >= saved_rl_state.raw_prompt && q >= password_prompt_search_string)
      if (*p-- != *q--)
        break;

    if (q < password_prompt_search_string)      /* found "assword:" */
      should_echo_anyway = FALSE;
    DPRINTF1(DEBUG_READLINE,".. result: should_echo_anyway = %d", should_echo_anyway);
  }


  if (!command_is_dead && (should_echo_anyway || pterm_slave->c_lflag & ECHO)) {
    redisplay = TRUE;
  } else {
    redisplay = FALSE;
  }
  if (pterm_slave)
    free(pterm_slave);
  set_echo(redisplay);          /* This is a bit weird: we want echo off all the time, because readline takes care
                                   of echoing, but as readline uses the current ECHO mode to determine whether
                                   you want echo or not, we must set it even if we know that readline will switch it
                                   off immediately   */
}

void
write_EOF_to_master_pty()
{
  struct termios *pterm_slave = my_tcgetattr(slave_pty_sensing_fd, "slave pty");
  char *sent_EOF = mysavestring("?");

  *sent_EOF = (pterm_slave && pterm_slave->c_cc[VEOF]  ? pterm_slave->c_cc[VEOF] : 4) ; /*@@@ HL shouldn't we directly mysavestring(pterm_slave->c_cc[VEOF]) ??*/
  DPRINTF1(DEBUG_TERMIO, "Sending %s", mangle_string_for_debug_log(sent_EOF, MANGLE_LENGTH));
  put_in_output_queue(sent_EOF);
  myfree(pterm_slave);
  free(sent_EOF);
}


/* @@@ The next fuction is probably superfluous */
void
write_EOL_to_master_pty(char *received_eol)
{
  struct termios *pterm_slave = my_tcgetattr(slave_pty_sensing_fd, "slave pty");
  char *sent_eol = mysavestring("?");

  *sent_eol = *received_eol;
  if (pterm_slave) { 
    switch (*received_eol) {
    case '\n':
      if (pterm_slave->c_iflag & INLCR)
        *sent_eol = '\r';
      break;
    case '\r':
      if (pterm_slave->c_iflag & IGNCR)
        return;
      if (pterm_slave->c_iflag & ICRNL)
        *sent_eol = '\n';
    }
  } 
  put_in_output_queue(sent_eol);
  myfree(pterm_slave);
  free(sent_eol);
}

void
completely_mirror_slaves_terminal_settings()
{
  
  struct termios *pterm_slave;
  DEBUG_RANDOM_SLEEP;
  pterm_slave = my_tcgetattr(slave_pty_sensing_fd, "slave pty");
  log_terminal_settings(pterm_slave);
  if (pterm_slave && tcsetattr(STDIN_FILENO, TCSANOW, pterm_slave) < 0 && errno != ENOTTY)
    ;   /* myerror(FATAL|USE_ERRNO, "cannot prepare terminal (tcsetattr error on stdin)"); */
  myfree(pterm_slave);
  DEBUG_RANDOM_SLEEP;
}

void
completely_mirror_slaves_output_settings() 
{
  struct termios *pterm_stdin, *pterm_slave;  
  DEBUG_RANDOM_SLEEP;
  pterm_stdin = my_tcgetattr(STDIN_FILENO, "stdin");
  pterm_slave = my_tcgetattr(slave_pty_sensing_fd, "slave pty");
  if (pterm_slave && pterm_stdin) { /* no error message -  we can be called while slave is already dead */
    pterm_stdin -> c_oflag = pterm_slave -> c_oflag;
    tcsetattr(STDIN_FILENO, TCSANOW, pterm_stdin);
  }     
  myfree(pterm_slave);
  myfree(pterm_stdin);
  DEBUG_RANDOM_SLEEP;
}



/* rlwrap's transparency depends on knowledge of the slave commands terminal settings, which are kept in its
   termios structure. In many cases it is enough to inspect those settings after the user has pressed a key
   (e.g.: user presses a key - rlwrap inspects slaves ECHO flag - rlwrap echoes the keypress (or not)

   But some keypresses are directly handled by the terminal driver, e.g.: user presses CTRL-C - driver
   inspects rlwrap's ISIG flag and c_cc[VINTR] character - driver sends a sigINT to rlwrap.

   Rlwrap doesn't get a chance to intervene - except by unsetting ISIG beforehand and handling the interrupt
   key itself. We cannot, however, just bind the key to a readline action self-insert(key), as this wouldn't
   interrupt an ongoing line edit.

   Rlwrap solves this problem by copying the relevant flags and special characters from the slave pty to
   stdin/stdout. Of course, ideally this should happen *before* the user presses her key. This can be done by
   calling rlwrap with the -W (--polling) option.
*/ 


/* Many termios flags and characters are serial-specific and will never be used on a pty
   Possible exceptions (cf: http://www.lafn.org/~dave/linux/termios.txt)
   ISTRIP IGNCR (and related flags) and all the c_cc characters (although only VINTR seems to be changed
   in practice, e.g. by emacs to CTRL-G)
*/  

void
completely_mirror_slaves_special_characters()
{
  struct termios *pterm_stdin, *pterm_slave;
  DEBUG_RANDOM_SLEEP;
  pterm_stdin = my_tcgetattr(STDIN_FILENO, "stdin");
  pterm_slave = my_tcgetattr(slave_pty_sensing_fd, "slave pty");
  if (pterm_slave && pterm_stdin) { /* no error message -  we can be called while slave is already dead */
    int isig_stdin = pterm_stdin -> c_lflag & ISIG;
    cc_t ic_stdin  = pterm_stdin -> c_cc[VINTR];
    int isig_slave = pterm_slave -> c_lflag & ISIG;
    cc_t ic_slave  = pterm_slave -> c_cc[VINTR];
    if ((isig_stdin == isig_slave) &&  (ic_stdin == ic_slave)) /* nothing to do */
      return;
    DPRINTF4(DEBUG_TERMIO,"stdin interrupt handling copied from slave: ISIG: %0x->%0x, VINTR: %0x->%0x", 
             isig_stdin, isig_slave, ic_stdin, ic_slave); 
    pterm_stdin -> c_cc[VINTR] = ic_slave;
    pterm_stdin -> c_lflag ^= (isig_slave ^ ISIG); /* copy ISIG bit from slave */ 
    tcsetattr(STDIN_FILENO, TCSANOW, pterm_stdin);
  }
  myfree(pterm_slave);
  myfree(pterm_stdin);
  DEBUG_RANDOM_SLEEP;
}


/* returns TRUE if the -N option has been specified, we can read /proc/<command_pid>/wchan,
   (which happens only on linux, as far as I know) and what we read there contains the word "wait"
   meaning (presumably...) that command is waiting for one of its children
   if otherwise returns FALSE
*/
int dont_wrap_command_waits() {
  static char command_wchan[MAXPATHLEN+1];
  static int initialised = FALSE;
  static int wchan_fd;
  static int been_warned = 0;
  char buffer[BUFFSIZE];
  int nread, result = FALSE;

  DEBUG_RANDOM_SLEEP;
  if (!commands_children_not_wrapped)
    return FALSE;
  if (!initialised) {   /* first time we're called after birth of child */
    snprintf2(command_wchan, MAXPATHLEN , "%s/%d/wchan", PROC_MOUNTPOINT, command_pid);
    initialised =  TRUE;
  }
  if (command_is_dead)
    return TRUE;  /* This is lazy!! signal may not have been delivered @@@ */
  wchan_fd =  open(command_wchan, O_RDONLY);
  if (wchan_fd < 0) { 
    if (been_warned++ == 0) {
      myerror(WARNING|USE_ERRNO, "you probably specified the -N (--no-children) option"
                                 " - but spying\non %s's wait status does not work on"
                                 " your system, as we cannot read %s", command_name, command_wchan);
    }
    return FALSE;
  }     


  if (((nread = read(wchan_fd, buffer, BUFFSIZE -1)) > 0)) {
    buffer[nread] =  '\0';
    DPRINTF1(DEBUG_READLINE, "read commands wchan: <%s> ", buffer);
    result = (strstr(buffer, "wait") != NULL);
  }
  close(wchan_fd);
  DEBUG_RANDOM_SLEEP;
  return result;
}       


int skip_rlwrap() { /* this function is called from sigTSTP signal handler. Is it re-entrant? */
  int retval = FALSE;
  DEBUG_RANDOM_SLEEP;
  if (dont_wrap_command_waits())
    retval = TRUE;
  else if (always_readline)
    retval =  FALSE;
  else if (slave_is_in_raw_mode())
    retval= TRUE;
  DEBUG_RANDOM_SLEEP;
  return retval;
}
