/* rlwrap - a readline wrapper
   (C) 2000-2007 Hans Lub

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,


   **************************************************************

   string_utils.c: rlwrap uses a fair number of custom string-handling
   functions. A few of those are replacements for (or default to)
   GNU or POSIX standard funcions (with names like mydirname,
   mystrldup). Others are special purpose string manglers for
   debugging, removing colour codes and the construction of history
   entries)

   All of these functions work on basic types as char * and int, and
   none of them refer to any of rlwraps global variables (except debug)
*/ 

     
#include "rlwrap.h"




/* mystrlcpy and mystrlcat: wrappers around strlcat and strlcpy, if
   available, otherwise emulations of them. Both versions *assure*
   0-termination, but don't check for truncation: return type is
   void */

void
mystrlcpy(char *dst, const char *src, size_t size)
{
#ifdef HAVE_STRLCPY
  strlcpy(dst, src, size);
#else
  strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
#endif
}

void
mystrlcat(char *dst, const char *src, size_t size)
{
#ifdef HAVE_STRLCAT
  strlcat(dst, src, size);
  dst[size - 1] = '\0';         /* we don't check for truncation, just assure '\0'-termination. */
#else
  strncat(dst, src, size - strnlen(dst, size) - 1);
  dst[size - 1] = '\0';
#endif
}



/* mystrndup: strndup replacement that uses the safer mymalloc instead
   of malloc*/

static char *
mystrndup(const char *string, int len)
{
  /* allocate copy of string on the heap */
  char *buf;
  assert(string != NULL);
  buf = (char *)mymalloc(len + 1);
  mystrlcpy(buf, string, len + 1);
  return buf;
}


/* compare strings for equality. fail if either of them is NULL */
bool
strings_are_equal(const char *s1, const char *s2) {
  return s1 && s2 && strcmp(s1,s2)== 0;
}

/* mysavestring: allocate a copy of a string on the heap */

char *
mysavestring(const char *string)
{
  assert(string != NULL);
  return mystrndup(string, strlen(string));
}


/* add3strings: allocate a sufficently long buffer on the heap and
   successively copy the three arguments into it */

char *
add3strings(const char *str1, const char *str2, const char *str3)
{
  int size;
  char *buf;
  
  assert(str1!= NULL); assert(str2!= NULL); assert(str3!= NULL);
  size = strlen(str1) + strlen(str2) + strlen(str3) + 1;        /* total length plus 0 byte */
  buf = (char *) mymalloc(size);

  /* DPRINTF3(DEBUG_TERMIO,"size1: %d, size2: %d, size3: %d",   (int) strlen(str1), (int) strlen(str2),  (int) strlen(str3)); */

  mystrlcpy(buf, str1, size);
  mystrlcat(buf, str2, size);
  mystrlcat(buf, str3, size);
  return buf;
}


/* append_and_free_old(str1, str2): return add2strings(str1, str2), freeing str1 
   append_and_free_old(NULL, str) just returns a copy of  str
*/ 

char *
append_and_free_old(char *str1, const char *str2)
{
  if (!str1)
    return mysavestring(str2); /* if str1 == NULL there is no need to "free the old str1" */
  else {
    char *result = add2strings(str1,str2);
    free (str1);
    return result;
  }     
}


/* mybasename and mydirname: wrappers around basename and dirname, if
   available, otherwise emulations of them */ 


char *
mybasename(const char *filename)
{                               /* determine basename of "filename" */
#if defined(HAVE_BASENAME) && defined(_GNU_SOURCE) /* we want only the GNU version  - but this doesn't guarantee that */
  char *filename_copy = mysavestring(filename); 
  char *result = mysavestring(basename(filename_copy));
  free(filename_copy);
  return result; /* basename on HP-UX is toxic: the result will be overwritten by subsequent invocations! */
#else
  char *p;

  /* find last '/' in name (if any) */
  for (p = filename + strlen(filename) - 1; p > filename; p--)
    if (*(p - 1) == '/')
      break;
  return p;
#endif
}

char *
mydirname(const char *filename)
{                               /* determine directory component of "name" */
#ifdef HAVE_DIRNAME
  char *filename_copy = mysavestring(filename);
  char *result = dirname(filename_copy);
  return result;
#else
  char *p;

  /* find last '/' in name (if any) */
  for (p = filename + strlen(filename) - 1; p > filename; p--)
    if (*(p - 1) == '/')
      break;
  return (p == filename ? "." : mystrndup(filename, p - filename));
#endif
}

 

/* Better atoi() with error checking */
int
my_atoi(const char *nptr)
{
  int result;
  char *endptr;
  
  errno = 0;
  result = (int) strtol(nptr, &endptr, 10);
  if (errno || endptr == nptr || *endptr)
    myerror(FATAL|USE_ERRNO, "Could not make sense of <%s> as an integer", mangle_string_for_debug_log(nptr, 20));
  return result;
}       

/* TODO: clean up the following mess. strtok() is cute, but madness. Write one function
   char *tokenize(const char *string, const char *delimiters, bool allow_empty_strings), and make 
   both split_with functions a special case of it. Drop mystrtok, count_str_occurrences and count_char_occurrences */ 


  /* mystrtok: saner version of strtok that doesn't overwrite its first argument */
  /* Scary strtok: "The  strtok()  function breaks a string into a sequence of zero or more nonempty tokens.  
    On the first call to strtok(), the string to be parsed should be specified in str.  
    In each subsequent call that should parse the same string, str must be NULL.
  */


char *
mystrtok(const char *s, const char *delimiters) {
  static char *scratchpad = NULL;
  if (s) { /* first call */
    if (scratchpad) 
      free(scratchpad); /* old news */
    scratchpad = mysavestring(s);
  }
  return strtok(s ? scratchpad : NULL, delimiters);
}       


static int
count_str_occurrences(const char *haystack, const char* needle)
{
  int count = 0, needle_length = strlen(needle);
  const char *p = haystack;
  
  assert(needle_length > 0);
  while ((p = strstr(p, needle))) {
      count++;
      p += needle_length;
  }
  return count;
}

