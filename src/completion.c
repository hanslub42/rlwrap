#line 1 "completion.rb"
/*  Completion.c is generated from completion.rb by the program rbgen
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
       e-mail:  hanslub42@gmail.com
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

/* rbgen generated code begins here */
/* rbgen: $Id: rbgen.in,v 1.3 2003/10/24 01:31:21 damo Exp $ */
#define RB_CUSTOMIZE
#define rbdata_t char
#define RB_CMP(s, t) compare(s, t)
#define RB_ENTRY(name) rb##name
#undef RB_INLINE
#define RB_STATIC static
#line 1 "completion.c"
/*
 * RCS $Id: redblack.h,v 1.9 2003/10/24 01:31:21 damo Exp $
 */

/*
   Redblack balanced tree algorithm
   Copyright (C) Damian Ivereigh 2000

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version. See the file COPYING for details.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Header file for redblack.c, should be included by any code that 
** uses redblack.c since it defines the functions 
*/ 
 
/* Stop multiple includes */
#ifndef _REDBLACK_H

#ifndef RB_CUSTOMIZE
/*
 * Without customization, the data member in the tree nodes is a void
 * pointer, and you need to pass in a comparison function to be
 * applied at runtime.  With customization, you specify the data type
 * as the macro RB_ENTRY(data_t) (has to be a macro because compilers
 * gag on typdef void) and the name of the compare function as the
 * value of the macro RB_CMP. Because the comparison function is
 * compiled in, RB_CMP only needs to take two arguments.  If your
 * content type is not a pointer, define INLINE to get direct access.
 */
#define rbdata_t	void
#define RB_CMP(s, t, e)	(*rbinfo->rb_cmp)(s, t, e)
#undef RB_INLINE
#define RB_ENTRY(name)	rb##name
#endif /* RB_CUSTOMIZE */

#ifndef RB_STATIC
#define RB_STATIC
#endif

/* Modes for rblookup */
#define RB_NONE -1	    /* None of those below */
#define RB_LUEQUAL 0	/* Only exact match */
#define RB_LUGTEQ 1		/* Exact match or greater */
#define RB_LULTEQ 2		/* Exact match or less */
#define RB_LULESS 3		/* Less than key (not equal to) */
#define RB_LUGREAT 4	/* Greater than key (not equal to) */
#define RB_LUNEXT 5		/* Next key after current */
#define RB_LUPREV 6		/* Prev key before current */
#define RB_LUFIRST 7	/* First key in index */
#define RB_LULAST 8		/* Last key in index */

/* For rbwalk - pinched from search.h */
typedef enum
{
  preorder,
  postorder,
  endorder,
  leaf
}
VISIT;

struct RB_ENTRY(lists) { 
const struct RB_ENTRY(node) *rootp; 
const struct RB_ENTRY(node) *nextp; 
}; 
 
#define RBLIST struct RB_ENTRY(lists) 

struct RB_ENTRY(tree) {
#ifndef RB_CUSTOMIZE
		/* comparison routine */
int (*rb_cmp)(const void *, const void *, const void *);
		/* config data to be passed to rb_cmp */
const void *rb_config;
		/* root of tree */
#endif /* RB_CUSTOMIZE */
struct RB_ENTRY(node) *rb_root;
};

#ifndef RB_CUSTOMIZE
RB_STATIC struct RB_ENTRY(tree) *rbinit(int (*)(const void *, const void *, const void *),
		 const void *);
#else
RB_STATIC struct RB_ENTRY(tree) *RB_ENTRY(init)(void);
#endif /* RB_CUSTOMIZE */

#ifndef no_delete
RB_STATIC const RB_ENTRY(data_t) *RB_ENTRY(delete)(const RB_ENTRY(data_t) *, struct RB_ENTRY(tree) *);
#endif

#ifndef no_find
RB_STATIC const RB_ENTRY(data_t) *RB_ENTRY(find)(const RB_ENTRY(data_t) *, struct RB_ENTRY(tree) *);
#endif

#ifndef no_lookup
RB_STATIC const RB_ENTRY(data_t) *RB_ENTRY(lookup)(int, const RB_ENTRY(data_t) *, struct RB_ENTRY(tree) *);
#endif

#ifndef no_search
RB_STATIC const RB_ENTRY(data_t) *RB_ENTRY(search)(const RB_ENTRY(data_t) *, struct RB_ENTRY(tree) *);
#endif

#ifndef no_destroy
RB_STATIC void RB_ENTRY(destroy)(struct RB_ENTRY(tree) *);
#endif

#ifndef no_walk
RB_STATIC void RB_ENTRY(walk)(const struct RB_ENTRY(tree) *,
		void (*)(const RB_ENTRY(data_t) *, const VISIT, const int, void *),
		void *); 
#endif

#ifndef no_readlist
RB_STATIC RBLIST *RB_ENTRY(openlist)(const struct RB_ENTRY(tree) *); 
RB_STATIC const RB_ENTRY(data_t) *RB_ENTRY(readlist)(RBLIST *); 
RB_STATIC void RB_ENTRY(closelist)(RBLIST *); 
#endif

/* Some useful macros */
#define rbmin(rbinfo) RB_ENTRY(lookup)(RB_LUFIRST, NULL, (rbinfo))
#define rbmax(rbinfo) RB_ENTRY(lookup)(RB_LULAST, NULL, (rbinfo))

