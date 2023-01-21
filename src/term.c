/*  term.c: terminal handling, cursor movement etc.
    (C) 2000-2007 Hans Lub

    This program is free software; you can redistribute it and/or modify
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

/* rlwrap needs a little bit of cursor acrobatics to cope with multi-line prompts, recovering from CTRL-Z and editors that may have used
the alternate screen. All such cursor movements are done via the functions in this file. In the past, rlwrap relied on only the termcap
library (or an emulation of it), and would fall back on reasonable defaults ("\r", "\b" etc) if this was not found.

In the future it will also use terminfo or even ncurses. But the fall-back to older libraries is still important, as rlwrap seems
to be especially needed on older machines                                                                                            */

/*global vars */
char term_eof;                       /* end_of_file char */
char term_stop;                      /* stop (suspend) key */
char *term_backspace;                /* backspace control seq  (or 0, if none defined in terminfo) */
char *term_cursor_hpos;              /* control seq to position cursor st given position on current line */
char *term_clear_screen;
char *term_cursor_up;
char *term_cursor_down;
char *term_cursor_left;              /* only used for debugging (the SHOWCURSOR macro)                */
char *term_cursor_right;             /* only used to emulate a missing term_cursor_hpos               */
char *term_smcup;                    /* smcup - char sequence to switch to  alternate screen          */
char *term_rmcup;                    /* rmcup - char sequence to return from alternate screen         */
char *term_rmkx;                     /* rmkx - char sequence to return from keyboard application mode */
char *term_enable_bracketed_paste;   /* If we write this to some terminals (xterm etc.) they  will    */
                                     /* "bracket" pasted input with \e[200 and \e201                  */
char *term_disable_bracketed_paste;
int term_has_colours;

int redisplay = 1;
int newline_came_last = TRUE; /* used to determine whether rlwrap needs to ouptut a newline at the very end */

struct termios saved_terminal_settings; /* original terminal settings */
int terminal_settings_saved = FALSE;    /*  saved_terminal_settings is valid */
struct winsize winsize; /* current window size */

static char *term_cr;           /* carriage return (or 0, if none defined in terminfo) */
static char *term_clear_line;
char *term_name;


static char *my_tgetstr (char *id, const char *capability_name) {
#ifdef HAVE_TERMCAP_H
  char *term_string_buf = (char *)mymalloc(2048), *tb = term_string_buf;
  char *stringcap = tgetstr(id, &tb); /*  rl_get_termcap(id) only gets capabilities used by readline */
  char *retval = stringcap ? mysavestring(stringcap) : NULL;
  DPRINTF3(DEBUG_TERMIO, "tgetstr(\"%s\") = %s (%s)", id, (stringcap ? M(stringcap) : "NULL"), capability_name);
  free(term_string_buf);
  return retval;
#else
  return NULL;
#endif
}

static char *my_tigetstr (char *tid, const char *capability_name) {
#ifdef HAVE_CURSES_H
  static int term_has_been_setup = FALSE;
  char *stringcap, *retval;
  if (!term_has_been_setup) {
     setupterm(term_name, STDIN_FILENO, (int *)0); /* like tgetent() before tgetstr(), we have to call setupterm() befor tigetstr() */
     term_has_been_setup = TRUE;
  }
  stringcap = tigetstr(tid);
  retval = stringcap ? mysavestring(stringcap) : NULL;
  DPRINTF3(DEBUG_TERMIO, "tigetstr(\"%s\") = %s (%s)", tid, (stringcap ? M(stringcap) : "NULL"), capability_name);
  return retval;
#else
  return NULL;
#endif
}

/* Get (copies of) escape codes ("string capabilities") by name. First try terminfo name, then termcap name  */
static char *
tigetstr_or_else_tgetstr(char *capname, char *tcap_code, const char *capability_name)
{
  char *retval;
  retval = my_tigetstr(capname, capability_name);
  if (!retval)
    retval = my_tgetstr(tcap_code, capability_name);
  return retval;
}

#define FALLBACK_TERMINAL "vt100" /* hard-coded terminal name to use when we don't know which terminal we're on */