static int
count_char_occurrences(const char *haystack, char c)
{
  int count;
  char *needle = mysavestring(" ");
  *needle = c;
  count = count_str_occurrences(haystack, needle);
  free(needle);
  return count;
}


void test_haystack(const char *haystack, const char* needle) {
  printf("<%s> contains <%s> %d times\n", haystack, needle, count_str_occurrences(haystack, needle));
}       



/* split_with("a bee    cee"," ") returns a pointer to an array {"a", "bee",  "cee", NULL} on the heap */

char **
split_with(const char *string, const char *delimiters) {
  const char *s;
  char *token, **pword;
  char **list = mymalloc((1 + strlen(string)) * sizeof(char **)); /* worst case: only delimiters  */ 
  for (s = string, pword = list; (token = mystrtok(s, delimiters)); s = NULL) 
    *pword++ = mysavestring(token);
  *pword = NULL;
  return list;
}       

/* unsplit_with(3, ["a", "bee", "cee"], "; ") returns a pointer to "a; bee; cee" on the heap */
/* takes n elements from strings (or all of them if n < 0) */
char *
unsplit_with(int n, char **strings, const char *delim) {
  int i;
  char *result = mysavestring(n != 0 && strings[0] ? strings[0]: "");
  for (i = 1; (n<0 && strings[i]) || i < n; i++) { 
       result = append_and_free_old(result, delim);
       result = append_and_free_old(result, strings[i]);
  }
  return result;
}

/* split_with("a\t\tbla", '\t') returns {"a" "bla", NULL}, but we want {"a", "", "bla", NULL} for filter completion.
   We write a special version (can be freed with free_splitlist), that optionally checks the number of components (if expected_count > 0) */
char **split_on_single_char(const char *string, char c, int expected_count) {
  /* the 1st +1 for the last element ("bla"), the 2nd +1 for the marker element (NULL) */
  char **list = mymalloc((count_char_occurrences(string,c) + 1 + 1) * sizeof(char **));
  char *stringcopy = mysavestring(string);
  char *p, **pword, *current_word;
  
  for (pword = list, p = current_word = stringcopy;
       *p; p++) {
    if (*p == c) {
      *p = '\0';
      *pword++ = mysavestring(current_word);
      current_word = p+1;
    }
  }
  *pword++ = mysavestring(current_word);
  if (expected_count  && pword-list != expected_count) 
    myerror(FATAL|NOERRNO, "splitting <%s> on single %s yields %d components, expected %d",
            mangle_string_for_debug_log(string, 50), mangle_char_for_debug_log(c, 1), pword -  list, expected_count); 
  *pword = NULL;
  free(stringcopy);
  return list;
}       

/* free_splitlist(list) frees lists components and then list itself. list must be NULL-terminated */
void free_splitlist (char **list) {
  char **p = list;
  while(*p)
    free(*p++);
  free (list);
}       


/* search_and_replace() is a utilty for handling multi-line input
   (-m option), keeping track of the cursor position on rlwraps prompt
   in order to put the cursor on the very same spot in the external
   editor For example, when using NL as a newline substitute (rlwrap
   -m NL <command>):
   
   search_and_replace("NL", "\n", "To be NL ... or not to be", 11,
   &line, &col) will return "To be \n ... or not to be", put 2 in line
   and 3 in col because a cursor position of 11 in "To be NL ..."
   corresponds to the 3rd column on the 2nd line in "To be \n ..."

   cursorpos, col and line only make sense if repl == "\n", otherwise
   they may be 0/NULL (search_and_replace works for any repl). The
   first position on the string corresponds to cursorpos = col = 0 and
   line = 1.
*/
   

char *
search_and_replace(char *patt, const char *repl, const char *string, int cursorpos,
                   int *line, int *col)
{
  int i, j, k;
  int pattlen = strlen(patt);
  int replen = strlen(repl);
  int stringlen = strlen(string);
  int cursor_found = FALSE;
  int current_line = 1;
  int current_column = 0;
  size_t scratchsize;
  char *scratchpad, *result;

  assert(patt && repl && string);
  DPRINTF2(DEBUG_READLINE, "string=%s, cursorpos=%d",
           M(string), cursorpos);
  scratchsize = max(stringlen, (stringlen * replen) / pattlen) + 1;     /* worst case : repleng > pattlen and string consists of only <patt> */
  DPRINTF1(DEBUG_READLINE, "Allocating %d bytes for scratchpad", (int) scratchsize);
  scratchpad = mymalloc(scratchsize);


  for (i = j = 0; i < stringlen;  ) {
    if (line && col &&                           /* if col and line are BOTH non-NULL, and .. */
        i >= cursorpos && !cursor_found) {       /*  ... for the first time, i >= cursorpos: */
      cursor_found = TRUE;                       /* flag that we're done here */                              
      *line = current_line;                      /* update *line */ 
      *col = current_column;                     /* update *column */
    }
    if (strncmp(patt, string + i, pattlen) == 0) { /* found match */
      i += pattlen;                                /* update i ("jump over" patt (and, maybe, cursorpos)) */  
      for (k = 0; k < replen; k++)                 /* append repl to scratchpad */
        scratchpad[j++] = repl[k];
      current_line++;                              /* update line # (assuming that repl = "\n") */
      current_column = 0;                          /* reset column */
    } else {
      scratchpad[j++] = string[i++];               /* or else just copy things */
      current_column++;
    }
  }
  if (line && col)
    DPRINTF2(DEBUG_READLINE, "line=%d, col=%d", *line, *col);
  scratchpad[j] = '\0';
  result = mysavestring(scratchpad);
  free(scratchpad);
  return (result);
}


/* first_of(&string_array) returns the first non-NULL element of string_array  */
char *
first_of(char **strings)
{                               
  char **p;

  for (p = strings; *p == NULL; p++);
  return *p;
}


/* allocate string representation of an integer on the heap */
char *
as_string(int i)
{
#define MAXDIGITS 10 /* let's pray no-one edits multi-line input more than 10000000000 lines long :-) */
  char *newstring = mymalloc(MAXDIGITS+1); 

  snprintf1(newstring, MAXDIGITS, "%d", i);
  return (newstring);
}



