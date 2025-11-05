/*  readline.c: interacting with the GNU readline library 
    (C) 2000-2025 Hans Lub
    
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

/* global vars */
int remember_for_completion = FALSE;    /* whether we should put al words from in/output on the list */
char *multiline_separator = NULL;       /* character sequence to use in lieu of newline when storing multi-line input in a single history line */
char *pre_given = NULL;                 /* pre-given user input when rlwrap starts up */
struct rl_state saved_rl_state = { "", "", 0, 0, 0 };      /* saved state of readline */
bool bracketed_paste_enabled = FALSE;
static char return_key;                 /* Key pressed to enter line */
static bool invoked_by_operate_and_get_next = FALSE;
static int forget_line;
static char *colour_start, *colour_end;        /* colour codes */

int multiline_prompts = TRUE;

/* forward declarations */
static void line_handler(char *);
static void my_add_history(const char *);
static int my_accept_line(int, int);
static int my_accept_line_and_forget(int, int);
static int my_operate_and_get_next(int, int);
static void munge_file_in_editor(const char *filename, int lineno, int colno);
static Keymap getmap(const char *name);
static void bindkey(int key, rl_command_func_t *function, const char *function_name, const char *maplist);
#define RL_COMMAND_FUN(f) f, #f

/* readline bindable functions: */
static int munge_line_in_editor(int, int);
static int direct_keypress(int, int);
static int direct_prefix(int, int);
static int handle_hotkey(int, int);
static int handle_hotkey_without_history(int, int);

/* only useful while debugging: */
static int debug_ad_hoc(int,int);
static int dump_all_keybindings(int,int);



void
init_readline(char *UNUSED(prompt))
{
  DPRINTF1(DEBUG_READLINE, "Initialising readline version %x", rl_readline_version);   
  rl_add_defun("rlwrap-accept-line", my_accept_line,-1);
  rl_add_defun("rlwrap-accept-line-and-forget", my_accept_line_and_forget,-1);
  rl_add_defun("rlwrap-call-editor", munge_line_in_editor, -1);
  rl_add_defun("operate-and-get-next", my_operate_and_get_next, -1); /* Even if the readline doc doesn't say so, it seems to be OK to re-bind  existing readline bindable commands */
  rl_add_defun("rlwrap-direct-keypress", direct_keypress, -1);  
  rl_add_defun("rlwrap-direct-prefix", direct_prefix, -1);
  rl_add_defun("rlwrap-hotkey", handle_hotkey, -1);
  rl_add_defun("rlwrap-hotkey-without-history", handle_hotkey_without_history, -1);

  /* only useful while debugging */
  rl_add_defun("rlwrap-dump-all-keybindings", dump_all_keybindings,-1);
  rl_add_defun("rlwrap-debug-ad-hoc", debug_ad_hoc, -1);

  /* if someone's .inputrc binds a key to accept-line, make it use our own version in lieu of readline's */
  rl_add_defun("accept-line", my_accept_line, -1);
  
  
  /* put the next variable binding(s) *before* rl_initialize(), so they can be overridden */
  rl_variable_bind("blink-matching-paren","on");  

  /* NB: there are more bindable readline commands that accept a line (like operate-and-get-next) */
  bindkey('\n', RL_COMMAND_FUN(my_accept_line), "emacs-standard; vi-insert; vi-command"); 
  bindkey('\r', RL_COMMAND_FUN(my_accept_line), "emacs-standard; vi-insert; vi-command");
  
  bindkey(15, RL_COMMAND_FUN(my_accept_line_and_forget), "emacs-standard; vi-insert; vi-command");	/* ascii #15 (Control-O) is unused in readline's emacs and vi keymaps */
  if (multiline_separator) 
    bindkey(30, RL_COMMAND_FUN(munge_line_in_editor), "emacs-standard;vi-insert;vi-command");           /* CTRL-^: unused in vi-insert-mode, hardly used in emacs  (doubles arrow-up) */

  
  
  /* rl_variable_bind("gnah","gnerp"); It is not possible to create new readline variables (only functions) */
  rl_catch_signals = 0;
  rl_initialize();		/* This has to happen *before* we set rl_redisplay_function, otherwise
				   readline will not bother to call tgetent(), will be agnostic about terminal
				   capabilities and hence not be able to honour e.g. a set horizontal-scroll-mode off
				   in .inputrc */

  /* put the next variable binding(s) *after* rl_initialize(), so they cannot be overridden */
  if (rl_readline_version < 0x802)  /* in bracketed_paste_mode rl_deprep_terminal() prints BRACK_PASTE_FINI ("\033[?2004l\r") 
                                       which moves the cursor to the beginning of the line. In pre-8.2 readline,  this doesn't
                                       set _rl_last_c_pos to 0, which makes rl_redisplay() think that it is already past the prompt,
                                       making it overwrite the prompt that rlwrap has already printed. cf. https://github.com/hanslub42/rlwrap/issues/168                    */
      rl_variable_bind("enable-bracketed-paste","off");      
 
  using_history();
  rl_redisplay_function = my_redisplay;
  rl_completion_entry_function =
    (rl_compentry_func_t *) & my_completion_function;
  
  rl_catch_signals = FALSE;
  rl_catch_sigwinch = FALSE;

  saved_rl_state.input_buffer = mysavestring(pre_given ? pre_given : ""); /* Even though the pre-given input won't be displayed before the first 
                                                                             cooking takes place, we still want it to be accepted when the user 
                                                                             presses ENTER before that (e.g. because she already knows the 
                                                                             pre-given input and wants to accept that) */
  saved_rl_state.point = strlen(saved_rl_state.input_buffer);
  saved_rl_state.raw_prompt = mysavestring("");
  saved_rl_state.cooked_prompt = NULL;

  bracketed_paste_enabled = term_enable_bracketed_paste != NULL && strings_are_equal(rl_variable_value("enable-bracketed-paste"),"on");
                           
  DPRINTF2(DEBUG_READLINE, "var: %s, enabled: %s", rl_variable_value("enable-bracketed-paste"), term_enable_bracketed_paste);
  if (bracketed_paste_enabled) {
    DPRINTF0(DEBUG_READLINE, "bracketed-paste is enabled");
    my_putstr(term_enable_bracketed_paste);
  }
}