#define _REDBLACK_H
#endif /* _REDBLACK_H */

/*
 *
 * $Log: redblack.h,v $
 * Revision 1.9  2003/10/24 01:31:21  damo
 * Patches from Eric Raymond: %prefix is implemented.  Various other small
 * changes avoid stepping on global namespaces and improve the documentation.
 *
 * Revision 1.8  2003/10/23 04:18:47  damo
 * Fixed up the rbgen stuff ready for the 1.3 release
 *
 * Revision 1.7  2002/08/26 03:11:40  damo
 * Fixed up a bunch of compiler warnings when compiling example4
 *
 * Tidies up the Makefile.am & Specfile.
 *
 * Renamed redblack to rbgen
 *
 * Revision 1.6  2002/08/26 01:03:35  damo
 * Patch from Eric Raymond to change the way the library is used:-
 *
 * Eric's idea is to convert libredblack into a piece of in-line code
 * generated by another program. This should be faster, smaller and easier
 * to use.
 *
 * This is the first check-in of his code before I start futzing with it!
 *
 * Revision 1.5  2002/01/30 07:54:53  damo
 * Fixed up the libtool versioning stuff (finally)
 * Fixed bug 500600 (not detecting a NULL return from malloc)
 * Fixed bug 509485 (no longer needs search.h)
 * Cleaned up debugging section
 * Allow multiple inclusions of redblack.h
 * Thanks to Matthias Andree for reporting (and fixing) these
 *
 * Revision 1.4  2000/06/06 14:43:43  damo
 * Added all the rbwalk & rbopenlist stuff. Fixed up malloc instead of sbrk.
 * Added two new examples
 *
 * Revision 1.3  2000/05/24 06:45:27  damo
 * Converted everything over to using const
 * Added a new example1.c file to demonstrate the worst case scenario
 * Minor fixups of the spec file
 *
 * Revision 1.2  2000/05/24 06:17:10  damo
 * Fixed up the License (now the LGPL)
 *
 * Revision 1.1  2000/05/24 04:15:53  damo
 * Initial import of files. Versions are now all over the place. Oh well
 *
 */

static char rcsid[]="$Id: redblack.c,v 1.9 2003/10/24 01:31:21 damo Exp $";

/*
   Redblack balanced tree algorithm
   Copyright (C) Damian Ivereigh 2000

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version. See the file COPYING for details.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Implement the red/black tree structure. It is designed to emulate
** the standard tsearch() stuff. i.e. the calling conventions are
** exactly the same
*/

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include "redblack.h"

#define assert(expr)

/* Uncomment this if you would rather use a raw sbrk to get memory
** (however the memory is never released again (only re-used). Can't
** see any point in using this these days.
*/
/* #define USE_SBRK */

enum nodecolour { BLACK, RED };

struct RB_ENTRY(node)
{
	struct RB_ENTRY(node) *left;		/* Left down */
	struct RB_ENTRY(node) *right;		/* Right down */
	struct RB_ENTRY(node) *up;		/* Up */
	enum nodecolour colour;		/* Node colour */
#ifdef RB_INLINE
	RB_ENTRY(data_t) key;		/* User's key (and data) */
#define RB_GET(x,y)		&x->y
#define RB_SET(x,y,v)		x->y = *(v)
#else
	const RB_ENTRY(data_t) *key;	/* Pointer to user's key (and data) */
#define RB_GET(x,y)		x->y
#define RB_SET(x,y,v)		x->y = v
#endif /* RB_INLINE */
};

/* Dummy (sentinel) node, so that we can make X->left->up = X
** We then use this instead of NULL to mean the top or bottom
** end of the rb tree. It is a black node.
**
** Initialization of the last field in this initializer is left implicit
** because it could be of any type.  We count on the compiler to zero it.
*/
struct RB_ENTRY(node) RB_ENTRY(_null)={&RB_ENTRY(_null), &RB_ENTRY(_null), &RB_ENTRY(_null), BLACK};
#define RBNULL (&RB_ENTRY(_null))

#if defined(USE_SBRK)

static struct RB_ENTRY(node) *RB_ENTRY(_alloc)();
static void RB_ENTRY(_free)(struct RB_ENTRY(node) *);

#else

static struct RB_ENTRY(node) *RB_ENTRY(_alloc)() {return (struct RB_ENTRY(node) *) malloc(sizeof(struct RB_ENTRY(node)));}
static void RB_ENTRY(_free)(struct RB_ENTRY(node) *x) {free(x);}

#endif

/* These functions are always needed */
static void RB_ENTRY(_left_rotate)(struct RB_ENTRY(node) **, struct RB_ENTRY(node) *);
static void RB_ENTRY(_right_rotate)(struct RB_ENTRY(node) **, struct RB_ENTRY(node) *);
static struct RB_ENTRY(node) *RB_ENTRY(_successor)(const struct RB_ENTRY(node) *);
static struct RB_ENTRY(node) *RB_ENTRY(_predecessor)(const struct RB_ENTRY(node) *);
static struct RB_ENTRY(node) *RB_ENTRY(_traverse)(int, const RB_ENTRY(data_t) * , struct RB_ENTRY(tree) *);

/* These functions may not be needed */
#ifndef no_lookup
static struct RB_ENTRY(node) *RB_ENTRY(_lookup)(int, const RB_ENTRY(data_t) * , struct RB_ENTRY(tree) *);
#endif