char *
mangle_char_for_debug_log(char c, int quote_me)
{
  char *special = NULL;
  char scrap[10], code, *format;
  char *remainder = "\\]^_"; 
  
  switch (c) {
  case 0: special = "<NUL>"; break;
  case 8: special  = "<BS>";  break;
  case 9: special  = "<TAB>"; break;
  case 10: special = "<NL>";  break;
  case 13: special = "<CR>";  break;
  case 27: special = "<ESC>"; break;
  case 127: special = "<DEL>"; break;
  }
  if (!special) {
    if (c > 0 && c < 27  ) {
      format = "<CTRL-%c>"; code =  c + 64;
    } else if (c > 27 && c < 32) {
      format = "<CTRL-%c>"; code =  remainder[c-28];
    } else {
      format = (quote_me ? "\"%c\"" : "%c"); code = c;
    }   
    snprintf1(scrap, sizeof(scrap), format, code);
  }
  return mysavestring (special ? special : scrap);
}       


/* mangle_string_for_debug_log(string, len) returns a printable
   representation of string for the debug log. It will truncate a
   resulting string longer than len, appending three dots ...  */

char *
mangle_string_for_debug_log(const char *string, int maxlen)
{
  int total_length;
  char *mangled_char, *result;
  const char *p; /* good old K&R-style *p. I have become a fossil... */
  MBSTATE st;
  
  if (!string)
    return mysavestring("(null)");
  result = mysavestring("");
  for(mbc_initstate(&st), p = string, total_length = 0; *p;  mbc_inc(&p, &st)) {
    if (is_multibyte(p, &st)) {
        mangled_char = mbc_first(p, &st);
        total_length += 1;
      } else {
        mangled_char = mangle_char_for_debug_log(*p, FALSE);
        total_length +=  strlen(mangled_char); /* can be more than 1 ("CTRL-A") */
      } 
    if (maxlen && (total_length > maxlen)) {
      result = append_and_free_old(result, "...");      
      break;  /* break *before* we append the latest char and exceed maxlen */
    }
    result = append_and_free_old(result, mangled_char);
    free(mangled_char);
  }
  return result;
}




char *mangle_buffer_for_debug_log(const char *buffer, int length) {
  char *string = mymalloc(length+1);
  int debug_saved = debug; /* needed by macro MANGLE_LENGTH */
  memcpy(string, buffer, length);
  string[length] = '\0';
  return mangle_string_for_debug_log(string, MANGLE_LENGTH);
}

/* mem2str(mem, size) returns a fresh string representation of mem where al 0 bytes have been replaced by "\\0" */
char *mem2str(const char *mem, int size) {
  const char *p_mem;
  char  *p_str;
  char *str = mymalloc(2*size + 1); /* worst case: "\0\0\0\0.." */
  for(p_mem = mem, p_str = str; p_mem < mem + size; p_mem++) { 
    if (*p_mem) 
      *p_str++ = *p_mem;
    else {
      *p_str++ = '\\';
      *p_str++ = '0';
    }
  }
  *p_str = '\0';
  return str;
}


char *
mystrstr(const char *haystack, const char *needle)
{
  return strstr(haystack, needle);
}



int scan_metacharacters(const char* string, const char *metacharacters) {
  const char *c;
  for (c = metacharacters; *c; c++)
    if (strchr(string, *c))
      return TRUE;
  return FALSE;
}       
               
/* allocate and init array of 4 strings (helper for munge_line_in_editor() and filter) */
char **
list4 (char *el0, char *el1, char *el2, char *el3)
{
  char **list = mymalloc(4*sizeof(char*));
  list[0] = el0;
  list[1] = el1;
  list[2] = el2;
  list[3] = el3;
  DPRINTF4(DEBUG_AD_HOC, "el0: <%s> el1: <%s> el2: <%s> el3: <%s>", el0, el1, el2, el3);
  return list;
}


/* remove_padding_and_terminate(buf, N) overwrites buf with a copy of
   its first N bytes, omitting any zero bytes, and then terminates the
   result with a final zero byte.  Example: if buf="a\0b\0\0cde@#!" then,
   after calling remove_padding_and_terminate(buf, 8) buf will contain
   "abcde\0de@#!"

   We need to call this function on everything we get from the
   inferior command because (out of sheer programmer laziness) rlwrap
   uses C strings internally. Zero bytes are only ever used as padding
   (@@@ I have never seen this happen, by the way), and padding is not
   used anymore on modern terminals. (except maybe for things like the
   visual bell) */
 

void remove_padding_and_terminate(char *buf, int length) {
  char *readptr, *copyptr;

  for (readptr = copyptr = buf; readptr < buf + length; readptr++) {
    if (*readptr != '\0')
      *copyptr++ = *readptr;
  }
  *copyptr = '\0';
  if (debug && strlen(buf) != (unsigned int) length)
    DPRINTF2(DEBUG_TERMIO, "removed %d zero bytes (padding?) from %s", length - (int) strlen(buf), M(buf));
}       
        
#define ESCAPE  '\033'
#define BACKSPACE '\010'
#define CARRIAGE_RETURN  '\015'

/* unbackspace(&buf) will overwrite buf (up to and including the first
   '\0') with a copy of itself. Backspaces will move the "copy
   pointer" one backwards, carriage returns will re-set it to the
   begining of buf.  Because the re-written string is always shorter
   than the original, we need not worry about writing outside buf

   Example: if buf="abc\bd\r\e" then, after calling unbackspace(buf),
   buf will contain "ebd" @@@ Should be just "e"

   We need this function because many commands emit "status lines"
   using backspaces and carriage returns to re-write parts of the line in-place.
   Rlwrap will consider such lines as "prompts" (@@@myabe it shouldn't?)
   but mayhem results if we feed the \b and \r characters to readline
*/

