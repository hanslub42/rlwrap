rlwrap v 0.45 February 2021

## WHAT IT IS: 

rlwrap is a 'readline wrapper', a small utility that uses the GNU
readline library to allow the editing of keyboard input for any
command. I couldn't find anything like it when I needed it, so I wrote
this one back in 1999.  By now, there are (and, in hindsight, even
then there were) a number of good readline wrappers around, like rlfe,
distributed as part of the GNU readline library, and the amazing socat
(http://freecode.com/projects/socat). You should consider using rlwrap
when you need user-defined completion (by way of completion word
lists) and persistent history, or if you want to program 'special
effects' using the filter mechanism. As it is especially useful with
older or even obsolete software, rlwrap is written to compile and run
on a fairly wide range of not necessarily recent Unix-like systems
(FreeBSD, OSX, AIX, Solaris, QNX, cygwin, linux and probably quite a
few more)

## HOW TO USE IT:
If 

    $ <command> <args>

doesn't let you use arrow keys to edit input, or if you just want
decent input history and completion, try:

    $ rlwrap [-options] <command> <args>

You then can edit <command>'s input and recall the input history using
the arrow keys.  Input history is remembered accross invocations,
separately for different `<command>`s. Typing `!<prefix><TAB>` will recall
the last input line starting with `<prefix>`, CTRL-R will search the
input history.  With the -r and -f options you can specify the list of
words which rlwrap will use as possible completions, taking them from a 
file (-f option) or from `<command>`'s standard in/output.
rlwrap monitors `<command>`'s terminal settings, so that it can do the
right thing when command asks for single keypresses or for a password.
Commands that already use readline will always ask for (and get)
single keypresses, which makes rlwrapping them rather
pointless. However, if one uses the -a option, rlwrap will use its own
line editing and history. Unforunately, in this case, rlwrap cannot
detect whether `<command>` asks for a password. This can be remedied by
giving the password prompt (excluding trailing space and possibly the
first few letters) as an argument to the -a option.
 
## EXAMPLES:
Run netcat with command-line editing:

    rlwrap nc localhost 80

Run lprolog and use library1 and library2 to build a completion word
list:
  
    rlwrap -f library1 -f library2 lprolog

Run smbclient (which already uses readline), add all input and output
to completion list, complete local filenames, avoid showing (and
storing) passwords:

    rlwrap -cr -aPassword: smbclient '\\PEANUT\C' 

## INSTALLATION:
Usually just

    # ./configure; make install

See the INSTALL file for more information.

## FILTERS 
Filters are "plug-in" scripts that give you complete control
over rlwrap's input and output, history and completion. They are 
not much used,and their implementation and the example
filters are therefore still somewhat untested.

## AUTHOR
Hans Lub, hanslub42@gmail.com

## HOMEPAGE
https://github.com/hanslub42/rlwrap