#ifndef no_destroy
static void RB_ENTRY(_destroy)(struct RB_ENTRY(node) *);
#endif

#ifndef no_delete
static void RB_ENTRY(_delete)(struct RB_ENTRY(node) **, struct RB_ENTRY(node) *);
static void RB_ENTRY(_delete_fix)(struct RB_ENTRY(node) **, struct RB_ENTRY(node) *);
#endif

#ifndef no_walk
static void RB_ENTRY(_walk)(const struct RB_ENTRY(node) *, void (*)(const RB_ENTRY(data_t) *, const VISIT, const int, void *), void *, int);
#endif

#ifndef no_readlist
static RBLIST *RB_ENTRY(_openlist)(const struct RB_ENTRY(node) *);
static const RB_ENTRY(data_t) * RB_ENTRY(_readlist)(RBLIST *);
static void RB_ENTRY(_closelist)(RBLIST *);
#endif

/*
** OK here we go, the balanced tree stuff. The algorithm is the
** fairly standard red/black taken from "Introduction to Algorithms"
** by Cormen, Leiserson & Rivest. Maybe one of these days I will
** fully understand all this stuff.
**
** Basically a red/black balanced tree has the following properties:-
** 1) Every node is either red or black (colour is RED or BLACK)
** 2) A leaf (RBNULL pointer) is considered black
** 3) If a node is red then its children are black
** 4) Every path from a node to a leaf contains the same no
**    of black nodes
**
** 3) & 4) above guarantee that the longest path (alternating
** red and black nodes) is only twice as long as the shortest
** path (all black nodes). Thus the tree remains fairly balanced.
*/

/*
 * Initialise a tree. Identifies the comparison routine and any config
 * data that must be sent to it when called.
 * Returns a pointer to the top of the tree.
 */
#ifndef RB_CUSTOMIZE
RB_STATIC struct RB_ENTRY(tree) *
rbinit(int (*cmp)(const void *, const void *, const void *), const void *config)
#else
RB_STATIC struct RB_ENTRY(tree) *RB_ENTRY(init)(void)
#endif /* RB_CUSTOMIZE */
{
	struct RB_ENTRY(tree) *retval;
	char c;

	c=rcsid[0]; /* This does nothing but shutup the -Wall */

	if ((retval=(struct RB_ENTRY(tree) *) malloc(sizeof(struct RB_ENTRY(tree))))==NULL)
		return(NULL);
	
#ifndef RB_CUSTOMIZE
	retval->rb_cmp=cmp;
	retval->rb_config=config;
#endif /* RB_CUSTOMIZE */
	retval->rb_root=RBNULL;

	return(retval);
}

#ifndef no_destroy
RB_STATIC void
RB_ENTRY(destroy)(struct RB_ENTRY(tree) *rbinfo)
{
	if (rbinfo==NULL)
		return;

	if (rbinfo->rb_root!=RBNULL)
		RB_ENTRY(_destroy)(rbinfo->rb_root);
	
	free(rbinfo);
}
#endif /* no_destroy */

#ifndef no_search
RB_STATIC const RB_ENTRY(data_t) *
RB_ENTRY(search)(const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
	struct RB_ENTRY(node) *x;

	if (rbinfo==NULL)
		return(NULL);

	x=RB_ENTRY(_traverse)(1, key, rbinfo);

	return((x==RBNULL) ? NULL : RB_GET(x, key));
}
#endif /* no_search */

#ifndef no_find
RB_STATIC const RB_ENTRY(data_t) * 
RB_ENTRY(find)(const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
	struct RB_ENTRY(node) *x;

	if (rbinfo==NULL)
		return(NULL);

	/* If we have a NULL root (empty tree) then just return */
	if (rbinfo->rb_root==RBNULL)
		return(NULL);

	x=RB_ENTRY(_traverse)(0, key, rbinfo);

	return((x==RBNULL) ? NULL : RB_GET(x, key));
}
#endif /* no_find */

#ifndef no_delete
RB_STATIC const RB_ENTRY(data_t) * 
RB_ENTRY(delete)(const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
	struct RB_ENTRY(node) *x;
	const RB_ENTRY(data_t) * y;

	if (rbinfo==NULL)
		return(NULL);

	x=RB_ENTRY(_traverse)(0, key, rbinfo);

	if (x==RBNULL)
	{
		return(NULL);
	}
	else
	{
		y=RB_GET(x, key);
		RB_ENTRY(_delete)(&rbinfo->rb_root, x);

		return(y);
	}
}
#endif /* no_delete */

#ifndef no_walk
RB_STATIC void
RB_ENTRY(walk)(const struct RB_ENTRY(tree) *rbinfo, void (*action)(const RB_ENTRY(data_t) *, const VISIT, const int, void *), void *arg)
{
	if (rbinfo==NULL)
		return;

	RB_ENTRY(_walk)(rbinfo->rb_root, action, arg, 0);
}
#endif /* no_walk */

#ifndef no_readlist
RB_STATIC RBLIST *
RB_ENTRY(openlist)(const struct RB_ENTRY(tree) *rbinfo)
{
	if (rbinfo==NULL)
		return(NULL);

	return(RB_ENTRY(_openlist)(rbinfo->rb_root));
}

RB_STATIC const RB_ENTRY(data_t) * 
RB_ENTRY(readlist)(RBLIST *rblistp)
{
	if (rblistp==NULL)
		return(NULL);

	return(RB_ENTRY(_readlist)(rblistp));
}