void
unbackspace_old(char* buf) {
  char *readptr, *copyptr, *endptr;
  int seen_bs_or_cr;

  DPRINTF1(DEBUG_TERMIO,"unbackspace: %s", M(buf));
  seen_bs_or_cr = FALSE;
  
  for (readptr = copyptr = endptr = buf; *readptr; readptr++) {

    assert(endptr <= readptr);
    assert(copyptr <= endptr);

    switch (*readptr) {
    case BACKSPACE:
      copyptr = (copyptr > buf ? copyptr - 1 : buf);  /* cannot backspace past beginning of buf */
      seen_bs_or_cr = TRUE;
      break;
    case CARRIAGE_RETURN:
      copyptr = buf;  
      seen_bs_or_cr = TRUE;
      break;
    default:
      *copyptr++ = *readptr;      
      break;
    }
    if (copyptr > endptr)
      endptr = copyptr;
  }
  *endptr = '\0';
  if (seen_bs_or_cr) 
      DPRINTF1(DEBUG_TERMIO,"unbackspace result: %s", M(buf));  
}




void
unbackspace(char* buf) {
  char *readptr, *copyptr;
  int seen_bs_or_cr;

  DPRINTF1(DEBUG_TERMIO,"unbackspace: %s", M(buf));
  seen_bs_or_cr = FALSE;
  
  for (readptr = copyptr = buf; *readptr; readptr++) {
    switch (*readptr) {
    case BACKSPACE:
      copyptr = (copyptr > buf ? copyptr - 1 : buf);  /* cannot backspace past beginning of buf          */
      while (*copyptr == RL_PROMPT_END_IGNORE && copyptr > buf) {    /* skip control codes  ...          */
        while (*copyptr != RL_PROMPT_START_IGNORE && copyptr > buf)  /* but never past buffer start      */
          copyptr--;                                                 /* e.g. with pathological "x\002\b" */
        if (copyptr > buf)
          copyptr--;
      }   
      seen_bs_or_cr = TRUE;
      break;
    case CARRIAGE_RETURN:
      copyptr = buf;  
      seen_bs_or_cr = TRUE;
      break;
    default:
      *copyptr++ = *readptr;      
      break;
    }
  }
  *copyptr = '\0';
  if (seen_bs_or_cr) 
      DPRINTF1(DEBUG_TERMIO,"unbackspace result: %s", M(buf));
  
}


#ifdef UNIT_TEST

static
void test_unbackspace (const char *input, const char *expected_result) {
  char *scrap = mysavestring(input);
  unbackspace(scrap);
  if (strcmp(scrap, expected_result) != 0)
    myerror(FATAL|NOERRNO, "unbackspace '%s' yielded '%s', expected '%s'",
              mangle_string_for_debug_log(input,0),
              mangle_string_for_debug_log(scrap,0),
              expected_result);
}

/*  run with: make clean ; make CFLAGS='-g -DUNIT_TEST=test'; ./rlwrap <argv> */
TESTFUNC(test, argc, argv, stage) {
  ONLY_AT_STAGE(TEST_AT_PROGRAM_START );
  test_unbackspace("zx\001a\002\bq","sssssssss");
  exit(0);
}

#endif



/* Readline allows to single out character sequences that take up no
   physical screen space when displayed by bracketing them with the
   special markers `RL_PROMPT_START_IGNORE' and `RL_PROMPT_END_IGNORE'
   (declared in `readline.h').

   mark_invisible(buf) returns a new copy of buf with sequences of the
   form ESC[;0-9]*m? marked in this way. 
   
*/

/*
  (Re-)definitions for testing   
  #undef RL_PROMPT_START_IGNORE
  #undef  RL_PROMPT_END_IGNORE
  #undef isprint
  #define isprint(c) (c != 'x')
  #define RL_PROMPT_START_IGNORE '('
  #define RL_PROMPT_END_IGNORE ')'
  #define ESCAPE 'E'
*/


/* TODO @@@ replace the following obscure and unsafe functions using the regex library */
static void  copy_ordinary_char_or_ESC_sequence(const char **original, char **copy);
static void match_and_copy(const char *charlist, const char **original, char **copy);
static int matches (const char *charlist, char c) ;
static void copy_next(int n, const char **original, char **copy);

char *
mark_invisible(const char *buf)
{
  int padsize =  (assert(buf != NULL), (3 * strlen(buf) + 1)); /* worst case: every char in buf gets surrounded by RL_PROMPT_{START,END}_IGNORE */
  char *scratchpad = mymalloc (padsize);
  char *result = scratchpad;
  const char **original = &buf;
  char **copy = &scratchpad;
  DPRINTF1(DEBUG_AD_HOC, "mark_invisible(%s) ...", M(buf));
  
  if (strchr(buf, RL_PROMPT_START_IGNORE))
    return mysavestring(buf); /* "invisible" parts already marked */
    
  while (**original) {
    copy_ordinary_char_or_ESC_sequence(original, copy); 
    assert(*copy - scratchpad < padsize);
  }
  **copy = '\0';
  DPRINTF1(DEBUG_AD_HOC, "mark_invisible(...) = <%s>", M(result));
  return(result);       
}       




static void
copy_ordinary_char_or_ESC_sequence (const char **original, char **copy)
{
  if (**original != ESCAPE || ! matches ("[]", *(*original + 1))) {
    copy_next(1, original, copy);
    return;       /* not an ESC[ sequence */
  }
  *(*copy)++ = RL_PROMPT_START_IGNORE;
  copy_next(2, original, copy);
  match_and_copy(";0123456789", original, copy);
  match_and_copy("m", original, copy);
  *(*copy)++ = RL_PROMPT_END_IGNORE;
}       
    
static void
match_and_copy(const char *charlist, const char **original, char **copy)
{
  while (matches(charlist, **original))
    *(*copy)++ = *(*original)++;
}       

static int
matches (const char *charlist, char c)
{
  const char *p;
  for (p = charlist; *p; p++)
    if (*p == c)
      return TRUE;
  return FALSE;
}       
  

static void
copy_next(int n, const char **original, char **copy)
{
  int i;
  for (i = 0; **original && (i < n); i++)
    *(*copy)++ = *(*original)++;
}




