/*  completion.c is generated from completion.rb by the program rbgen
    (cf. http://libredblack.sourceforge.net/)
    
    completion.rb: maintaining the completion list, my_completion_function()
    (callback for readline lib)

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

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"


#include "rlwrap.h"

#ifdef assert
#undef assert
#endif


int completion_is_case_sensitive = 1;

int
compare(const char *string1, const char *string2)
{
  const char *p1;
  const char *p2; 
  int count;

  for (p1 = string1, p2 = string2, count = 0;
       *p1 && *p2 && count < BUFFSIZE; p1++, p2++, count++) {
    char c1 = completion_is_case_sensitive ? *p1 : tolower(*p1);
    char c2 = completion_is_case_sensitive ? *p2 : tolower(*p2);

    if (c1 != c2)
      return (c1 < c2 ? -1 : 1);
  }
  if ((*p1 && *p2) || (!*p1 && !*p2))
    return 0;
  return (*p1 ? 1 : -1);
}





#ifdef malloc
#  undef malloc
#endif
#define malloc(x) mymalloc(x) /* This is a bit evil, but there is no other way to make libredblack use mymalloc() */


/* This file has to be processed by the program rbgen  */

%%rbgen
%type char
%cmp compare
%access pointer
%static
%omit
%%rbgen


/* forward declarations */
static struct rbtree *completion_tree;

static char *my_history_completion_function(char *prefix, int state);
static void print_list(void);


void
my_rbdestroy(struct rbtree *rb)
{				/* destroy rb tree, freeing the keys first */
  const char *key, *lastkey;

  for (key = rbmin(rb);
       key;
       lastkey = key, key =
       rblookup(RB_LUGREAT, key, rb), free((void *)lastkey))
    rbdelete(key, rb);
  rbdestroy(rb);
}


void
print_list()
{
  const char *word;
  RBLIST *completion_list = rbopenlist(completion_tree);	/* uses mymalloc() internally, so no chance of getting a NULL pointer back */

  printf("Completions:\n");
  while ((word = rbreadlist(completion_list)))
    printf("%s\n", word);
  rbcloselist(completion_list);
}

void
init_completer()
{
  completion_tree = rbinit();
}


void
add_word_to_completions(const char *word)
{
  rbsearch(mysavestring(word), completion_tree);	/* the tree stores *pointers* to the words, we have to allocate copies of them ourselves
							   freeing the tree will call free on the pointers to the words
							   valgrind reports the copies as lost, I don't understand this.' */
}


void
remove_word_from_completions(const char *word)
{
  free((char *) rbdelete(word, completion_tree));  /* why does rbdelete return a const *? I want to be able to free it! */	
}

void
feed_line_into_completion_list(const char *line)
{
  
  char **words = split_with(line, rl_basic_word_break_characters);
  char **plist, *word;
  for(plist = words;(word = *plist); plist++)
    add_word_to_completions(word);
  free_splitlist(words);
}

void
feed_file_into_completion_list(const char *completions_file)
{
  FILE *compl_fp;
  char buffer[BUFFSIZE];

  if ((compl_fp = fopen(completions_file, "r")) == NULL)
    myerror(FATAL|USE_ERRNO, "Could not open %s", completions_file);
  while (fgets(buffer, BUFFSIZE - 1, compl_fp) != NULL) {
    buffer[BUFFSIZE - 1] = '\0';	/* make sure buffer is properly terminated (it should be anyway, according to ANSI) */
    feed_line_into_completion_list(buffer);
  }
  if (errno)
    myerror(FATAL|USE_ERRNO, "Couldn't read completions from %s", completions_file);
  fclose(compl_fp);
  /* print_list(); */
}


#define COMPLETE_FILENAMES 1
#define COMPLETE_FROM_LIST 2
#define COMPLETE_USERNAMES 4
#define FILTER_COMPLETIONS 8
#define COMPLETE_PARANORMALLY 16 /* read user's thoughts */


int
get_completion_type()
{				/* some day, this function will inspect the current line and make rlwrap complete
				   differently according to the word *preceding* the one we're completing ' */
  return (COMPLETE_FROM_LIST | (complete_filenames ? COMPLETE_FILENAMES : 0) | (filter_pid ? FILTER_COMPLETIONS : 0));
}


/* helper function for my_completion_function */
static int
is_prefix(const char *s0, const char *s1)
{				/* s0 is prefix of s1 */
  const char *p0, *p1;
  int count;

  for (count = 0, p0 = s0, p1 = s1; *p0; count++, p0++, p1++) {
    char c0 = completion_is_case_sensitive ? *p0 : tolower(*p0);
    char c1 = completion_is_case_sensitive ? *p1 : tolower(*p1);

    if (c0 != c1 || count == BUFFSIZE)
      return FALSE;
  }
  return TRUE;
}

/* See readline doumentation: this function is called by readline whenever a completion is needed. The first time state == 0,
   whwnever the user presses TAB to cycle through the list, my_completion_function() is called again, but then with state != 0
   It should return the completion, which then will be freed by readline (so we'll hand back a copy instead of the real thing) ' */