void
init_terminal(void)
{                               /* save term settings and determine term type */
  char *term_buf = mymalloc(2048);
  int  we_have_stringcaps;
  int  we_are_on_hp_ux11;

  if (!isatty(STDIN_FILENO))
    myerror(FATAL|NOERRNO, "stdin is not a tty");
  if (tcgetattr(STDIN_FILENO, &saved_terminal_settings) < 0)
    myerror(FATAL|USE_ERRNO, "tcgetattr error on stdin");
  else
    terminal_settings_saved = TRUE;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &winsize) < 0)
    myerror(FATAL|USE_ERRNO, "Could not get terminal size");

  DPRINTF2(DEBUG_TERMIO, "winsize.ws_col (term width): %d; winsize.ws_row (term height): %d", winsize.ws_col, winsize.ws_row);
  if (winsize.ws_col == 0)
    myerror(FATAL|NOERRNO, "My terminal reports width=0 (is it emacs?)  I can't handle this, sorry!");

  /* init some variables: */
  term_name = getenv("TERM");
  DPRINTF1(DEBUG_TERMIO, "found TERM = %s", term_name);






  /* On weird and scary HP-UX 11 succesful tgetent() returns 0, so we need the followig to portably catch errors: */
  we_are_on_hp_ux11 = tgetent(term_buf, "vt100") == 0 && tgetent(term_buf,"QzBgt57gr6xwxw") == -1; /* assuming there is no terminal called Qz... */
  #define T_OK(retval) ((retval) > 0 || ((retval)== 0 && we_are_on_hp_ux11))

  /* if TERM is not set, use FALLBACK_TERMINAL */
  if (!term_name || strlen(term_name)==0) {
    myerror(WARNING|USE_ERRNO, "environment variable TERM not set, assuming %s", FALLBACK_TERMINAL);
    term_name = FALLBACK_TERMINAL;
  }

  /* If we cannot find stringcaps, use FALLBACK_TERMINAL  */
  if (! (we_have_stringcaps = T_OK(tgetent(term_buf, term_name))) && strcmp(term_name, FALLBACK_TERMINAL)) {
    myerror(WARNING|NOERRNO, "your $TERM is '%s' but %s couldn't find it in the terminfo database. We'll use '%s'",
                            term_name, program_name, FALLBACK_TERMINAL);
    term_name = FALLBACK_TERMINAL;
  }


  /* If we cannot find stringcaps, even for FALLBACK_TERMINAL, complain */
  if (!we_have_stringcaps && ! (we_have_stringcaps = T_OK(tgetent(term_buf, FALLBACK_TERMINAL))))
    myerror(WARNING|NOERRNO, "Even %s is not found in the terminfo database. Expect some problems.", FALLBACK_TERMINAL);

  DPRINTF1(DEBUG_TERMIO, "using TERM = %s", term_name);
  mysetenv("TERM", term_name);




  if (we_have_stringcaps)  {
    term_backspace      = tigetstr_or_else_tgetstr("cub1",   "le", "backspace");
    term_cr             = tigetstr_or_else_tgetstr("cr",     "cr", "carriage return");
    term_clear_line     = tigetstr_or_else_tgetstr("el",     "ce", "clear line");
    term_clear_screen   = tigetstr_or_else_tgetstr("clear",  "cl", "clear screen");
    term_cursor_hpos    = tigetstr_or_else_tgetstr("hpa",    "ch", "set cursor horizontal position");
    term_cursor_left    = tigetstr_or_else_tgetstr("cub1",   "le", "move cursor left");
    term_cursor_right   = tigetstr_or_else_tgetstr("cuf1",   "nd", "move cursor right");
    term_cursor_up      = tigetstr_or_else_tgetstr("cuu1",   "up", "move cursor up");
    term_cursor_down    = tigetstr_or_else_tgetstr("cud1",   "do", "move cursor down");
    term_has_colours    = tigetstr_or_else_tgetstr("initc", "Ic",  "initialise colour") ? TRUE : FALSE;

    /* the following codes are never output by rlwrap, but used to recognize the use of the the alternate screen by the client, and
       to filter out "garbage" that is coming from commands that use them */
    term_smcup          = tigetstr_or_else_tgetstr("smcup",  "ti", "switch to alternate screen");
    term_rmcup          = tigetstr_or_else_tgetstr("rmcup",  "te", "exit alternate screen");
    term_rmkx           = tigetstr_or_else_tgetstr("rmkx",   "ke", "leave keypad-transmit mode");

    /* there is no way (yet) to determine whether a terminal knows about bracketed_paste_enabled paste - we cannot use tigetstr(). "Dumb" terminals do not, of course */

    term_enable_bracketed_paste  = strings_are_equal(term_name, "dumb") ? NULL : "\033[?2004h";
    term_disable_bracketed_paste = strings_are_equal(term_name, "dumb") ? NULL : "\033[?2004l";


    if (!term_cursor_right) /* probably only on 'dumb' terminal */
      term_cursor_right = " ";

    /*
    term_colors         = tigetnum("colors");
    DPRINTF1(DEBUG_TERMIO, "terminal colors: %d", term_colors);
    */

  }

  if (!(term_cursor_hpos || term_cursor_right) || !term_cursor_up || !term_cursor_down)
    myerror(WARNING|NOERRNO, "Your terminal '%s' is not fully functional, expect some problems.", term_name);
  term_eof = saved_terminal_settings.c_cc[VEOF];
  term_stop = saved_terminal_settings.c_cc[VSTOP];

  DPRINTF1(DEBUG_TERMIO, "term_eof=%d", term_eof);
  free(term_buf);
}