/* helper function: returns the number of displayed characters (the "colourless length") of str (which has to have its
   unprintable sequences marked with RL_PROMPT_*_IGNORE).  Puts a copy without the RL_PROMPT_*_IGNORE characters in
   *copy_without_ignore_markers (if != NULL)
*/

int
colourless_strlen(const char *str, char ** pcopy_without_ignore_markers, int UNUSED(termwidth))
{
  int visible  = TRUE;
  int length   = strlen(str);
  int i, colourless_length, colourless_bytes;
  const char *str_ptr, *p;
  char *copy_ptr, *copy_without_ignore_markers;
  MBSTATE st;

  typedef struct {
    char *bytes;
    MBSTATE state;
  } mbchar_cell;
  
  mbchar_cell *cellptr, *copied_cells = mymalloc((length + 1)  * sizeof(mbchar_cell));
    
  /* The next loop scans str, one multi-byte character at a time, constructing a colourless copy  */
  /* cellptr always points at the next available free cell                                        */
  for(mbc_initstate(&st), str_ptr = str, colourless_bytes = 0, cellptr = copied_cells;
      *str_ptr;
      mbc_inc(&str_ptr, &st)) {
    assert (cellptr < copied_cells + length);
    switch (*str_ptr) {
    case RL_PROMPT_START_IGNORE:
      visible = FALSE;
      continue;
    case RL_PROMPT_END_IGNORE:
      visible = TRUE;
      continue;
    case '\r':
      if (visible) {                  /* only ever interpret CR (and Backspace) when visible (i.e. outside control sequences) */
        for ( ; cellptr > copied_cells; cellptr--)  
          free((cellptr-1)->bytes);   /* free all cells            */
        mbc_initstate(&st);           /* restart with virgin state */
        colourless_bytes = 0;
      }
      continue;
    case '\b':
      if ((visible && cellptr > copied_cells)) {     /* except when invisible, or at beginning of copy ... */
        cellptr -= 1;                                /* ... reset cellptr to previous (multibyte) char     */
        colourless_bytes -= strlen(cellptr->bytes);  
        free(cellptr->bytes);
        if (cellptr > copied_cells)
          st = (cellptr -1) -> state;                /* restore corresponding shift state                  */
        else
          mbc_initstate(&st);                        /* or initial state, if at start of line              */
      }
      continue;
    }
    if (visible) {
      MBSTATE st_copy   = st;
      int nbytes        = mbc_charwidth(str_ptr, &st_copy);
      char *q           = cellptr -> bytes = mymalloc(1 + nbytes);

      colourless_bytes += nbytes;
      mbc_copy(str_ptr,&q, &st); /* copy the possibly multi-byte character at str_ptr to cellptr -> bytes , incrementing q to just past the copy */
      *q                = '\0';
      cellptr -> state  = st; /* remember shift state after reading str_ptr, just in case a backspace comes along later */
      cellptr           += 1;
    }
  } /* end of for loop */

  colourless_length = cellptr - copied_cells;
  
  copy_without_ignore_markers = mymalloc(colourless_bytes + 1);
  for (cellptr = copied_cells, copy_ptr = copy_without_ignore_markers, i = 0; i < colourless_length; i++, cellptr++) {
    for(p = cellptr->bytes; *p; p++, copy_ptr++) 
      *copy_ptr = *p;
    free(cellptr->bytes);
  }
  *copy_ptr = '\0';
  free(copied_cells);


  DPRINTF4(DEBUG_READLINE, "colourless_strlen(\"%s\", \"%s\") = %d  chars, %d  bytes",
           M(str), copy_without_ignore_markers, colourless_length, colourless_bytes);

  if (pcopy_without_ignore_markers)
    *pcopy_without_ignore_markers = copy_without_ignore_markers;
  else
    free(copy_without_ignore_markers);
  
  

  return colourless_length;
}

/* helper function: returns the number of displayed characters (the
   "colourless length") of str (which has its unprintable sequences
   marked with RL_PROMPT_*_IGNORE).

   Until rlwrap 0.44, this function didn't take wide characters into 
   consideration, causing problems with long prompts containing wide characters.
*/



int
colourless_strlen_unmarked (const char *str, int termwidth)
{
  char *marked_str = mark_invisible(str);
  int colourless_length = colourless_strlen(marked_str, NULL, termwidth);
  free(marked_str);
  return colourless_length;
}


/* skip a maximal number (possibly zero) of termwidth-wide
   initial segments of long_line and return the remainder
   (i.e. the last line of long_line on screen)
   if long_line contains an ESC character, return "" (signaling
   "don't touch") */   


char *
get_last_screenline(char *long_line, int termwidth)
{
  int line_length, removed;
  char *line_copy, *last_screenline;

  line_copy = mysavestring(long_line);
  line_length = strlen(line_copy);
  
  if (termwidth == 0 ||              /* this may be the case on some weird systems */
      line_length <=  termwidth)  {  /* line doesn't extend beyond right margin
                                        @@@ are there terminals that put the cursor on the
                                        next line if line_length == termwidth?? */
    return line_copy; 
  } else if (strchr(long_line, '\033')) { /* <ESC> found, give up */
    free (line_copy);
    return mysavestring("Ehhmm..? > ");
  } else {      
    removed = (line_length / termwidth) * termwidth;   /* integer arithmetic: 33/10 = 3 */
    last_screenline  = mysavestring(line_copy + removed);
    free(line_copy);
    return last_screenline;
  }
}

/* lowercase(str) returns lowercased copy of str */
char *
lowercase(const char *str) {
  char *result, *p;
  result = mysavestring(str);
  for (p=result; *p; p++)
    *p = tolower(*p);
  return result;
}       