/* save readline internal state in rl_state, redisplay the prompt
   (so that client output gets printed at the right place) */
void
save_rl_state(void)
{
  free(saved_rl_state.input_buffer); /* free(saved_rl_state.raw_prompt) */;
  saved_rl_state.input_buffer = mysavestring(rl_line_buffer);
  /* saved_rl_state.raw_prompt = mysavestring(rl_prompt); */
 
  saved_rl_state.point = rl_point;      /* and point    */
  rl_line_buffer[0] = '\0';
  if (saved_rl_state.already_saved)
    return;
  saved_rl_state.already_saved = 1;
  rl_delete_text(0, rl_end);    /* clear line  (after prompt) */
  rl_point = 0;
  my_redisplay();               /* and redisplay (this time without user input, cf the comments for the line_handler() function below) */
  
  rl_free_undo_list();          /* prevent readline from reverting the most recently entered history item  */
  rl_callback_handler_remove(); /* restore original terminal settings */
  rl_deprep_terminal();
}


/* Restore readline internal state from rl_state.   */

void
restore_rl_state(void)
{
  
  char *newprompt;
  move_cursor_to_start_of_prompt(impatient_prompt ? ERASE : DONT_ERASE); /* we do this before cooking, as the uncooked prompt may be longer than the cooked one */
  cook_prompt_if_necessary();
  newprompt =  mark_invisible(saved_rl_state.cooked_prompt); /* bracket (colour) control sequences with \001 and \002 */
  DPRINTF2(DEBUG_READLINE, "newprompt: <%s>, cooked prompt: <%s>", M(newprompt), M(saved_rl_state.cooked_prompt));
  rl_expand_prompt(newprompt);
  DPRINTF1(DEBUG_READLINE, "newprompt after rl_expand_prompt(): <%s>", M(newprompt));
  mirror_slaves_echo_mode();    /* don't show passwords etc */
  
  DPRINTF1(DEBUG_READLINE,"newprompt now <%s>", M(newprompt));
  rl_callback_handler_install(newprompt, &line_handler);
  free(newprompt);             /* readline docs don't say it, but we can free newprompt now (readline apparently
                                  uses its own copy) */
  rl_insert_text(saved_rl_state.input_buffer);
  rl_point = saved_rl_state.point;
  saved_rl_state.already_saved = 0;
  rl_redisplay(); 
  rl_prep_terminal(1);
  prompt_is_still_uncooked =  FALSE; /* has been done right now */
}


/* display (or remove, if message == NULL) message in echo area, with the appropriate bookkeeping */
void
message_in_echo_area(char *message)
{
  static int message_in_echo_area = FALSE;
  DPRINTF1(DEBUG_READLINE, "message: %s", M(message));
  if (message) {
    rl_save_prompt();
    message_in_echo_area = TRUE;  
    rl_message("%s", message);
  }  else {
    if (message_in_echo_area)
      rl_restore_prompt();
    rl_clear_message();
    message_in_echo_area = FALSE;
  }     
} 

#ifndef HAVE_RL_VARIABLE_VALUE /* very ancient readline? */
#  define rl_variable_value(s) "off"
#endif


static void
line_handler(char *line)
{
  char *rewritten_line, *filtered_line;
  bool history_can_safely_be_extended = !invoked_by_operate_and_get_next ||  history_duplicate_avoidance_policy != ELIMINATE_ALL_DOUBLES;
  DPRINTF1(DEBUG_HISTORY, "history_can_safely_be_extended: %d", history_can_safely_be_extended);
  if (line == NULL) {           /* EOF on input, forward it  */
    DPRINTF1(DEBUG_READLINE, "EOF detected, writing character %d", term_eof);
    /* colour_the_prompt = FALSE; don't mess with the cruft that may come out of dying command @@@ but command may not die!*/
    write_EOF_to_master_pty();
  } else {
    /* NB: with bracketed-paste, "line" may actually contain newline characters */
    DPRINTF2(DEBUG_READLINE, "return_key: %d , invoked_by_operate_and_get_next: %d" , return_key, invoked_by_operate_and_get_next);

         
    if (*line &&                 /* forget empty lines  */
        redisplay &&             /* forget passwords    */
        !forget_line &&          /* forget lines entered by CTRL-O */
        !match_regexp(line, forget_regexp, TRUE) &&
        history_can_safely_be_extended) {     /* forget lines (case-inseitively) matching -g option regexp */ 
      my_add_history(line); /* if line consists of multiple lines, each line is added to history separately. Is this documented somewhere? */
    }
    
    forget_line = FALSE; /* until CTRL-O is used again */

    /* Time for a design decision: when we send the accepted line to the client command, it will most probably be echoed
       back. We have two choices: we leave the entered line on screen and suppress just enough of the clients output (I
       believe this is what rlfe does), or we remove the entered input (but not the prompt!) and let it be replaced by
       the echo.

       This is what we do; as a bonus, if the program doesn't echo, e.g. at a password prompt, the **** which has been
       put there by our homegrown_redisplay function will be removed (@@@is this what we want?)

       I think this method is more natural for multi-line input as well, (we will actually see our multi-line input as
       multiple lines) but not everyone will agree with that.
          
       O.K, we know for sure that cursor is at start of line (after the prompt, or at position 0, if bracketed paste is
       enabled)). When clients output arrives, it will be printed at just the right place - but first we 'll erase the
       user input (as it may be about to be changed by the filter) */

    rl_delete_text(0, rl_end);  /* clear line  (after prompt) */
    rl_point = 0;
    my_redisplay();             /* and redisplay (now without user input, which will be echoed back, cf. comments above) */
    rewritten_line =
      (multiline_separator ? 
       search_and_replace(multiline_separator, "\n", line, 0, NULL,
                          NULL) : mysavestring(line));


    
      
    if (redisplay)
      filtered_line = pass_through_filter(TAG_INPUT, rewritten_line);
    else { /* password, none of filters business */
      pass_through_filter(TAG_INPUT, "***"); /* filter some input anyway, to keep filter in sync (result is discarded).  */
      filtered_line = mysavestring(rewritten_line);
    }   
    free(rewritten_line);
        
    
    /* do we have to adapt clients winzsize now? */
    if (deferred_adapt_commands_window_size) {
      adapt_tty_winsize(STDIN_FILENO, master_pty_fd);
      kill(-command_pid, SIGWINCH);
      deferred_adapt_commands_window_size = FALSE;
    }


    
    /*OK, now feed line to underlying command and wait for the echo to come back */
    put_in_output_queue(filtered_line);
    DPRINTF2(DEBUG_READLINE, "putting %d bytes %s in output queue", (int) strlen(rewritten_line),
             M(rewritten_line));
    write_EOL_to_master_pty((return_key && !invoked_by_operate_and_get_next) ? &return_key : "\n");

    accepted_lines++;
    free_foreign(line);         /* free_foreign because line was malloc'ed by readline, not by rlwrap */
    free(filtered_line);        /* we're done with them  */
        
    return_key = 0;
    invoked_by_operate_and_get_next = FALSE;
    within_line_edit = FALSE;
    if(!RL_ISSTATE(RL_STATE_MACROINPUT)) /* when called during playback of a multi-line macro, line_handler() will be called more 
                                            than once whithout re-entering main_loop(). If we'd remove it here, the second call
                                            would crash  */
    rl_free_undo_list();                 /* prevent readline from reverting the most recently entered history item  */
    rl_callback_handler_remove();
    set_echo(FALSE);
    free(saved_rl_state.input_buffer);
    free(saved_rl_state.raw_prompt);
    free(saved_rl_state.cooked_prompt); 
   
 
    saved_rl_state.input_buffer = mysavestring("");
    saved_rl_state.raw_prompt = mysavestring("");
    saved_rl_state.cooked_prompt = NULL;
    saved_rl_state.point = 0;
    saved_rl_state.already_saved = TRUE;
    redisplay  = TRUE;


    if (one_shot_rlwrap)
      write_EOF_to_master_pty();

    /* readline only outputs term_enable_bracketed_paste when we call rl_prep_terminal(). That's too   */
    /* late for us, as we only call rl_prep_terminal *after* we have received user input               */

    if (bracketed_paste_enabled)
       my_putstr(term_enable_bracketed_paste); 
  }
}