RB_STATIC void
RB_ENTRY(closelist)(RBLIST *rblistp)
{
	if (rblistp==NULL)
		return;

	RB_ENTRY(_closelist)(rblistp);
}
#endif /* no_readlist */

#ifndef no_lookup
RB_STATIC const RB_ENTRY(data_t) * 
RB_ENTRY(lookup)(int mode, const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
	struct RB_ENTRY(node) *x;

	/* If we have a NULL root (empty tree) then just return NULL */
	if (rbinfo==NULL || rbinfo->rb_root==NULL)
		return(NULL);

	x=RB_ENTRY(_lookup)(mode, key, rbinfo);

	return((x==RBNULL) ? NULL : RB_GET(x, key));
}
#endif /* no_lookup */

/* --------------------------------------------------------------------- */

/* Search for and if not found and insert is true, will add a new
** node in. Returns a pointer to the new node, or the node found
*/
static struct RB_ENTRY(node) *
RB_ENTRY(_traverse)(int insert, const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
	struct RB_ENTRY(node) *x,*y,*z;
	int cmp;
	int found=0;
	int cmpmods();

	y=RBNULL; /* points to the parent of x */
	x=rbinfo->rb_root;

	/* walk x down the tree */
	while(x!=RBNULL && found==0)
	{
		y=x;
		/* printf("key=%s, RB_GET(x, key)=%s\n", key, RB_GET(x, key)); */
#ifndef RB_CUSTOMIZE
		cmp=RB_CMP(key, RB_GET(x, key), rbinfo->rb_config);
#else
		cmp=RB_CMP(key, RB_GET(x, key));
#endif /* RB_CUSTOMIZE */

		if (cmp<0)
			x=x->left;
		else if (cmp>0)
			x=x->right;
		else
			found=1;
	}

	if (found || !insert)
		return(x);

	if ((z=RB_ENTRY(_alloc)())==NULL)
	{
		/* Whoops, no memory */
		return(RBNULL);
	}

	RB_SET(z, key, key);
	z->up=y;
	if (y==RBNULL)
	{
		rbinfo->rb_root=z;
	}
	else
	{
#ifndef RB_CUSTOMIZE
		cmp=RB_CMP(RB_GET(z, key), RB_GET(y, key), rbinfo->rb_config);
#else
		cmp=RB_CMP(RB_GET(z, key), RB_GET(y, key));
#endif /* RB_CUSTOMIZE */
		if (cmp<0)
			y->left=z;
		else
			y->right=z;
	}

	z->left=RBNULL;
	z->right=RBNULL;

	/* colour this new node red */
	z->colour=RED;

	/* Having added a red node, we must now walk back up the tree balancing
	** it, by a series of rotations and changing of colours
	*/
	x=z;

	/* While we are not at the top and our parent node is red
	** N.B. Since the root node is garanteed black, then we
	** are also going to stop if we are the child of the root
	*/

	while(x != rbinfo->rb_root && (x->up->colour == RED))
	{
		/* if our parent is on the left side of our grandparent */
		if (x->up == x->up->up->left)
		{
			/* get the right side of our grandparent (uncle?) */
			y=x->up->up->right;
			if (y->colour == RED)
			{
				/* make our parent black */
				x->up->colour = BLACK;
				/* make our uncle black */
				y->colour = BLACK;
				/* make our grandparent red */
				x->up->up->colour = RED;

				/* now consider our grandparent */
				x=x->up->up;
			}
			else
			{
				/* if we are on the right side of our parent */
				if (x == x->up->right)
				{
					/* Move up to our parent */
					x=x->up;
					RB_ENTRY(_left_rotate)(&rbinfo->rb_root, x);
				}

				/* make our parent black */
				x->up->colour = BLACK;
				/* make our grandparent red */
				x->up->up->colour = RED;
				/* right rotate our grandparent */
				RB_ENTRY(_right_rotate)(&rbinfo->rb_root, x->up->up);
			}
		}
		else
		{
			/* everything here is the same as above, but
			** exchanging left for right
			*/

			y=x->up->up->left;
			if (y->colour == RED)
			{
				x->up->colour = BLACK;
				y->colour = BLACK;
				x->up->up->colour = RED;

				x=x->up->up;
			}
			else
			{
				if (x == x->up->left)
				{
					x=x->up;
					RB_ENTRY(_right_rotate)(&rbinfo->rb_root, x);
				}

				x->up->colour = BLACK;
				x->up->up->colour = RED;
				RB_ENTRY(_left_rotate)(&rbinfo->rb_root, x->up->up);
			}
		}
	}

	/* Set the root node black */
	(rbinfo->rb_root)->colour = BLACK;

	return(z);
}