char *
colour_name_to_ansi_code(const char *colour_name) {
  if (colour_name  && *colour_name && isalpha(*colour_name)) {
    char *lc_colour_name = mysavestring(lowercase(colour_name));
    char *bold_code = (isupper(*colour_name) ? "1" : "0");

#define isit(c) (strcmp(c,lc_colour_name)==0)
    char *colour_code =
      isit("black")   ? "30" :
      isit("red")     ? "31" :
      isit("green")   ? "32" :
      isit("yellow")  ? "33" :
      isit("blue")    ? "34" :
      isit("magenta") ? "35" :
      isit("purple")  ? "35" :
      isit("cyan")    ? "36" :
      isit("white")   ? "37" :
      NULL ;
      
#undef isit
    if (colour_code)
      return add3strings(bold_code,";",colour_code);
    else
      myerror(FATAL|NOERRNO, "unrecognised colour name '%s'. Use e.g. 'yellow' or 'Blue'.", colour_name);
  }
  return mysavestring(colour_name);
}       
    
    






/* returns TRUE if string is numeric (i.e. positive or negative integer), otherwise FALSE */
int isnumeric(char *string){
  char *pstr = string;
  if (*pstr == '-')  /* allow negative numbers */
    pstr++; 
  while (*pstr != '\0')
    if (!isdigit(*pstr++)) return FALSE;

  return TRUE;
}

#define DIGITS_NUMBER 8  /* number of (hex) digits of length of field. 6 digits -> max 16MB per message field, should suffice */
#define MAX_FIELD_LENGTH ((1UL <<  (DIGITS_NUMBER * 4 - 1)) -1) /* max integer that can be written with DIGITS_NUMBER (hex)digits */
#define MY_HEX_FORMAT(n) ("%0" MY_ITOA(n) "x")
#define MY_ITOA(n) #n 


/* fussy strtol with error checking */
long
mystrtol(const char *nptr, int base)
{
  char *endptr;
  long result;

  errno = 0;
  result = strtol(nptr, &endptr, base);
  if (*endptr != '\0')
    myerror(FATAL|NOERRNO, "invalid representation %s", nptr);
  if (errno != 0)
    myerror(FATAL|USE_ERRNO, "strtol error");

  return result;
}




/* Encode a length as a string on the heap */
static char *
encode_field_length(int length)
{
  char *encoded_length = mymalloc(DIGITS_NUMBER+1);
  sprintf(encoded_length, MY_HEX_FORMAT(DIGITS_NUMBER), length);
  return encoded_length;
}       


/* decode first length field in a message of form "<length 1> <message 1> <length 2> ...." 
   and advance pointer *ppmessage to the start of <message 1> */
static int
decode_field_length(char** ppmessage)
{
  char hex_string[DIGITS_NUMBER+1];
  long length;
  
  mystrlcpy(hex_string, *ppmessage, DIGITS_NUMBER+1);
  length = mystrtol(hex_string, 16);
  *ppmessage += DIGITS_NUMBER;
  return length;
}       
  

/* Test an invariant: */
void
test_field_length_encoding()
{
  int testval = 1423722;
  char *encoded = encode_field_length(testval);
  assert( decode_field_length(&encoded) == testval &&  (*encoded == '\0'));
}

/* Append <field_length> <field> to message and return the result (after freeing the original message)
<field_length> is a string representation of <field>s length
message can be empty, or, equivalently, NULL.
*/
char *
append_field_and_free_old(char *message, const char *field)
{
  long unsigned int length = strlen(field);
  char *encoded_length;
  if (length > MAX_FIELD_LENGTH)
    myerror(FATAL|NOERRNO, "message field\"%s...\" has length %ld, it should be less than %ld",
            mangle_string_for_debug_log(field, 10), length,  MAX_FIELD_LENGTH);
  encoded_length =  encode_field_length(length);
  message = append_and_free_old(message, encoded_length);
  message = append_and_free_old(message, field);
  free(encoded_length);
  return message;
}



char *
merge_fields(char *field, ...)
{  
  char *varg = field;
  char *message = NULL;
  va_list vargs;
  
  va_start(vargs, field);
  while (varg != END_FIELD) {
    message = append_field_and_free_old(message, varg);
    varg = va_arg(vargs, char*);
  }
  va_end(vargs);
  return message;
}



/*
split a message of a string:

    <field_length1> <field1> <field_length 2> <field2>...

into:

    [<field1>, <field2>, ...]
*/


char **
split_filter_message(char *message, int *counter)
{
  char *pmessage = message;
  int message_length = strlen(message);
  char **list, **plist;
  int nfields = 0;

  static int smallest_message_size = 0;
  
  if (smallest_message_size == 0)
    smallest_message_size = strlen(append_field_and_free_old(NULL, "")); /* this assumes that the empty message is the smallest possible */

  assert(smallest_message_size > 0);
  plist = list = mymalloc(sizeof(char*) * (1 + strlen(message)/smallest_message_size )); /* worst case: "0000000000000000000000" */
  nfields = 0;

  while(!(*pmessage == '\0')) {
    long length = decode_field_length(&pmessage);

    /* cut out a field from the head of the message: */
    char *field = mymalloc(sizeof(char) * (length+1));
    mystrlcpy(field, pmessage, length+1);
    *plist++ = field;
    pmessage += length;
    nfields++;
    if (pmessage > message + message_length)
      myerror(FATAL|NOERRNO, "malformed message; %s", mangle_string_for_debug_log(message, 256));
  }
  if (counter)
    *counter =  nfields;
  *plist = 0;
  return list;
}



#ifndef HAVE_REGEX_H
char *protect_or_cleanup(const char *prompt) {
  return mysavestring(prompt); /*essentially a NOP */
}       

#else

/* regcomp with error checking (and simpler signature) */
static regex_t *my_regcomp(const char*regex, int flags) {
  regex_t *compiled_regexp;
  int compile_error;
  if (!*regex)
    return NULL;
  compiled_regexp = mymalloc(sizeof(regex_t));
  compile_error = regcomp(compiled_regexp, regex, flags);     
  if (compile_error) {
    int size = regerror(compile_error, compiled_regexp, NULL, 0);
    char *error_message =  mymalloc(size);
    regerror(compile_error, compiled_regexp, error_message, size);
    myerror(FATAL|NOERRNO, "(Internal error:) in regexp \"%s\": %s", mangle_string_for_debug_log(regex,256), error_message);  
  }
  return compiled_regexp;
}       
 