/* this function (drop-in replacement for readline's own accept-line())
   will be bound to RETURN key: */
static int
my_accept_line(int UNUSED(count), int key)
{
  rl_point = 0;			/* leave cursor on predictable place */
  my_redisplay();
  rl_done = 1;
  return_key = (char)key;
  return 0;
}

static int
my_operate_and_get_next(int count, int key)
{
#ifdef HAVE_RL_OPERATE_AND_GET_NEXT
  rl_operate_and_get_next(count, key);
  curs_up(); /* Kludge alert! rl_operate_and_get_next() calls rl_newline() directly, while line_handler() expects the cursor  to still be on the original command line */ 
  rl_point = 0;			/* leave cursor on predictable place */
  return_key = (char) key;
  my_redisplay();
  invoked_by_operate_and_get_next = TRUE;
#else
  MAYBE_UNUSED(count);
  MAYBE_UNUSED(key);
#endif
  return 0;
}



/* this function will be bound to rlwrap-accept-line-and-forget (CTRL-O by default) */
static int
my_accept_line_and_forget(int count, int UNUSED(key))
{
  forget_line = 1;
  return my_accept_line(count, '\n');
}

static int
dump_all_keybindings(int count, int key)
{
  rl_dump_functions(count,key);
  rl_variable_dumper(FALSE);
  rl_macro_dumper(FALSE);
  return 0;
}       


void log_history_info(int lookback, const char* tag) {
  #ifndef DEBUG
    return;
  #else
    int count,here;
    if (debug & DEBUG_HISTORY) {
      DPRINTF3(DEBUG_HISTORY, "(%s) %d history items, from #%d and back:", tag, lookback, history_length);
      for (count = 0, here = history_length - 1;
           count < lookback ;
           count++, here--) {
        DPRINTF4(DEBUG_HISTORY,"(%s) item %d (base: %d): <%s>", tag, here, history_base, history_get(history_base + here)->line); 
      }
    }
  #endif
}


/* filter line and add it to history list, avoiding duplicates if necessary */
static void
my_add_history(const char *line)
{       
  int lookback, count, here;
  char *new_entry, *filtered_line, **lineptr, **list;


  /* with bracketed-paste mode, line may actually be multiple lines. (Is that so??) We'll treat each of those lines */
  /* as a separate history item (I believe that is what bash does as well).                                         */

  DPRINTF1(DEBUG_HISTORY, "my_add_history: %s", line);
  list = split_with(line,"\n"); 
  for (lineptr = list; *lineptr; lineptr++) {
    filtered_line =  pass_through_filter(TAG_HISTORY, *lineptr);
    DPRINTF1(DEBUG_HISTORY, "after splitting (and filtering) %s", filtered_line);
    
  
    switch (history_duplicate_avoidance_policy) { 
    case KEEP_ALL_DOUBLES:
      lookback = 0; break;
    case ELIMINATE_SUCCESIVE_DOUBLES:
      lookback = 1; break;
    case ELIMINATE_ALL_DOUBLES:
      lookback = history_length; break;
    default: lookback = 0;
    }

  
    new_entry = filtered_line;    
  
    lookback = min(history_length, lookback);      
    for (count = 0, here = history_length - 1;
         count < lookback ;
         count++, here--) {
      DPRINTF4(DEBUG_HISTORY, "comparing <%s> and <%s> (count = %d, here = %d)", line
             , history_get(history_base + here)->line ,count, here);
      if (strncmp(new_entry, history_get(history_base + here) -> line, 10000) == 0) { /* history_get uses the logical offset history_base .. */
        HIST_ENTRY *entry = remove_history (here);                                   /* .. but remove_history doesn't!                      */
        DPRINTF2(DEBUG_HISTORY, "removing duplicate entry #%d (%s)", here, entry->line);
        free_foreign(entry->line);
        free_foreign(entry);
      }
    }
    add_history(new_entry);
    free(new_entry);
  }
  free_splitlist(list);
}
  