#ifndef no_lookup
/* Search for a key according to mode (see redblack.h)
*/
static struct RB_ENTRY(node) *
RB_ENTRY(_lookup)(int mode, const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
	struct RB_ENTRY(node) *x,*y;
	int cmp;
	int found=0;

	y=RBNULL; /* points to the parent of x */
	x=rbinfo->rb_root;

	if (mode==RB_LUFIRST)
	{
		/* Keep going left until we hit a NULL */
		while(x!=RBNULL)
		{
			y=x;
			x=x->left;
		}

		return(y);
	}
	else if (mode==RB_LULAST)
	{
		/* Keep going right until we hit a NULL */
		while(x!=RBNULL)
		{
			y=x;
			x=x->right;
		}

		return(y);
	}

	/* walk x down the tree */
	while(x!=RBNULL && found==0)
	{
		y=x;
		/* printf("key=%s, RB_GET(x, key)=%s\n", key, RB_GET(x, key)); */
#ifndef RB_CUSTOMIZE
		cmp=RB_CMP(key, RB_GET(x, key), rbinfo->rb_config);
#else
		cmp=RB_CMP(key, RB_GET(x, key));
#endif /* RB_CUSTOMIZE */


		if (cmp<0)
			x=x->left;
		else if (cmp>0)
			x=x->right;
		else
			found=1;
	}

	if (found && (mode==RB_LUEQUAL || mode==RB_LUGTEQ || mode==RB_LULTEQ))
		return(x);
	
	if (!found && (mode==RB_LUEQUAL || mode==RB_LUNEXT || mode==RB_LUPREV))
		return(RBNULL);
	
	if (mode==RB_LUGTEQ || (!found && mode==RB_LUGREAT))
	{
		if (cmp>0)
			return(RB_ENTRY(_successor)(y));
		else
			return(y);
	}

	if (mode==RB_LULTEQ || (!found && mode==RB_LULESS))
	{
		if (cmp<0)
			return(RB_ENTRY(_predecessor)(y));
		else
			return(y);
	}

	if (mode==RB_LUNEXT || (found && mode==RB_LUGREAT))
		return(RB_ENTRY(_successor)(x));

	if (mode==RB_LUPREV || (found && mode==RB_LULESS))
		return(RB_ENTRY(_predecessor)(x));
	
	/* Shouldn't get here */
	return(RBNULL);
}
#endif /* no_lookup */

#ifndef no_destroy
/*
 * Destroy all the elements blow us in the tree
 * only useful as part of a complete tree destroy.
 */
static void
RB_ENTRY(_destroy)(struct RB_ENTRY(node) *x)
{
	if (x!=RBNULL)
	{
		if (x->left!=RBNULL)
			RB_ENTRY(_destroy)(x->left);
		if (x->right!=RBNULL)
			RB_ENTRY(_destroy)(x->right);
		RB_ENTRY(_free)(x);
	}
}
#endif /* no_destroy */

/*
** Rotate our tree thus:-
**
**             X        rb_left_rotate(X)--->            Y
**           /   \                                     /   \
**          A     Y     <---rb_right_rotate(Y)        X     C
**              /   \                               /   \
**             B     C                             A     B
**
** N.B. This does not change the ordering.
**
** We assume that neither X or Y is NULL
*/

static void
RB_ENTRY(_left_rotate)(struct RB_ENTRY(node) **rootp, struct RB_ENTRY(node) *x)
{
	struct RB_ENTRY(node) *y;

	assert(x!=RBNULL);
	assert(x->right!=RBNULL);

	y=x->right; /* set Y */

	/* Turn Y's left subtree into X's right subtree (move B)*/
	x->right = y->left;

	/* If B is not null, set it's parent to be X */
	if (y->left != RBNULL)
		y->left->up = x;

	/* Set Y's parent to be what X's parent was */
	y->up = x->up;

	/* if X was the root */
	if (x->up == RBNULL)
	{
		*rootp=y;
	}
	else
	{
		/* Set X's parent's left or right pointer to be Y */
		if (x == x->up->left)
		{
			x->up->left=y;
		}
		else
		{
			x->up->right=y;
		}
	}

	/* Put X on Y's left */
	y->left=x;

	/* Set X's parent to be Y */
	x->up = y;
}

static void
RB_ENTRY(_right_rotate)(struct RB_ENTRY(node) **rootp, struct RB_ENTRY(node) *y)
{
	struct RB_ENTRY(node) *x;

	assert(y!=RBNULL);
	assert(y->left!=RBNULL);

	x=y->left; /* set X */

	/* Turn X's right subtree into Y's left subtree (move B) */
	y->left = x->right;

	/* If B is not null, set it's parent to be Y */
	if (x->right != RBNULL)
		x->right->up = y;

	/* Set X's parent to be what Y's parent was */
	x->up = y->up;

	/* if Y was the root */
	if (y->up == RBNULL)
	{
		*rootp=x;
	}
	else
	{
		/* Set Y's parent's left or right pointer to be X */
		if (y == y->up->left)
		{
			y->up->left=x;
		}
		else
		{
			y->up->right=x;
		}
	}

	/* Put Y on X's right */
	x->right=y;

	/* Set Y's parent to be X */
	y->up = x;
}

/* Return a pointer to the smallest key greater than x
*/
static struct RB_ENTRY(node) *
RB_ENTRY(_successor)(const struct RB_ENTRY(node) *x)
{
	struct RB_ENTRY(node) *y;

	if (x->right!=RBNULL)
	{
		/* If right is not NULL then go right one and
		** then keep going left until we find a node with
		** no left pointer.
		*/
		for (y=x->right; y->left!=RBNULL; y=y->left);
	}
	else
	{
		/* Go up the tree until we get to a node that is on the
		** left of its parent (or the root) and then return the
		** parent.
		*/
		y=x->up;
		while(y!=RBNULL && x==y->right)
		{
			x=y;
			y=y->up;
		}
	}
	return(y);
}