#define TOKEN '@'
#define MAXGROUPS 2

/* protect_pattern("foo", 'a', 'b) = "(a[^b]*b)|(foo)" */
char *protect(const char *pattern, char protect_start, char protect_end) {
  char *alternative_pattern = mymalloc(strlen(pattern) + 14);
  sprintf(alternative_pattern,"(%c[^%c]*%c)|(%s)", protect_start, protect_end, protect_end, pattern); 
  return alternative_pattern;
}



/* Substitute all occurences of the second group in a <compiled_pattern> "(..)|(..)" within <source> with <replacement>
   (any TOKENs within the replacement will be replaced by the match) , skipping everything that matches the first group
   (which can be said to "protect" against replacement) E.g: 
        
       replace_special("a zzz b zzz", "(a[^a]*b)|(z+)", "(@)") = "a zzz b (zzz)" 

   The role of a and b above will be played by the  RL_PROMPT_{START,END}_IGNORE characters                           */

char *replace_special(const char *source, regex_t *compiled_pattern, const char*replacement) {
  const char *source_cursor;
  char *copy_with_replacements, *copy_cursor;
  int max_copylen;

  assert(source != NULL);
  max_copylen = 1 + max(1, strlen(replacement)) * strlen(source);

  if (!compiled_pattern) /* pattern == NULL: just return a copy */
    return mysavestring(source);
  copy_with_replacements = mymalloc(max_copylen); /* worst case: replace every char in source by replacement (+ 1 final zero byte)  */
  source_cursor = source;
  copy_cursor = copy_with_replacements;
    
  while(TRUE) {
    regmatch_t matches[MAXGROUPS + 1];          /* whole match + MAXGROUPS groups */
    if ((regexec(compiled_pattern, source_cursor, MAXGROUPS + 1, matches , 0) == REG_NOMATCH)) {   /* no (more) matches ...   */
      strcpy(copy_cursor,source_cursor);        /* .. copy remainder of source (may be empty), and we're done      */
      break;                                    /* 0-terminates copy, even if last match consumed source copletely */
    } else {
      int i; const char *p;
      int protected_start   = matches[1].rm_so;
      int protected_end     = matches[1].rm_eo;
      int protected_length  = protected_end - protected_start;
      int match_start       = matches[2].rm_so;  
      int match_end         = matches[2].rm_eo;
      int match_length      = match_end - match_start;   /* Either the first ("protected") group in alternative_pattern matches, */
      assert(!(protected_end > -1 && match_end > -1));   /*  ... or the second (never both - that is the assertion here)         */
      assert(protected_length > 0 || match_length > 0);  /*  ... and that match cannot be empty                                  */
      if (protected_end > -1)                   /* If it is the first ...                                                        */
        for (i = 0; i< protected_end; i++)      /* copy until protected match ends                                               */
          *copy_cursor++ =  *source_cursor++;
      else {                                    /* if it is the second (the original pattern) ...    */
        for (i = 0; i< match_start; i++)        /* ... copy until match starts                       */
          *copy_cursor++ =  *source_cursor++;
        for(p = replacement; *p; p++)           /* splice replacement                                */
          if (*p == TOKEN) {                    /* where there is a TOKEN (may be more than one) ... */
            assert(copy_cursor + match_length - copy_with_replacements < max_copylen);
            for(i=0; i< match_length ; i++)     /* ... splice matched group                          */
              *copy_cursor++ = *(source_cursor + i);
          } else                                  /* otherwise, just copy replacement                  */
            *copy_cursor++ = *p;
        source_cursor += match_length;          /* finished replacing this match, try again          */
      }
    }
    *copy_cursor = '\0';
  }
  return copy_with_replacements;
}
      

/* All the codes we want to preserve (i.e. keep and put between RL_PROMPT_{START,END}_IGNORE )    */
static
char *protected_codes[]  = { "\x1B\x1B", NULL};

/* As we don't protect (and hence will get rid of) ANSI colour codes when "bleaching" the prompt, */
/* specify separately:                                                                            */ 
static char *ansi_colour_code_regexp = "(\x1B\\[[0-9;]*m)";          /* colour codes              */



/* All the codes we want to get rid of. cf.                                                                        */
/* https://stackoverflow.com/questions/14693701/how-can-i-remove-the-ansi-escape-sequences-from-a-string-in-python */
/* We only cover 7-bit C1 ANSI sequences; the 8-bit are not much used as they interfere with UTF-8, and we         */
/* don't need to absolutely catch everything anyway                                                                */
static
char *unwanted_codes[] = { "(\x1B[ -/]*[0-Z\\\\-~])"           /* ANSI X3.41: ESC + I-pattern + F-pattern (except [, which is covered below) */
                           ,"(\x1B[@-Z\\]-_])"                 /* C1_Fe */
                           ,"(\x1B\\[[0-9:;<>=?]*[-/]*[@-~])"  /* CSI   (overlaps with "protected" codes!) (is the meaning of [@-~] locale-dependent?) */
                           ,"(\x1B\\[?1h\x1B=)"                /* smkx keypad            */
                           ,"(\x1B\\]0;[[:print:]]*\x07)"      /* tsl <window title> fsl */

                         ,NULL};