/* Homegrown redisplay function - erases current line and prints the
   new one.  Used for passwords (where we want to show **** instead of
   user input) and whenever HOMEGROWN_REDISPLAY is defined (for
   systems where rl_redisplay() misbehaves, like sometimes on
   Solaris). Otherwise we use the much faster and smoother
   rl_redisplay() This function cannot display multiple lines: it will
   only scroll horizontally (even if horizontal-scroll-mode is off in
   .inputrc)
*/


static void
my_homegrown_redisplay(int hide_passwords)
{
  static int line_start = 0;    /* at which position of prompt_plus_line does the printed line start? */
  static int line_extends_right = 0;
  static int line_extends_left = 0;
  static char *previous_line = NULL;
  
  
  int width = winsize.ws_col;
  int skip = max(1, min(width / 5, 10));        /* jumpscroll this many positions when cursor reaches edge of terminal */
  
  char *prompt_without_ignore_markers;
  int colourless_promptlen = colourless_strlen(rl_prompt, &prompt_without_ignore_markers, 0, 0, NULL);
  int promptlen = strlen(prompt_without_ignore_markers);
  int invisible_chars_in_prompt = promptlen - colourless_promptlen;
  char *prompt_plus_line = add2strings(prompt_without_ignore_markers, rl_line_buffer);
  char *new_line;
  int total_length = strlen(prompt_plus_line);
  int curpos = promptlen + rl_point; /* cursor position within prompt_plus_line */
  int i, printed_length,
    new_curpos,                    /* cursor position on screen */
    keep_old_line, vlinestart, printwidth, last_column;

  /* In order to handle prompt with colour we either print the whole prompt, or start past it:
     starting in the middle is too difficult (i.e. I am too lazy) to get it right.
     We use a "virtual line start" vlinestart, which is the number of invisible chars in prompt in the former case, or
     linestart in the latter (which then must be >= strlen(prompt))

     At all times (before redisplay and after) the following is true:
     - the cursor is at column (curpos - vlinestart) (may be < 0 or > width)
     - the character under the cursor is prompt_plus_line[curpos]
     - the character at column 0 is prompt_plus_line[linestart]
     - the last column is at <number of printed visible or invisible chars> - vlinestart
     
     the goal of this function is to display (part of) prompt_plus_line such
     that the cursor is visible again */
     
  
  if (hide_passwords)
    for (i = promptlen; i < total_length; i++)
      prompt_plus_line[i] = '*';        /* hide a pasword by making user input unreadable  */


  if (rl_point == 0)            /* (re)set  at program start and after accept_line (where rl_point is zeroed) */
    line_start = 0;
  assert(line_start == 0 || line_start >= promptlen); /* the line *never* starts in the middle of the prompt (too complicated to handle)*/
  vlinestart = (line_start > promptlen ? line_start : invisible_chars_in_prompt); 
  

  if (curpos - vlinestart > width - line_extends_right) /* cursor falls off right edge ?   */
    vlinestart = (curpos - width + line_extends_right) + skip;  /* jumpscroll left                 */

  else if (curpos < vlinestart + line_extends_left) {   /* cursor falls off left edge ?    */
    if (curpos == total_length) /* .. but still at end of line?    */
      vlinestart = max(0, total_length - width);        /* .. try to display entire line   */
    else                        /* in not at end of line ..        */
      vlinestart = curpos - line_extends_left - skip; /* ... jumpscroll right ..         */
  }     
  if (vlinestart <= invisible_chars_in_prompt) {
    line_start = 0;             /* ... but not past start of line! */
    vlinestart = invisible_chars_in_prompt;
  } else if (vlinestart > invisible_chars_in_prompt && vlinestart <= promptlen) {
    line_start = vlinestart = promptlen;
  } else {
    line_start = vlinestart;
  }

  printwidth = (line_start > 0 ? width : width + invisible_chars_in_prompt);
  printed_length = min(printwidth, total_length - line_start);  /* never print more than width     */
  last_column = printed_length - vlinestart;


  /* some invariants :     0 <= line_start <= curpos <= line_start + printed_length <= total_length */
  /* these are interesting:   ^                                                      ^              */

  assert(0 <= line_start);
  assert(line_start <= curpos);
  assert(curpos <= line_start + printed_length);        /* <=, rather than <, as cursor may be past eol   */
  assert(line_start + printed_length <= total_length);


  new_line = prompt_plus_line + line_start;
  new_line[printed_length] = '\0';
  new_curpos = curpos - vlinestart;

  /* indicate whether line extends past right or left edge  (i.e. whether the "interesting
     inequalities marked ^ above are really unequal) */

  line_extends_left = (line_start > 0 ? 1 : 0);
  line_extends_right = (total_length - vlinestart > width ? 1 : 0);
  if (line_extends_left)
    new_line[0] = '<';
  if (line_extends_right)
    new_line[printwidth - 1] = '>';

  

  keep_old_line = FALSE;
  if (term_cursor_hpos) {
    if (previous_line && strcmp(new_line, previous_line) == 0) {
      keep_old_line = TRUE;
    } else {
      if (previous_line)
        free(previous_line);
      previous_line = mysavestring(new_line);
    }
  }
  /* keep_old_line = TRUE; */
  if (!keep_old_line) {
    clear_line();
    cr();
    write_patiently(STDOUT_FILENO, new_line, printed_length, "to stdout");
  }
  
  assert(term_cursor_hpos || !keep_old_line);   /* if we cannot position cursor, we must have reprinted ... */

  if (term_cursor_hpos)
    cursor_hpos(new_curpos);
  else                          /* ... so we know we're 1 past last position on line */
    backspace(last_column - new_curpos);
  free(prompt_plus_line);
  free(prompt_without_ignore_markers);
}