char *
my_completion_function(char *prefix, int state)
{
  static struct rbtree *scratch_tree = NULL;
  static RBLIST *scratch_list = NULL;	/* should remain unchanged between invocations */
  int completion_type, count;
  const char *word;
  const char *completion;
  
  rl_completion_append_character = *extra_char_after_completion;
  
  if (*prefix == '!')
    return my_history_completion_function(prefix + 1, state);

  if (state == 0) {		/* first time we're called for this prefix ' */
    if (scratch_list)
      rbcloselist(scratch_list);
    if (scratch_tree)
      my_rbdestroy(scratch_tree);
    scratch_tree = rbinit();	/* allocate scratch_tree. We will use this to get a sorted list of completions */
    /* now find all possible completions: */
    completion_type = get_completion_type();
    DPRINTF2(DEBUG_ALL, "completion_type: %d, filter_pid: %d", completion_type, filter_pid);
    if (completion_type & COMPLETE_FROM_LIST) {
      for (word = rblookup(RB_LUGTEQ, prefix, completion_tree);	/* start with first word >= prefix */
	   word && is_prefix(prefix, word);	/* as long as prefix is really prefix of word */
	   word = rblookup(RB_LUGREAT, word, completion_tree)) {	/* find next word in list */
	rbsearch(mysavestring(word), scratch_tree);	/* insert fresh copy of the word */
	/* DPRINTF1(DEBUG_READLINE, "Adding %s to completion list ", word); */
      }
    }
    if (completion_type & COMPLETE_FILENAMES) {
      change_working_directory();
      for (count = 0;
	   (word = copy_and_free_string_for_malloc_debug(rl_filename_completion_function(prefix, count)));
	   count++) {	/* using rl_filename_completion_function means
			   that completing filenames will always be case-sensitive */
	rbsearch(word, scratch_tree);
      }
    }

    scratch_list = rbopenlist(scratch_tree); /* OK, we now have our list with completions. We may have to filter it: */
    
    if (completion_type & FILTER_COMPLETIONS) {
      /* build message (filter_food) consisting of: input line, TAB, prefix, TAB, completion_1 SPACE completion_2 .... */
      char *word, *filtered, **words, **plist, **fragments;
      
      int length = strlen(rl_line_buffer) + strlen(prefix) + 3;
      char *filter_food = mymalloc(length);
      
      assert(strchr(rl_line_buffer,'\n') == NULL);
      
      
      sprintf(filter_food, "%s\t%s\t", rl_line_buffer, prefix);
      while((completion = rbreadlist(scratch_list))) {  
        filter_food = append_and_free_old(filter_food, completion);
        filter_food = append_and_free_old(filter_food, " ");
	  
      }
      /* feed message to filter */
      filtered = pass_through_filter(TAG_COMPLETION, filter_food);
      free(filter_food);
      rbcloselist(scratch_list);  /* throw away old list, and */
      DPRINTF1(DEBUG_ALL, "Filtered: %s", mangle_string_for_debug_log(filtered, 40));

      /* parse contents */
      fragments = split_on_single_char(filtered,  '\t');
      if (!fragments[0] || ! fragments[1] || !fragments[2]  ||  
          strncmp(fragments[0], rl_line_buffer, length) ||  strncmp(fragments[1], prefix,length)) {
	  myerror(FATAL|NOERRNO, "filter has illegally messed with completion message\n"); /* it should ONLY have changed the completion word list  */
      }

      my_rbdestroy(scratch_tree); /* burn the old scratch tree (but leave the completion tree alone)  */
      scratch_tree = rbinit();    /* now grow a new one */
      words = split_with(fragments[2], " ");
      for(plist = words;(word = *plist); plist++) {
	if (!*word)
	  continue; /* empty space at beginning or end of the word list results in an empty word. skip it now */	
	rbsearch(mysavestring(word), scratch_tree); /* add the filtered completions to the new scratch tree */
	DPRINTF1(DEBUG_READLINE, "Adding %s to completion list ", word); 
      }
      free_splitlist(words);
      free_splitlist(fragments);
      free(filtered);
      scratch_list = rbopenlist(scratch_tree);      /* flatten the tree into a new list */	  
    }

    
    
  } /* if state ==  0 */

  /* we get here each time the user presses TAB to cycle through the list */
  assert(scratch_tree != NULL);
  assert(scratch_list != NULL);
  if ((completion = rbreadlist(scratch_list))) {	/* read next possible completion */
    char *copy_for_readline = malloc_foreign(strlen(completion)+1);
    strcpy(copy_for_readline, completion);
    return copy_for_readline;	/* we cannot just return it as  readline will free it (and make rlwrap explode) */
  } else {
    return NULL;
  }
}




static char *
my_history_completion_function(char *prefix, int state)
{
  while (next_history());
  if (state || history_search_prefix(prefix, -1) < 0)
    return NULL;
  return mysavestring(current_history()->line);
}







/* The following sets edit modes for GNU EMACS
   Local Variables:
   mode:c
   End:  */