void
set_echo(int yes)
{
  struct termios *pterm = my_tcgetattr(slave_pty_sensing_fd, "slave pty");      /* mimic terminal settings of client */

  if (!pterm)                   /* child has probably died */
    return;
  pterm->c_lflag &= ~ICANON;    /* except a few details... */
  pterm->c_cc[VMIN] = 1;
  pterm->c_cc[VTIME] = 0;
  if (yes)
    pterm->c_lflag |= ECHO;
  else                          /* no */
    pterm->c_lflag &= ~ECHO;

  log_terminal_settings(pterm);
  if (tcsetattr(STDIN_FILENO, TCSANOW, pterm) < 0 && errno != ENOTTY)
    { /* nothing */ }   /* myerror(FATAL|USE_ERRNO, "cannot prepare terminal (tcsetattr error on stdin)"); */
  free(pterm);
}


int
cursor_hpos(int col)
{
  char *instantiated;
  if (term_cursor_hpos) {
    instantiated = tgoto(term_cursor_hpos, 0, col); /* tgoto with a command that takes one parameter: parameter goes to 2nd arg ("vertical position"). */
    assert(instantiated);
    DPRINTF2(DEBUG_TERMIO, "tgoto(term_cursor_hpos, 0, %d) = %s", col, M(instantiated));
    tputs(instantiated, 1, my_putchar);
  } else {
    int i;
    cr();
    for (i = 0; i < col; i++)
      tputs(term_cursor_right, 1, my_putchar);
  }
  return TRUE;
}

void
cr(void)
{
  if (term_cr)
    tputs(term_cr, 1, my_putchar);
  else
    my_putchar('\r');
}

void
clear_the_screen(void)
{                               /* clear_screen is a macro in term.h */
  int i;

  if (term_clear_screen)
    tputs(term_clear_screen, 1, my_putchar);
  else
    for (i = 0; i < winsize.ws_row; i++)
      my_putchar('\n');         /* poor man's clear screen */
}

void
clear_line(void)
{
  int i;
  int width = winsize.ws_col;
  char *p, *spaces;

  cr();
  if (term_clear_line)
    tputs(term_clear_line, 1, my_putchar);
  else {
    spaces = (char *) mymalloc(width +1);
    for (i = 0, p = spaces; i < width; i++, p++)
      *p = ' ';
    *p = '\0';
    my_putstr(spaces);  /* poor man's clear line */
    free((void *)spaces);
  }
  cr();
}



void
backspace(int count)
{
  int i;

  if (term_backspace)
    for (i = 0; i < count; i++)
      tputs(term_backspace, 1, my_putchar);
  else
    for (i = 0; i < count; i++)
      my_putchar('\b');
}

void
curs_left(void)
{
  if (term_cursor_left)
    tputs(term_cursor_left, 1, my_putchar);
  else
    backspace(1);
}


void
curs_up(void)
{
  if (term_cursor_up)
    tputs(term_cursor_up, 1, my_putchar);
}

void
curs_down(void)
{
  if (term_cursor_down)
    tputs(term_cursor_down, 1, my_putchar);
}

int my_putchar(TPUTS_PUTC_ARGTYPE c)
{
  char ch = c;
  ssize_t nwritten = write(STDOUT_FILENO, &ch, 1);
  return (nwritten == -1 ? -1 : c);
}


void
my_putstr(const char *string)
{
  int string_length = strlen(string);
  DPRINTF2(DEBUG_TERMIO,"wrote %d bytes to stdout: %s", string_length, M(string));
  if (string_length == 0)
    return;
  write_patiently(STDOUT_FILENO, string, string_length, "to stdout");
  newline_came_last = (string[string_length - 1] == '\n'); /* remember whether newline came last */

}


static void test_termfunc(char *control_string, char *control_string_name, char* start, void (* termfunc)(void), char *end)
{
  char *mangled_control_string =
    (control_string ?
     add3strings("\"", mangle_string_for_debug_log(control_string,20),"\"")  :
     mysavestring("NULL"));
  printf("\n%s = %s\n", control_string_name, mangled_control_string);
  if (!control_string)
    printf("trying without suitable control string, fasten seatbelts and brace for impact... \n");
  my_putstr(start);
  sleep(1);
  termfunc();
  my_putstr(end);
  sleep(1);
  free(mangled_control_string);
}

static void backspace1 (void) { backspace(1); }
static void cursor_hpos4 (void) { cursor_hpos(4);}


void test_terminal(void)
{
  if (debug) {
    debug_fp = fopen(DEBUG_FILENAME, "w");
    setbuf(debug_fp, NULL);
  }
  init_terminal();
  printf("\nTerminal is \"%s\"\n", term_name);
  test_termfunc(term_backspace, "term_backspace", "This should print \"grape\": \ngras", &backspace1, "pe\n");
  test_termfunc(term_cr, "term_cr", "This should print \"lemon juice\": \napple", &cr, "lemon juice\n");
  test_termfunc(term_clear_line, "term_clear_line","This should print \"apple\": \npomegranate", &clear_line, "apple\n");
  test_termfunc(term_cursor_hpos, "term_cursor_hpos", "This should print \"pomegranate\": \npomelo", &cursor_hpos4, "granate\n");
  test_termfunc(term_cursor_up, "term_cursor_up", "This should print \"blackberry\": \n blueberry\n", &curs_up,"black\n");
  printf("\n");

}


