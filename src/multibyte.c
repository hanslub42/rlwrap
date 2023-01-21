#include "rlwrap.h"
#include <limits.h>


/* rlwrap was written without worrying about multibyte characters.
   the functions below "retrofit" multi-byte support by providing
   functions that can be used with minimal adaptations in the usual
   idioms like
      for (char *p = s; *p; p++)
   which can be re-written as:
      for(mbc_initstate(&st), p = s; *p;  mbc_inc(&p, &st)) {


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
    DPRINTF1(DEBUG_READLINE, "invalid multi-byte charavter at stert of %s", M(p));
    width = 1; /* if we don''n recognise it, interpret it as a byte */
  }
  return width;
}

/* copy the first (wide) character in p to *q, incrementing q to one past the result. Does not NULL-terminate the copy */
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
mbc_is_valid(const char *UNUSED(string), const MBSTATE *UNUSED(st))
{
  return TRUE;
}

const char *
mbc_next(const char *string, MBSTATE *UNUSED(st))
{
  return string + 1;
}

const char *
mbc_inc(const char **p, MBSTATE *UNUSED(st))
{
  return (*p)++;
}

char *
mbc_first(const char *string, const MBSTATE *UNUSED(st))
{
  char p[2] = " ";
  p[0] = string[0];
  return mysavestring(p);
}

int
mbc_charwidth(const char *UNUSED(p), MBSTATE *UNUSED(st))
{
  return 1;
}

/* copy the first byte in p to *q, incrementing q */
void
mbc_copy(const char *p, char **q, MBSTATE *UNUSED(st))
{
  *(*q)++ = *p;
}


int
is_multibyte(const char *UNUSED(p), const MBSTATE *UNUSED(st))
{
  return FALSE;
}

size_t
mbc_strnlen(const char *string, size_t maxlen, MBSTATE *UNUSED(st))
{
  return strnlen(string, maxlen);
}

#endif