void
my_redisplay(void)
{
  int debug_force_homegrown_redisplay = 0;

#ifdef DEBUG
  debug_force_homegrown_redisplay = debug & FORCE_HOMEGROWN_REDISPLAY;
#endif
  
#ifndef HOMEGROWN_REDISPLAY
  if (redisplay && !debug_force_homegrown_redisplay) {
    rl_redisplay();
  } else
#endif
    my_homegrown_redisplay(!redisplay);
}


/* (setq font-lock-maximum-decoration t) */

static void
munge_file_in_editor(const char *filename, int lineno, int colno)
{

  int ret;
  char *editor_command1, *editor_command2, *editor_command3,
       *editor_command4, *line_number_as_string, *column_number_as_string,  **possible_editor_commands;

  /* find out which editor command we have to use */
  possible_editor_commands = list4(getenv("RLWRAP_EDITOR"), getenv("EDITOR"), getenv("VISUAL"), "vi +%L");
  editor_command1 = first_of(possible_editor_commands);
  line_number_as_string = as_string(lineno);
  column_number_as_string = as_string(colno);
  editor_command2 = search_and_replace("%L", line_number_as_string, editor_command1, 0, NULL, NULL);
  editor_command3 = search_and_replace("%C", column_number_as_string, editor_command2, 0, NULL, NULL);
  editor_command4 = strstr(editor_command3, "%F") ?
    search_and_replace("%F", filename, editor_command3, 0, NULL, NULL) :
    add3strings(editor_command3, " ", filename);
  

  /* call editor, temporarily restoring terminal settings */    
  if (terminal_settings_saved && (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_terminal_settings) < 0))    /* reset terminal */
    myerror(FATAL|USE_ERRNO, "tcsetattr error on stdin");
  DPRINTF1(DEBUG_READLINE, "calling %s", editor_command4);
  if ((ret = system(editor_command4))) {
    if (WIFSIGNALED(ret)) {
      fprintf(stderr, "\n"); 
      myerror(FATAL|NOERRNO, "editor killed by signal");
    } else {    
      myerror(FATAL|USE_ERRNO, "failed to invoke editor with '%s'", editor_command4);
    }
  }
  completely_mirror_slaves_terminal_settings();
  ignore_queued_input = TRUE;  

  free_multiple(possible_editor_commands, editor_command2, editor_command3,
                editor_command4, line_number_as_string, column_number_as_string, FMEND);
}


static int
munge_line_in_editor(int UNUSED(count), int UNUSED(key))
{
  int line_number = 0, column_number = 0, tmpfile_fd, bytes_read;
  size_t tmpfilesize;
  char *p, *tmpfilename, *text_to_edit;
  char *input, *rewritten_input, *rewritten_input2;


  if (!multiline_separator)
    return 0;

  tmpfile_fd = open_unique_tempfile(multi_line_tmpfile_ext, &tmpfilename);

  text_to_edit =
    search_and_replace(multiline_separator, "\n", rl_line_buffer, rl_point,
                       &line_number, &column_number);
  write_patiently(tmpfile_fd, text_to_edit, strlen(text_to_edit), "to temporary file");

  if (close(tmpfile_fd) != 0) /* improbable */
    myerror(FATAL|USE_ERRNO, "couldn't close temporary file %s", tmpfilename); 

  munge_file_in_editor(tmpfilename, line_number, column_number);
  
  /* read back edited input, replacing real newline with substitute */
  tmpfile_fd = open(tmpfilename, O_RDONLY);
  if (tmpfile_fd < 0)
    myerror(FATAL|USE_ERRNO, "could not read temp file %s", tmpfilename);
  tmpfilesize = filesize(tmpfilename);
  input = mymalloc(tmpfilesize + 1);
  bytes_read = read(tmpfile_fd, input, tmpfilesize);
  if (bytes_read < 0)
    myerror(FATAL|USE_ERRNO, "unreadable temp file %s", tmpfilename);
  input[bytes_read] = '\0';
  rewritten_input = search_and_replace("\t", "    ", input, 0, NULL, NULL);     /* rlwrap cannot handle tabs in input lines */
  rewritten_input2 =
    search_and_replace("\n", multiline_separator, rewritten_input, 0, NULL, NULL);
  for(p = rewritten_input2; *p ;p++)
    if(*p >= 0 && *p < ' ') /* @@@FIXME: works for UTF8, but not UTF16 or UTF32 (Mention this in manpage?)*/ 
      *p = ' ';        /* replace all control characters (like \r) by spaces */


  rl_delete_text(0, strlen(rl_line_buffer));
  rl_point = 0;  
  clear_line();
  cr();
  my_putstr(saved_rl_state.cooked_prompt);
  rl_insert_text(rewritten_input2);
  rl_point = 0;                 /* leave cursor on predictable place */
  rl_done = 1;                  /* accept line immediately */

  


  /* wash those dishes */
  if (unlink(tmpfilename))
    myerror(FATAL|USE_ERRNO, "could not delete temporary file %s", tmpfilename);

  free(tmpfilename);
  free(text_to_edit);
  free(input);
  free(rewritten_input);
  free(rewritten_input2);
  
  return_key = (char)'\n';
  return 0;
}

static int
direct_keypress(int UNUSED(count), int key)
{
#ifdef HAVE_RL_EXECUTING_KEYSEQ /* i.e. if readline version is >= 6.3 */
  MAYBE_UNUSED(key);
  DPRINTF1(DEBUG_READLINE,"direct keypress: %s", M(rl_executing_keyseq));
  put_in_output_queue(rl_executing_keyseq);
#else
  char *key_as_str = mysavestring("?"); /* ? is just a placeholder */
  *key_as_str = key;
  DPRINTF1(DEBUG_READLINE,"direct keypress: %s", mangle_char_for_debug_log(key, TRUE));
  put_in_output_queue(key_as_str);
  free(key_as_str);
#endif
  return 0;
}

static int
direct_prefix(int count, int key)
{
  char *key_as_str = mysavestring("?"); /* ? is just a placeholder */
  
  /* process the keypress used to invoke the function */
  direct_keypress(count, key);
  /* read an extra key to pass on */
  key = rl_read_key();
  *key_as_str = key;
  DPRINTF1(DEBUG_READLINE,"read key: %s", mangle_char_for_debug_log(key, TRUE));
  put_in_output_queue(key_as_str);
  free(key_as_str);

  return 0;
}

