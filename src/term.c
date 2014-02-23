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
    e-mail:  hlub@knoware.nl
*/


#include "rlwrap.h"

/*global vars */
char term_eof;                  /* end_of_file char */
char term_stop;                 /* stop (suspend) key */
char *term_backspace;           /* backspace control seq  (or 0, if none defined in terminfo) */
char *term_cursor_hpos;
char *term_clear_screen;
char *term_cursor_up;
char *term_cursor_down;

int redisplay = 1;
int newline_came_last = TRUE; /* used to determine whether rlwrap needs to ouptut a newline at the very end */

struct termios saved_terminal_settings; /* original terminal settings */
int terminal_settings_saved = FALSE;    /*  saved_terminal_settings is valid */
struct winsize winsize; /* current window size */

static char *term_cr;           /* carriage return (or 0, if none defined in terminfo) */
static char *term_clear_line;
char *term_name;

static char *my_tgetstr (char *id) {
  char *term_string_buf = (char *)mymalloc(2048), *tb = term_string_buf;
  char *stringcap = tgetstr(id, &tb); /*  rl_get_termcap(id) should also get string capability but doesn't. Why? */
  char *retval = stringcap ? mysavestring(stringcap) : NULL; 
  DPRINTF2(DEBUG_TERMIO, "tgetstr(\"%s\") = %s", id, (stringcap ? mangle_string_for_debug_log(stringcap,20) : "NULL"));
  free(term_string_buf);
  return retval;
}

void
init_terminal(void)
{                               /* save term settings and determine term type */
  char *term_buf;
  int  tgetent_returnvalue;
  int  we_are_on_hp_ux11;

  if (!isatty(STDIN_FILENO))
    myerror("stdin is not a tty");
  if (tcgetattr(STDIN_FILENO, &saved_terminal_settings) < 0)
    myerror("tcgetattr error on stdin");
  else
    terminal_settings_saved = TRUE;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &winsize) < 0)
    myerror("Could not get terminal size");

  /* init some variables: */
  term_name = getenv("TERM");
  if (!term_name || strlen(term_name)==0) {
    mywarn("environment variable TERM not set, assuming vt100\nSet it to your real terminal name if rlwrap misbehaves"); 
    term_name = "vt100";
  }
 
  
  DPRINTF1(DEBUG_TERMIO, "TERM = %s", term_name);  
  term_buf = (char *)mymalloc(4096);
 
  term_backspace = NULL;
  term_cr = NULL;
  term_clear_line = NULL;
  term_cursor_hpos = NULL;
   
  
  we_are_on_hp_ux11 = tgetent(term_buf, "vt100") == 0 && tgetent(term_buf,"QzBgt57gr6xwxw") == -1; 
  tgetent_returnvalue = tgetent(term_buf, term_name);
  if (tgetent_returnvalue > 0 || 
      (tgetent_returnvalue == 0 && we_are_on_hp_ux11))  { /* On weird and scary HP-UX 11 succesful tgetent() returns 0 */  
    term_backspace      = my_tgetstr("le");
    term_cr             = my_tgetstr("cr");
    term_clear_line     = my_tgetstr("ce"); /* was: @@@ my_tgetstr("dl1") */
    term_clear_screen   = my_tgetstr("cl");
    term_cursor_hpos    = my_tgetstr("ch");
    if (term_cursor_hpos && !strstr(term_cursor_hpos, "%p")) {
      /* Some RedHat or Debian people with out-of sync devel packages will get problems when tgoto expects terminfo-style caps
         and their tgetstr returns termcap-style. This is a rather heavy-handed way of avoiding those problems: */    
      DPRINTF0(DEBUG_TERMIO, "term_cursor_hpos appears to be termcap-style, not terminfo; NULLing it");
      term_cursor_hpos = NULL;
    }   
    term_cursor_up      = my_tgetstr("up");
    term_cursor_down    = my_tgetstr("do");                                        
  } else {
    DPRINTF1(DEBUG_TERMIO, "tgetent returned %d", tgetent_returnvalue);
    errno=0; mywarn("your $TERM is '%s' but %s couldn't find it in the terminfo database. Expect some problems.", term_name, program_name);
  }     

  term_eof = saved_terminal_settings.c_cc[VEOF];
  term_stop = saved_terminal_settings.c_cc[VSTOP];

  DPRINTF1(DEBUG_TERMIO, "term_eof=%d", term_eof);
  free(term_buf);
}


void
set_echo(int yes)
{
  struct termios *pterm = my_tcgetattr(slave_pty_fd, "slave pty");      /* mimic terminal settings of client */

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
    ;   /* myerror ("cannot prepare terminal (tcsetattr error on stdin)"); */
  free(pterm);
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

int
cursor_hpos(int col)
{
  char *instantiated;
  assert(term_cursor_hpos != NULL);     /* caller has to make sure */
  instantiated = tgoto(term_cursor_hpos, 0, col); /* tgoto with a command that takes one parameter: parameter goes to 2nd arg ("vertical position"). */
  assert(instantiated);
  DPRINTF2(DEBUG_TERMIO, "tgoto(term_cursor_hpos, 0, %d) = %s", col, mangle_string_for_debug_log(instantiated, 20));
  tputs(instantiated, 1, my_putchar);
  return TRUE;
}

void
cr()
{
  if (term_cr)
    tputs(term_cr, 1, my_putchar);
  else
    my_putchar('\r');
}

void
clear_the_screen()
{                               /* clear_screen is a macro in term.h */
  int i;

  if (term_clear_screen)
    tputs(term_clear_screen, 1, my_putchar);
  else
    for (i = 0; i < winsize.ws_row; i++)
      my_putchar('\n');         /* poor man's clear screen */
}

void
clear_line()
{
  int i;
  int width = winsize.ws_col;
  char *p, *spaces;

  cr();
  if (term_clear_line)
    tputs(term_clear_line, 1, my_putchar);
  else {
    spaces = (char *) mymalloc(width + 1);
    for (i = 0, p = spaces; i < width; i++, p++)
      *p = ' ';
    *p = '\0';
    my_putstr(spaces);  /* poor man's clear line */
    free((void *)spaces);
  }
  cr();
}


void
curs_up()
{
  /* assert(term_cursor_up != NULL);    caller has to make sure */
  tputs(term_cursor_up, 1, my_putchar);
}

void
curs_down()
{
  assert(term_cursor_down != NULL);     /* caller has to make sure */
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
  DPRINTF2(DEBUG_TERMIO,"wrote %d bytes to stdout: %s", string_length,
           mangle_string_for_debug_log(string, MANGLE_LENGTH));
  if (string_length == 0)
    return;
  write_patiently(STDOUT_FILENO, string, string_length, "to stdout");
  newline_came_last = (string[string_length - 1] == '\n'); /* remember whethere newline came last */

}
  

static void test_termfunc(char *control_string, char *control_string_name, char* start, void (* termfunc)(), char *end)
{
  char *mangled_control_string =
    (control_string ?
     add3strings("\"", mangle_string_for_debug_log(control_string,20),"\"")  :
     mysavestring("NULL"));
  printf("\n%s = %s\n", control_string_name, mangled_control_string);
  if (!control_string)
    return;
  my_putstr(start);
  sleep(1);
  termfunc();
  my_putstr(end);
  sleep(1);
  free(mangled_control_string);
}       

static void backspace1 () { backspace(1); }
static void cursor_hpos4 () { cursor_hpos(4);}


void test_terminal()
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


