#include "rlwrap.h"
#include <limits.h>


/* rlwrap was written without worrying about multibyte characters.
   the functions below "retrofit" multi-byte support by providing
   functions that can be used with minimal adaptations in the usual
   idioms like for (char *p = s; *p; p++) 

   c.f. https://kirste.userpage.fu-berlin.de/chemnet/use/info/libc/libc_18.html

*/


/* voor ascii chars is next_mbc(p) == p+1, maar voor een multibyte positie kan het ook bijv. p+3 of meer zijn */

#ifdef MULTIBYTE_AWARE

MBSTATE *
mbc_initstate(MBSTATE *st)
{
  memset (st, 0, sizeof(MBSTATE));
  return st;
}

MBSTATE *
mbc_copystate(MBSTATE st, MBSTATE *stc)
{
  *stc = st;
  return stc;
}  

int
mbc_is_valid(const char *mb_string, const MBSTATE *st)
{
  MBSTATE scrap = *st;
  return (int) mbrlen(mb_string, MB_LEN_MAX, &scrap ) >= 0;
}

const char *
mbc_next(const char *mb_string, MBSTATE *st)
{
  if (!*mb_string || !mbc_is_valid(mb_string, st))
    return mb_string + 1;     
  else
    return mb_string + mbrlen(mb_string, MB_LEN_MAX, st);
}  



const char *
mbc_inc(const char **mbc, MBSTATE *st)
{
  return (*mbc = mbc_next(*mbc, st));
}

char *
mbc_first(const char *mb_string, const MBSTATE *st)
{
  MBSTATE scrap = *st;
  char buffer[MB_LEN_MAX+1];
  int len = mbrlen(mb_string, MB_LEN_MAX, &scrap );
  strncpy(buffer, mb_string, len);
  buffer[len] = '\0';
  return mysavestring(buffer);
}       


int
mbc_charwidth(const char *p, MBSTATE *st)
{
  int width =  mbrlen(p, MB_LEN_MAX, st);
  if (width < 0) {
    DPRINTF1(DEBUG_READLINE, "invalid multi-byte charavter at stert of %s", mangle_string_for_debug_log(p, MANGLE_LENGTH)); 
    myerror(FATAL|USE_ERRNO, "invalid multi-byte character");
  }
  return width;
}

/* copy the first (wide) character in p to *q, incrementing q to one past the result */
void mbc_copy(const char *p, char **q, MBSTATE *st)
{
  int i;
  for(i=0; i < mbc_charwidth(p, st); i++)
    *(*q)++ = p[i];
}


int
is_multibyte(const char *mb_char, const MBSTATE *st)
{
  MBSTATE scrap = *st;
  return mbc_is_valid(mb_char, st) && mbrlen(mb_char, MB_LEN_MAX, &scrap) > 1;
}

size_t
mbc_strnlen(const char *mb_string, size_t maxlen, MBSTATE *st)
{
  size_t len;
  const char *p;
  for (len = 0, p = mb_string; *p && p < mb_string + maxlen; mbc_inc(&p, st))
    len++;
  return len;
}


#else /* if not MULTIBYTE_AWARE: */

MBSTATE *
mbc_initstate(MBSTATE *st)
{
  return st;
}

MBSTATE *
mbc_copystate(MBSTATE st, MBSTATE *stc)
{
  *stc = st;
  return stc;
}  

int
mbc_is_valid(const char *string, const MBSTATE *st)
{
  return TRUE;
}

const char *
mbc_next(const char *string, MBSTATE *st)
{
  return string + 1;
}  

const char *
mbc_inc(const char **p, MBSTATE *st)
{
  return (*p)++;
}

char *
mbc_first(const char *string, const MBSTATE *st)
{
  char p[2] = " ";
  p[0] = string[0];
  return mysavestring(p);
}       

int
mbc_charwidth(const char *p, MBSTATE *st)
{
  return 1;
}

/* copy the first byte in p to *q, incrementing q */
void
mbc_copy(const char *p, char **q, MBSTATE *st)
{
  *(*q)++ = *p;
}


int
is_multibyte(const char *p, const MBSTATE *st)
{
  return FALSE;
}

size_t
mbc_strnlen(const char *string, size_t maxlen, MBSTATE *st)
{
  return strnlen(string, maxlen);
}

#endif



void test_mbc_strnlen(int UNUSED(argc), char** UNUSED(argv), enum test_stage stage) {
  MBSTATE st; 
  const char *p, teststring[] = "hihi אני יכול לאכול זכוכית וזה לא מזיק לי jaja!";

  if (stage != TEST_AT_PROGRAM_START)
    return;
  printf("teststring: <%s>\n", teststring);
  for (mbc_initstate(&st), p = teststring; *p ; mbc_inc(&p, &st))
    printf("%s: len: %d multi? %s\n", mbc_first(p, &st), (int) mbc_charwidth(p, &st), is_multibyte(p, &st) ? "ja" : "nee");
  printf("Totale lengte: %d\n", (int) mbc_strnlen(teststring, 1000, mbc_initstate(&st)));
  
  exit(0);
}       

void test_utf8(int argc, char**argv, enum test_stage stage) {
  char buf[BUFFSIZE];
  MBSTATE st;
  int i;
  FILE *fp_in, *fp_out;
  
  if (stage != TEST_AT_PROGRAM_START)
    return;
  if (argc < 3)
    myerror(FATAL|NOERRNO, "need 2 args");

  fp_in = fopen(argv[1],"r");
  if (!fp_in)
    myerror(FATAL|USE_ERRNO, "Kan %s niet lezen", argv[1]); 

  fp_out= fopen(argv[2],"w");
  if (!fp_out)
    myerror(FATAL|USE_ERRNO, "Kan %s niet schrijven", argv[2]);
  mbc_initstate(&st);
  while (fgets(buf, BUFFSIZE, fp_in)) {
    int mbl =  mbc_strnlen(buf, BUFFSIZE, &st);
    int len =  strnlen(buf, BUFFSIZE);

    puts(buf); fputs(buf, fp_out);
    for(i = 0; i< mbl -1; i++) {
      putchar(len == mbl ?  '*' : '+');
      fputc(len == mbl ?  '*' : '+', fp_out);
    }   
    putchar('\n');
    fputc('\n', fp_out);
  }
  exit(0);
}


static char *repeat(const char *repetendum, int times) {
  int i;
  char *result= NULL;
  for (i=0; i < times; i++) 
    result = append_and_free_old(result,repetendum);
  return result;
}

void test_colourless_strnlen(int UNUSED(argc), char** UNUSED(argv), enum test_stage stage) {
  char buf[BUFFSIZE], *copy;
  if (stage != TEST_AFTER_OPTION_PARSING)
    return;

  while(fgets(buf, BUFFSIZE, stdin)) {
    int len = strnlen(buf, BUFFSIZE), len2;
    char *marked, *stars;
    if (len > 0 && buf[len-1] == '\n')
      buf[len-1] = '\0';
    marked = mark_invisible(buf);
    len2 = colourless_strlen(marked, &copy, 0);
    stars = repeat("*", len2);
    printf("%d\n<%s>\n<%s>\n<%s>\n\n", len, buf, copy, stars);
    free(copy);
    free(stars);
    free(marked);
  }
  exit(0);
}       