static char* entire_history_as_one_string(void) {
  HIST_ENTRY **the_list = history_list(), **entryp;
  char *big_string = mymalloc(history_total_bytes() + history_length + 1);
  char *stringp =  big_string;

  if (!the_list) /* i.e. if there is no history */
    return mysavestring("");

  for (entryp = the_list; *entryp; entryp++) {
    int length = strlen((*entryp)->line);
    strcpy(stringp, (*entryp)->line); 
    stringp +=length;
    *stringp++ = '\n';
  }
  *stringp = '\0';
  DPRINTF1(DEBUG_READLINE, "stringified %d bytes of history", (int) strlen(big_string));
  return big_string;
}       

static int
debug_ad_hoc(int UNUSED(count), int UNUSED(hotkey))
{
  printf("\n%s", entire_history_as_one_string());
  cleanup_rlwrap_and_exit(EXIT_SUCCESS);
  return 42;
}       
  


static int
handle_hotkey2(int UNUSED(count), int hotkey, int without_history)
{
  char *prefix, *postfix, *history,  *histpos_as_string, *executing_keyseq;
  char *new_prefix, *new_postfix, *new_history, *new_histpos_as_string, *message; 
  char *filter_food, *filtered, **fragments,  *new_rl_line_buffer;
  int nfilter_fields, length, new_histpos;
  unsigned long int hash;

  static const unsigned int MAX_HISTPOS_DIGITS = 6; /* one million history items should suffice */

  
#ifdef HAVE_RL_EXECUTING_KEYSEQ /* i.e. if readline version is >= 6.3 */
  executing_keyseq = mysavestring(rl_executing_keyseq);
#else
  executing_keyseq = mysavestring("?");
  *executing_keyseq = hotkey; /* The filter will only get the *last* byte of the key sequence that triggered rl_handle_hotkey */ 
#endif

    
  DPRINTF3(DEBUG_READLINE, "hotkey press (without_history == %d): %x (%s)", without_history, hotkey, M(executing_keyseq));

  prefix = mysavestring(rl_line_buffer);
  prefix[rl_point] = '\0';                                     /* chop off just before cursor */
  postfix = mysavestring(rl_line_buffer + rl_point);

  if (without_history) {
    histpos_as_string = mysavestring("0");
    history = mysavestring("");
  } else {
    histpos_as_string = as_string(where_history());
    assert(strlen(histpos_as_string) <= MAX_HISTPOS_DIGITS);
    history = entire_history_as_one_string();
    hash = hash_multiple(2, history, histpos_as_string);
  }     

  /* filter_food = <keyseq_length><keyseq><prefix_length><prefix><postfix_length><postfix><history_length><history><histpos_length><histpos> + '\0' */
  filter_food = merge_fields(executing_keyseq, prefix, postfix, history, histpos_as_string, END_FIELD); /* this is the format that the filter expects */

  /* let the filter filter ...! */
  filtered= pass_through_filter(TAG_HOTKEY, filter_food);
  
  /* OK, we now have to read back everything. After splitting the message, here should be exactly 5 components*/
  fragments = split_filter_message(filtered, &nfilter_fields);
  if (nfilter_fields != 5)
    myerror(FATAL, "found %d fields (expected 5) in filtered hotkey message <%s>", nfilter_fields, filtered); 
  message               = fragments[0]; 
  new_prefix            = fragments[1];
  new_postfix           = fragments[2];
  new_history           = fragments[3];
  new_histpos_as_string = fragments[4];

  if (!without_history && hash_multiple(2, new_history, new_histpos_as_string) != hash) { /* history has been rewritten */
    char **linep, **history_lines = split_on_single_char(new_history, '\n', 0);
    DPRINTF3(DEBUG_READLINE, "hash=%lx, new_history is %d bytes long, histpos <%s>", hash, (int) strlen(new_history), new_histpos_as_string);
    clear_history();
    for (linep = history_lines; *linep; linep++) 
      add_history(*linep);
    new_histpos = my_atoi(new_histpos_as_string);
    history_set_pos(new_histpos);
    free_splitlist(history_lines);
  }
  new_rl_line_buffer = add2strings(new_prefix, new_postfix);

  if ( (length = strlen(new_rl_line_buffer)) > 0  &&  new_rl_line_buffer[length - 1] == '\n') {
    new_rl_line_buffer[length - 1] = '\0';
    rl_done = TRUE;
    return_key = (char) '\n';
    assert(strchr(new_rl_line_buffer, '\n') == NULL);
  }

  rl_delete_text(0, strlen(rl_line_buffer));
  rl_point = 0;
  rl_insert_text(new_rl_line_buffer);
  rl_point = strlen(new_rl_line_buffer);

  
  
  if (*message && *message != hotkey) {                          /* if message has been set (i.e. != hotkey) , and isn't empty: */ 
    message = append_and_free_old(mysavestring(message), " ");   /* put space (for readability) between the message and the input line .. */
    message_in_echo_area(message);                          /* .. then write it to echo area */
  }     

  clear_line();
  rl_on_new_line();
  rl_redisplay();
 

  free_splitlist(fragments);                                   /* this will free all the fragments (and the list itself) in one go  */
  free_multiple(prefix, postfix, filter_food, executing_keyseq, filtered, new_rl_line_buffer, history, histpos_as_string, FMEND);
  return 0;
}


static int
handle_hotkey(int count, int hotkey)
{
  return handle_hotkey2(count, hotkey, FALSE);
}       


static int
handle_hotkey_without_history(int count, int hotkey)
{
  return handle_hotkey2(count, hotkey, TRUE);
}       