/* Return a pointer to the largest key smaller than x
*/
static struct RB_ENTRY(node) *
RB_ENTRY(_predecessor)(const struct RB_ENTRY(node) *x)
{
	struct RB_ENTRY(node) *y;

	if (x->left!=RBNULL)
	{
		/* If left is not NULL then go left one and
		** then keep going right until we find a node with
		** no right pointer.
		*/
		for (y=x->left; y->right!=RBNULL; y=y->right);
	}
	else
	{
		/* Go up the tree until we get to a node that is on the
		** right of its parent (or the root) and then return the
		** parent.
		*/
		y=x->up;
		while(y!=RBNULL && x==y->left)
		{
			x=y;
			y=y->up;
		}
	}
	return(y);
}

#ifndef no_delete
/* Delete the node z, and free up the space
*/
static void
RB_ENTRY(_delete)(struct RB_ENTRY(node) **rootp, struct RB_ENTRY(node) *z)
{
	struct RB_ENTRY(node) *x, *y;

	if (z->left == RBNULL || z->right == RBNULL)
		y=z;
	else
		y=RB_ENTRY(_successor)(z);

	if (y->left != RBNULL)
		x=y->left;
	else
		x=y->right;

	x->up = y->up;

	if (y->up == RBNULL)
	{
		*rootp=x;
	}
	else
	{
		if (y==y->up->left)
			y->up->left = x;
		else
			y->up->right = x;
	}

	if (y!=z)
	{
		RB_SET(z, key, RB_GET(y, key));
	}

	if (y->colour == BLACK)
		RB_ENTRY(_delete_fix)(rootp, x);

	RB_ENTRY(_free)(y);
}

/* Restore the reb-black properties after a delete */
static void
RB_ENTRY(_delete_fix)(struct RB_ENTRY(node) **rootp, struct RB_ENTRY(node) *x)
{
	struct RB_ENTRY(node) *w;

	while (x!=*rootp && x->colour==BLACK)
	{
		if (x==x->up->left)
		{
			w=x->up->right;
			if (w->colour==RED)
			{
				w->colour=BLACK;
				x->up->colour=RED;
				rb_left_rotate(rootp, x->up);
				w=x->up->right;
			}

			if (w->left->colour==BLACK && w->right->colour==BLACK)
			{
				w->colour=RED;
				x=x->up;
			}
			else
			{
				if (w->right->colour == BLACK)
				{
					w->left->colour=BLACK;
					w->colour=RED;
					RB_ENTRY(_right_rotate)(rootp, w);
					w=x->up->right;
				}


				w->colour=x->up->colour;
				x->up->colour = BLACK;
				w->right->colour = BLACK;
				RB_ENTRY(_left_rotate)(rootp, x->up);
				x=*rootp;
			}
		}
		else
		{
			w=x->up->left;
			if (w->colour==RED)
			{
				w->colour=BLACK;
				x->up->colour=RED;
				RB_ENTRY(_right_rotate)(rootp, x->up);
				w=x->up->left;
			}

			if (w->right->colour==BLACK && w->left->colour==BLACK)
			{
				w->colour=RED;
				x=x->up;
			}
			else
			{
				if (w->left->colour == BLACK)
				{
					w->right->colour=BLACK;
					w->colour=RED;
					RB_ENTRY(_left_rotate)(rootp, w);
					w=x->up->left;
				}

				w->colour=x->up->colour;
				x->up->colour = BLACK;
				w->left->colour = BLACK;
				RB_ENTRY(_right_rotate)(rootp, x->up);
				x=*rootp;
			}
		}
	}

	x->colour=BLACK;
}
#endif /* no_delete */

#ifndef no_walk
static void
RB_ENTRY(_walk)(const struct RB_ENTRY(node) *x, void (*action)(const RB_ENTRY(data_t) *, const VISIT, const int, void *), void *arg, int level)
{
	if (x==RBNULL)
		return;

	if (x->left==RBNULL && x->right==RBNULL)
	{
		/* leaf */
		(*action)(RB_GET(x, key), leaf, level, arg);
	}
	else
	{
		(*action)(RB_GET(x, key), preorder, level, arg);

		RB_ENTRY(_walk)(x->left, action, arg, level+1);

		(*action)(RB_GET(x, key), postorder, level, arg);

		RB_ENTRY(_walk)(x->right, action, arg, level+1);

		(*action)(RB_GET(x, key), endorder, level, arg);
	}
}
#endif /* no_walk */

#ifndef no_readlist
static RBLIST *
RB_ENTRY(_openlist)(const struct RB_ENTRY(node) *rootp)
{
	RBLIST *rblistp;

	rblistp=(RBLIST *) malloc(sizeof(RBLIST));
	if (!rblistp)
		return(NULL);

	rblistp->rootp=rootp;
	rblistp->nextp=rootp;

	if (rootp!=RBNULL)
	{
		while(rblistp->nextp->left!=RBNULL)
		{
			rblistp->nextp=rblistp->nextp->left;
		}
	}

	return(rblistp);
}

static const RB_ENTRY(data_t) * 
RB_ENTRY(_readlist)(RBLIST *rblistp)
{
	const RB_ENTRY(data_t) *key=NULL;

	if (rblistp!=NULL && rblistp->nextp!=RBNULL)
	{
		key=RB_GET(rblistp->nextp, key);
		rblistp->nextp=RB_ENTRY(_successor)(rblistp->nextp);
	}

	return(key);
}