/* mark protected codes between RL_PROMPT_{START,END}_IGNORE and erase unwanted codes */
char *protect_or_cleanup(char *prompt, bool free_prompt) {
  char *result1, *result;
  static char *protected_codes_regexp ;
  static regex_t *compiled_and_protected_protected_codes_regexp;
  static char *protected_token;
  static char *unwanted_codes_regexp;
  static regex_t *compiled_and_protected_unwanted_codes_regexp;
  
  /* protect stuff we want to keep: */
  
  /* (once)construct the regexp for the protected codes: */
  if (!protected_codes_regexp) {
    protected_codes_regexp = unsplit_with(-1, &protected_codes[0], "|");
    if (!bleach_the_prompt)
      protected_codes_regexp = add3strings(protected_codes_regexp, "|", ansi_colour_code_regexp);
    compiled_and_protected_protected_codes_regexp = my_regcomp(protect(protected_codes_regexp, RL_PROMPT_START_IGNORE, RL_PROMPT_END_IGNORE), REG_EXTENDED);
  }
  /* (once) construct the replacement pattern */
  if (!protected_token) {
     protected_token = mymalloc(4); /* "\x01@\x02" */
     sprintf(protected_token,"%c%c%c",RL_PROMPT_START_IGNORE, TOKEN, RL_PROMPT_END_IGNORE);
  }
  result1 = replace_special(prompt, compiled_and_protected_protected_codes_regexp, protected_token);
  
  if (!unwanted_codes_regexp) {
    unwanted_codes_regexp = unsplit_with(-1, &unwanted_codes[0], "|");
    DPRINTF1(DEBUG_AD_HOC, "unwanted_codes_regexp: %s", M(unwanted_codes_regexp));
    compiled_and_protected_unwanted_codes_regexp =  my_regcomp(protect(unwanted_codes_regexp, RL_PROMPT_START_IGNORE, RL_PROMPT_END_IGNORE), REG_EXTENDED);
  }
  result = replace_special(result1, compiled_and_protected_unwanted_codes_regexp, "");
   DPRINTF2(DEBUG_READLINE, "protect_or_cleanup(%s) = %s", M(prompt), M(result)); 
  free(result1);
  
  if (free_prompt)
    free(prompt);

  return result;
}


/*
  returns TRUE if 'string' matches the 'regexp' (or is a superstring
  of it, when we don't HAVE_REGEX_H). The regexp is recompiled with
  every call, which doesn't really hurt as this function is not called
  often: at most twice for every prompt.  'string' and 'regexp' may be
  NULL (in which case FALSE is returned)

  Only used for the --forget-regexp and the --prompt-regexp options
*/
int match_regexp (const char *string, const char *regexp, int case_insensitive) {
  int result = FALSE;
  
  if (!regexp || !string)
    return FALSE;

#ifndef HAVE_REGEX_H
  {
    static int been_warned = 0;
    char *metachars = "*()+?";
    char *lc_string = (case_insensitive ? lowercase(string) : mysavestring(string));
    char *lc_regexp = (case_insensitive  ? lowercase(regexp) : mysavestring(regexp));

    if (scan_metacharacters(regexp, metachars) && !been_warned++) /* warn only once if the user specifies a metacharacter */
      myerror(WARNING|NOERRNO, "one of the regexp metacharacters \"%s\" occurs in regexp(?) \"%s\"\n"
             "  ...but on your platform, regexp matching is not supported!", metachars, regexp);        
    
    result = mystrstr(lc_string, lc_regexp);
    free(lc_string);
    free(lc_regexp);    
  }
#else
  {
    regex_t *compiled_regexp = my_regcomp(regexp, REG_EXTENDED|REG_NOSUB|(case_insensitive ? REG_ICASE : 0));
    result = !regexec(compiled_regexp, string, 0, NULL, 0);
    regfree(compiled_regexp);
  }
#endif

  
  return result;        
}


  
#ifdef UNIT_TEST  

  
TESTFUNC(test_subst, argc, argv, stage) {
  ONLY_AT_STAGE(TEST_AFTER_OPTION_PARSING);
  while(TRUE) {
     regmatch_t matches[MAXGROUPS + 1];          /* whole match + MAXGROUPS groups */
     char *line_read = readline ("go ahead (string pattern): ");
     if (strings_are_equal(line_read, "stop"))
       break;
     char**components = split_with(line_read," ");
     if (!(components[0] && components[1]))
       continue;
     regex_t *re = my_regcomp(protect(components[1],'a','b'), REG_EXTENDED);
     bool does_match = regexec(re, components[0], MAXGROUPS + 1, matches , 0) != REG_NOMATCH;
     printf("%s\n", does_match  ? "JA" : "NEE");
     free_foreign(line_read);
  }
  cleanup_rlwrap_and_exit(EXIT_SUCCESS);
}

#endif

#endif /* def HAVE_REGEX_H */


/* scan blocks of client output for "cupcodes" that enter and exit the "alternate screen" and set the global variable screen_is_alternate accordingly */
void check_cupcodes(const char *client_output) {
  static char *still_unchecked; /* to avoid missing a cupcode that spans more than 1 read buffer, we keep the last few 
                                   bytes in a static buffer (still_unchecked) , that we always prepend to the next incoming block */
  static int rmcup_len, smcup_len, max_cuplen;
  char *output_copy, *outputptr;
  int cut_here;
  
  if (!(commands_children_not_wrapped && term_smcup && term_rmcup))
    return; /* check is impossible or unnecessary */

  if (still_unchecked == NULL) { /* init static vars once */
    still_unchecked = mysavestring("");
    rmcup_len = strlen(term_rmcup);
    smcup_len = strlen(term_smcup);
    max_cuplen = max(rmcup_len, smcup_len);
  }     
  outputptr = output_copy = append_and_free_old(still_unchecked, client_output);

  /* keep the very end for next time, as it might contain a partial cupcode */
  /* in the worst case we will notice the shortest of the two cupcodes twice */
  cut_here  = max(0, strlen(output_copy) - max_cuplen);
  still_unchecked = mysavestring (&output_copy[cut_here]);


  while (TRUE) {
    char *rmptr = strstr(outputptr, term_rmcup);
    char *smptr = strstr(outputptr, term_smcup);
    if (rmptr == smptr) {
      assert(rmptr == NULL); /* can only fail  if term_smcup is prefix of term_rmcup, or vice versa, which never happens as far as I know */ 
      break;
    } else if (rmptr > smptr) { /* rmcup and smcup may occur in the same block of output */
      DPRINTF0(DEBUG_READLINE, "Saw rmcup");
      screen_is_alternate = FALSE;
      outputptr = rmptr + rmcup_len; /* 1 past match */
    } else  {
      DPRINTF0(DEBUG_READLINE, "Saw smcup");
      screen_is_alternate = TRUE;
      outputptr = smptr + smcup_len;
    }       
  }
  free(output_copy);
}