void
initialise_colour_codes(char *colour)
{
  int attributes, foreground, background;
  DPRINTF1(DEBUG_READLINE, "initialise_colour_codes(\"%s\")", colour);
  attributes = foreground = -1;
  background = 40; /* don't need to specify background; 40 passes the test automatically */
  sscanf(colour, "%d;%d;%d", &attributes, &foreground, &background);
  
#define OUTSIDE(lo,hi,val) (val < lo || val > hi) 
  if (OUTSIDE(0,8,attributes) || OUTSIDE(30,37,foreground) || OUTSIDE(40,47,background))
    myerror(FATAL|NOERRNO, "\n"
            "  prompt colour spec should be <attr>;<fg>[;<bg>]\n"
            "  where <attr> ranges over [0...8], <fg> over [30...37] and <bg> over [40...47]\n"
            "  example: 0;33 for yellow on current background, 1;31;40 for bold red on black ");
  colour_start= add3strings("\033[", colour,"m");
  colour_end  = "\033[0m";
}

/* returns a colourised copy of prompt, trailing space is not colourised */
char*
colourise (const char *prompt)
{
  char *prompt_copy, *trailing_space, *colour_end_with_space, *result, *p;
  prompt_copy = mysavestring(prompt);
  /* if (strchr(prompt_copy, '\033') || strchr(prompt_copy, RL_PROMPT_START_IGNORE) ) {     /\* prompt contains escape codes? *\/ */
  /*   DPRINTF1(DEBUG_READLINE, "colourise %s: left as-is", prompt); */
  /*   return prompt_copy; /\* if so, leave prompt alone  *\/ */
  /* } */
  for (p = prompt_copy + strlen(prompt_copy); p > prompt_copy && *(p-1) == ' '; p--)
    ; /* skip back over trailing space */
  trailing_space = mysavestring(p); /* p now points at first trailing space, or else the final NULL */
  *p = '\0';
  colour_end_with_space = add2strings(colour_end, trailing_space);
  result = add3strings(colour_start, prompt_copy, colour_end_with_space);
  free (prompt_copy); free(trailing_space); free(colour_end_with_space);
  DPRINTF1(DEBUG_READLINE, "colourise %s: colour added ", prompt);
  return result;
}


void
move_cursor_to_start_of_prompt(int erase)
{
  int termwidth = winsize.ws_col;
  int promptlen_on_screen, number_of_lines_in_prompt, curpos, count;
  int ATTR_ONLY_FOR_DEBUGGING cooked = (saved_rl_state.cooked_prompt != NULL);
  
  DPRINTF2(DEBUG_READLINE,"prompt_is_still_uncooked: %d, impatient_prompt: %d", prompt_is_still_uncooked, impatient_prompt);
  if (prompt_is_still_uncooked && ! impatient_prompt)
    return; /* @@@ is this necessary ?*/

	
  promptlen_on_screen =  colourless_strlen_unmarked(saved_rl_state.cooked_prompt ? saved_rl_state.cooked_prompt : saved_rl_state.raw_prompt, termwidth);
  curpos = (within_line_edit ? 1 : 0); /* if the user has pressed a key the cursor will be 1 past the current prompt */
  assert(termwidth > 0); 
  number_of_lines_in_prompt = 1 +  ((promptlen_on_screen + curpos -1) / termwidth); /* integer arithmetic! (e.g. 171/80 = 2) */
  cr(); 
  for (count = 0; count < number_of_lines_in_prompt -1; count++) {
    if (erase)
      clear_line();
    curs_up();
  } 
  clear_line();
  DPRINTF4(DEBUG_READLINE,"moved cursor up %d lines (erase = %d, len=%d, cooked=%d)", number_of_lines_in_prompt - 1, erase, promptlen_on_screen, cooked); 
}       


int
prompt_is_single_line(void)
{
  int homegrown_redisplay= FALSE;
  int force_homegrown_redisplay = FALSE;
  int retval;  
  
#ifndef SPY_ON_READLINE
#  define _rl_horizontal_scroll_mode FALSE
#  define rl_variable_value(s) "off"
#endif
  
#ifdef HOMEGROWN_REDISPLAY
  homegrown_redisplay=TRUE;
#endif

#ifdef DEBUG
  force_homegrown_redisplay =  debug & FORCE_HOMEGROWN_REDISPLAY;
#endif

  retval = _rl_horizontal_scroll_mode ||
    strncmp(rl_variable_value("horizontal-scroll-mode"),"on",3) == 0 ||
    homegrown_redisplay ||
    force_homegrown_redisplay;
 
  DPRINTF1(DEBUG_READLINE, "prompt is %s-line", (retval ? "single" : "multi"));
  return retval;
}


char *process_new_output(const char* buffer, struct rl_state* UNUSED(state)) {
  char*last_nl, *old_prompt_plus_new_output, *new_prompt, *result;
  
  old_prompt_plus_new_output = append_and_free_old(saved_rl_state.raw_prompt, buffer); 
  last_nl = strrchr(old_prompt_plus_new_output, '\n');
  if (last_nl != NULL) {        /* newline seen, will get new prompt: */
    new_prompt = mysavestring(last_nl +1); /* chop off the part after the last newline -  this will be the new prompt */
    *last_nl = '\0';
    old_prompt_plus_new_output = append_and_free_old (old_prompt_plus_new_output, "\n");
    result = (impatient_prompt ? mysavestring (old_prompt_plus_new_output): pass_through_filter(TAG_OUTPUT, old_prompt_plus_new_output));
    if (remember_for_completion) {
      feed_line_into_completion_list(result); /* feed output into completion list *after* filtering */
    }
  } else {      
    new_prompt = mysavestring(old_prompt_plus_new_output);
    result = mysavestring("");
  }                 
  free(old_prompt_plus_new_output);

  saved_rl_state.raw_prompt = new_prompt;
  if (saved_rl_state.cooked_prompt) {
    free (saved_rl_state.cooked_prompt);
    saved_rl_state.cooked_prompt = NULL; 
  }     
  return result;
}