static void
rb_closelist(RBLIST *rblistp)
{
	if (rblistp)
		free(rblistp);
}
#endif /* no_readlist */

#if defined(RB_USE_SBRK)
/* Allocate space for our nodes, allowing us to get space from
** sbrk in larger chucks.
*/
static struct RB_ENTRY(node) *rbfreep=NULL;

#define RB_ENTRY(NODE)ALLOC_CHUNK_SIZE 1000
static struct RB_ENTRY(node) *
RB_ENTRY(_alloc)()
{
	struct RB_ENTRY(node) *x;
	int i;

	if (rbfreep==NULL)
	{
		/* must grab some more space */
		rbfreep=(struct RB_ENTRY(node) *) sbrk(sizeof(struct RB_ENTRY(node)) * RB_ENTRY(NODE)ALLOC_CHUNK_SIZE);

		if (rbfreep==(struct RB_ENTRY(node) *) -1)
		{
			return(NULL);
		}

		/* tie them together in a linked list (use the up pointer) */
		for (i=0, x=rbfreep; i<RB_ENTRY(NODE)ALLOC_CHUNK_SIZE-1; i++, x++)
		{
			x->up = (x+1);
		}
		x->up=NULL;
	}

	x=rbfreep;
	rbfreep = rbfreep->up;
#ifdef RB_ALLOC
 	RB_ALLOC(ACCESS(x, key));
#endif /* RB_ALLOC */
	return(x);
}

/* free (dealloc) an RB_ENTRY(node) structure - add it onto the front of the list
** N.B. RB_ENTRY(node) need not have been allocated through rb_alloc()
*/
static void
RB_ENTRY(_free)(struct RB_ENTRY(node) *x)
{
#ifdef RB_FREE
 	RB_FREE(ACCESS(x, key));
#endif /* RB_FREE */
	x->up=rbfreep;
	rbfreep=x;
}

#endif

#if 0
int
RB_ENTRY(_check)(struct RB_ENTRY(node) *rootp)
{
	if (rootp==NULL || rootp==RBNULL)
		return(0);

	if (rootp->up!=RBNULL)
	{
		fprintf(stderr, "Root up pointer not RBNULL");
		dumptree(rootp, 0);
		return(1);
	}

	if (RB_ENTRY(_check)1(rootp))
	{
		RB_ENTRY(dumptree)(rootp, 0);
		return(1);
	}

	if (RB_ENTRY(count_black)(rootp)==-1)
	{
		RB_ENTRY(dumptree)(rootp, 0);
		return(-1);
	}

	return(0);
}

int
RB_ENTRY(_check1)(struct RB_ENTRY(node) *x)
{
	if (x->left==NULL || x->right==NULL)
	{
		fprintf(stderr, "Left or right is NULL");
		return(1);
	}

	if (x->colour==RED)
	{
		if (x->left->colour!=BLACK && x->right->colour!=BLACK)
		{
			fprintf(stderr, "Children of red node not both black, x=%ld", x);
			return(1);
		}
	}

	if (x->left != RBNULL)
	{
		if (x->left->up != x)
		{
			fprintf(stderr, "x->left->up != x, x=%ld", x);
			return(1);
		}

		if (rb_check1(x->left))
			return(1);
	}		

	if (x->right != RBNULL)
	{
		if (x->right->up != x)
		{
			fprintf(stderr, "x->right->up != x, x=%ld", x);
			return(1);
		}

		if (rb_check1(x->right))
			return(1);
	}		
	return(0);
}

RB_ENTRY(count_black)(struct RB_ENTRY(node) *x)
{
	int nleft, nright;

	if (x==RBNULL)
		return(1);

	nleft=RB_ENTRY(count_black)(x->left);
	nright=RB_ENTRY(count_black)(x->right);

	if (nleft==-1 || nright==-1)
		return(-1);

	if (nleft != nright)
	{
		fprintf(stderr, "Black count not equal on left & right, x=%ld", x);
		return(-1);
	}

	if (x->colour == BLACK)
	{
		nleft++;
	}

	return(nleft);
}

RB_ENTRY(dumptree)(struct RB_ENTRY(node) *x, int n)
{
	char *prkey();

	if (x!=NULL && x!=RBNULL)
	{
		n++;
		fprintf(stderr, "Tree: %*s %ld: left=%ld, right=%ld, colour=%s, key=%s",
			n,
			"",
			x,
			x->left,
			x->right,
			(x->colour==BLACK) ? "BLACK" : "RED",
			prkey(RB_GET(x, key)));

		RB_ENTRY(dumptree)(x->left, n);
		RB_ENTRY(dumptree)(x->right, n);
	}	
}
#endif

/*
 * $Log: redblack.c,v $
 * Revision 1.9  2003/10/24 01:31:21  damo
 * Patches from Eric Raymond: %prefix is implemented.  Various other small
 * changes avoid stepping on global namespaces and improve the documentation.
 *
 * Revision 1.8  2002/08/26 05:33:47  damo
 * Some minor fixes:-
 * Stopped ./configure warning about stuff being in the wrong order
 * Fixed compiler warning about const (not sure about this)
 * Changed directory of redblack.c in documentation
 *
 * Revision 1.7  2002/08/26 03:11:40  damo
 * Fixed up a bunch of compiler warnings when compiling example4
 *
 * Tidies up the Makefile.am & Specfile.
 *
 * Renamed redblack to rbgen
 *
 * Revision 1.6  2002/08/26 01:03:35  damo
 * Patch from Eric Raymond to change the way the library is used:-
 *
 * Eric's idea is to convert libredblack into a piece of in-line code
 * generated by another program. This should be faster, smaller and easier
 * to use.
 *
 * This is the first check-in of his code before I start futzing with it!
 *
 * Revision 1.5  2002/01/30 07:54:53  damo
 * Fixed up the libtool versioning stuff (finally)
 * Fixed bug 500600 (not detecting a NULL return from malloc)
 * Fixed bug 509485 (no longer needs search.h)
 * Cleaned up debugging section
 * Allow multiple inclusions of redblack.h
 * Thanks to Matthias Andree for reporting (and fixing) these
 *
 * Revision 1.4  2000/06/06 14:43:43  damo
 * Added all the rbwalk & rbopenlist stuff. Fixed up malloc instead of sbrk.
 * Added two new examples
 *
 * Revision 1.3  2000/05/24 06:45:27  damo
 * Converted everything over to using const
 * Added a new example1.c file to demonstrate the worst case scenario
 * Minor fixups of the spec file
 *
 * Revision 1.2  2000/05/24 06:17:10  damo
 * Fixed up the License (now the LGPL)
 *
 * Revision 1.1  2000/05/24 04:15:53  damo
 * Initial import of files. Versions are now all over the place. Oh well
 *
 */

/* rbgen generated code ends here */
#line 77 "completion.rb"


 

/* forward declarations */
static struct rbtree *completion_tree;


static char *my_history_completion_function(char *prefix, int state);
static void print_list(void);


static void
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


static void
print_list()
{
  const char *word;
  RBLIST *completion_list = rbopenlist(completion_tree);	/* uses mymalloc() internally, so no chance of getting a NULL pointer back */

  printf("Completions:\n");
  while ((word = rbreadlist(completion_list)))
    printf("%s\n", word);
  rbcloselist(completion_list);
}

static char *rbtree_to_string(const struct rbtree *rb, int max_items) {
   const char *word;
   char  *result = NULL;
   int i;
   RBLIST *list = rbopenlist(rb);
   for (i = 0; (word = rbreadlist(list)) && (i < max_items); i++) {
      if (i > 0)
         result = append_and_free_old(result, ", ");
      result = append_and_free_old(result, word);
   }
   if (i >= max_items)
      result = append_and_free_old(result, "...");
   rbcloselist(list);
   return result;
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
  if (! feof(compl_fp) && ferror(compl_fp))   /* at least in GNU libc, errno will be set in this case. If not, no harm is done */
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
  
  /* if (*prefix == '!')
    return my_history_completion_function(prefix + 1, state); */

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
	/* DPRINTF1(DEBUG_COMPLETION, "Adding %s to completion list ", word); */
      }
    }
    if (completion_type & COMPLETE_FILENAMES) {
      change_working_directory();
      DPRINTF1(DEBUG_COMPLETION, "Starting milking of rl_filename_completion_function, prefix = <%s> ", prefix);
      for (count = 0;
	   (word = copy_and_free_string_for_malloc_debug(rl_filename_completion_function(prefix, count)));
	   count++) {	/* using rl_filename_completion_function means
			   that completing filenames will always be case-sensitive */
        DPRINTF1(DEBUG_COMPLETION, "Adding <%s> to completion list ", word);
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
      fragments = split_on_single_char(filtered,  '\t', 3);
      if ( strncmp(fragments[0], rl_line_buffer, length) ||  strncmp(fragments[1], prefix,length)) {
	  myerror(FATAL|NOERRNO, "filter has illegally messed with completion message\n"); /* it should ONLY have changed the completion word list  */
      }

      my_rbdestroy(scratch_tree); /* burn the old scratch tree (but leave the completion tree alone)  */
      scratch_tree = rbinit();    /* now grow a new one */
      words = split_with(fragments[2], " ");
      for(plist = words;(word = *plist); plist++) {
	if (!*word)
	  continue; /* empty space at beginning or end of the word list results in an empty word. skip it now */	
	rbsearch(mysavestring(word), scratch_tree); /* add the filtered completions to the new scratch tree */
	DPRINTF1(DEBUG_COMPLETION, "Adding %s to completion list ", word); 
      }
      free_splitlist(words);
      free_splitlist(fragments);
      free(filtered);
      scratch_list = rbopenlist(scratch_tree);      /* flatten the tree into a new list */	  
    }

  
    DPRINTF1(DEBUG_COMPLETION, "scratch list: %s", rbtree_to_string(scratch_tree, 6));  
  } /* if state ==  0 */

  /* we get here each time the user presses TAB to cycle through the list */
  assert(scratch_tree != NULL);
  assert(scratch_list != NULL);
  if ((completion = rbreadlist(scratch_list))) {	/* read next possible completion */
    struct stat buf; 
    char *copy_for_readline = malloc_foreign(strlen(completion)+1);
    strcpy(copy_for_readline, completion);
    
  /* This doesn ... (comment disappeared) */   
    rl_filename_completion_desired = rl_filename_quoting_desired = (stat(completion, &buf) ? FALSE : TRUE); 

    DPRINTF1(DEBUG_COMPLETION, "Returning completion to readline: <%s>", copy_for_readline);
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