int cook_prompt_if_necessary (void) {
  char *pre_cooked, *slightly_cooked, *rubbish_from_alternate_screen,  *filtered, *uncoloured, *cooked, *p, *non_rubbish = NULL;
  static char **term_ctrl_seqs[] 
    = {&term_rmcup, &term_rmkx, NULL}; /* (NULL-terminated) list of (pointers to) term control sequences that may be
                                       used by clients to return from an 'alternate screen'. If we spot one of those,
                                       assume that it, and anything before it, is rubbish and better left untouched */

  char ***tcptr;
  filtered = NULL;

  DPRINTF2(DEBUG_READLINE, "Prompt <%s>: %s", saved_rl_state.raw_prompt, prompt_is_still_uncooked ? "still raw" : "cooked already");

  if (saved_rl_state.cooked_prompt)    /* if (!prompt_is_still_uncooked) bombs with multi-line paste. Apparently
                                        prompt_is_still_uncooked can be FALSE while saved_rl_state.cooked_prompt = NULL. Ouch!@@@! */
    return FALSE;  /* cooked already */
  
  pre_cooked = mysavestring(saved_rl_state.raw_prompt);

  
  for (tcptr = term_ctrl_seqs; *tcptr; tcptr++) { 
    /* find last occurence of one of term_ctrl_seq */
    if (**tcptr && (p = mystrstr(pre_cooked, **tcptr))) {
      p += strlen(**tcptr);  /* p now points 1 char past term control sequence */ 
      if (p > non_rubbish) 
        non_rubbish = p; 
    }   
  }     
  /* non_rubbish now points 1 past the last 'alternate screen terminating' control char in prompt */
  if (non_rubbish) { 
    rubbish_from_alternate_screen = pre_cooked;
    pre_cooked = mysavestring(non_rubbish);
    *non_rubbish = '\0'; /* 0-terminate rubbish_from_alternate_screen */ 
  } else { 
    rubbish_from_alternate_screen = mysavestring("");
  }
  
  slightly_cooked = ansi_colour_aware ? protect_or_cleanup(pre_cooked, FALSE) : mysavestring(pre_cooked);
  free(pre_cooked);
  
  unbackspace(slightly_cooked); /* programs that display a running counter would otherwise make rlwrap keep prompts
                              like " 1%\r 2%\r 3%\ ......" */

  if ( /* raw prompt doesn't match '--only-cook' regexp */
      (prompt_regexp && ! match_regexp(slightly_cooked, prompt_regexp, FALSE)) ||
       /* now filter it, but filter may "refuse" the prompt */
      (strcmp((filtered =  pass_through_filter(TAG_PROMPT, slightly_cooked)), "_THIS_CANNOT_BE_A_PROMPT_")== 0)) { 
    /* don't cook, eat raw (and eat nothing if patient) */       
    saved_rl_state.cooked_prompt =  (impatient_prompt ? mysavestring(slightly_cooked) : mysavestring("")); 
    /* NB: if impatient, the rubbish_from_alternate_screen has been output already, no need to send it again */  
    free(slightly_cooked);
    free(filtered); /* free(NULL) is never an error */
    return FALSE;
  }   
  free(slightly_cooked);
  if(substitute_prompt) {
    uncoloured = mysavestring(substitute_prompt); 
    free(filtered);
  } else {
    uncoloured = filtered;
  }     
  if (colour_the_prompt) { 
    cooked =  colourise(uncoloured);
    free(uncoloured);
  } else {
    cooked = uncoloured;
  }
  if (! impatient_prompt)  /* in this case our rubbish hasn't been output yet. Output it now, but don't store
                              it in the prompt, as this may be re-printed e.g. after resuming a suspended rlwrap */                            
    write_patiently(STDOUT_FILENO,rubbish_from_alternate_screen, strlen(rubbish_from_alternate_screen), "to stdout");

  free(rubbish_from_alternate_screen);
  saved_rl_state.cooked_prompt = cooked;
  return TRUE;
}       




/*
   From the readline documentation: Acceptable keymap names are emacs, emacs-standard, emacs-meta, emacs-ctlx, vi,
   vi-move, vi-command, and vi-insert. vi is equivalent to vi-command; emacs is equivalent to emacs-standard. The
   default value is emacs. The value of the editing-mode variable also affects the default keymap.
*/


static Keymap getmap(const char *name) {
  Keymap km = rl_get_keymap_by_name(name);
  if (!km) 
    myerror(FATAL|NOERRNO, "Could not get keymap '%s'", name);
  return km;
}

static void bindkey(int key, rl_command_func_t *function, const char *function_name, const char *maplist) {
  char *mapnames[] = {"emacs-standard","emacs-ctlx","emacs-meta","vi-insert","vi-move","vi-command",NULL};
  char **mapname;
  for (mapname = mapnames; *mapname; mapname++) 
    if(strstr(maplist, *mapname)) {
      Keymap kmap = getmap(*mapname);
      DPRINTF4(DEBUG_READLINE,"Binding key %d (%s) in keymap '%s' to %s()", key, mangle_char_for_debug_log(key,TRUE), *mapname, function_name);
      if (rl_bind_key_in_map(key, function, kmap))
        myerror(FATAL|NOERRNO, "Could not bind key %d (%s) in keymap '%s'", key, mangle_char_for_debug_log(key,TRUE), *mapname);
    }
}       


#ifdef UNIT_TEST

void my_line_handler(char *line) { /* This function (callback) gets called by readline
                                   whenever rl_callback_read_char sees an ENTER */ 
  printf("\nYou typed: '%s'\n", line);
  if (!line)
    exit(0);
}
      

TESTFUNC(simple_callback, argc, argv, stage) {
  char byte_read;
  ONLY_AT_STAGE(TEST_AFTER_OPTION_PARSING);
  init_readline("Weh! ");
  set_echo(FALSE);
  rl_callback_handler_install("Type anything: ", &my_line_handler);
  while (TRUE) {
    if (!read(STDIN_FILENO, &byte_read, 1))
      exit(0);
    DPRINTF1(DEBUG_READLINE, "Byte read: %s", mangle_char_for_debug_log(byte_read, TRUE));
    rl_stuff_char(byte_read);
    rl_callback_read_char();
  }
 }    

TESTFUNC(standard_readline, argc, argv, stage) {
  ONLY_AT_STAGE(TEST_AFTER_OPTION_PARSING);
  while(TRUE) {
     char *line_read = readline ("go ahead: ");
     printf("\nYou typed: '%s'\n", line_read);
     if (!line_read || !*line_read)
       break;
     free(line_read);
  }
  exit(0);
}       

#endif /* UNIT_TEST */
