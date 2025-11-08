rlwrap v 0.48 Nov 2025

## WHAT IT IS:

`rlwrap` is a wrapper that uses the [GNU
Readline](https://tiswww.case.edu/php/chet/readline/rltop.html)
library to allow the comfortable editing and recalling of keyboard
input for other commands.

There are many other such readline wrappers around, like rlfe and
socat, but `rlwrap` can be especially useful for those who need
user-defined completion (by way of completion word lists) or a
persistent history, and people who want to program 'special effects'
using the filter mechanism.

As it is often used with older software on obsolete systems, `rlwrap`
strives to compile and run on a fairly wide range of not necessarily
recent Unix-like systems (FreeBSD, OSX, HP-UX, AIX, Solaris, QNX,
cygwin, linux and probably quite a few more).  This would not have
been possible without [Polarhome](http://polarhome.com)'s now retired
'dinosaur zoo' of ageing Unix systems.


## WHEN AND HOW TO USE IT:

If 

    $ <command> <args>

displays the infamous `^[[D` when you press a left arrow key, or if
you just want decent input history and completion, try:

    $ rlwrap [rlwrap-options] <command> [command-args]

You should not notice any difference compared to directly calling `<command>
<args>`, except that you now can edit `<command>`'s input and recall
its entire input history using the arrow keys.


Input history is remembered accross invocations, separately for
different `<command>`s. `CTRL-R` will search the input history, like
in `bash`.  With the `-r` and `-f` options you can specify the list of
words which `rlwrap` will use as possible completions, taking them
from a file (`-f` option) or from `<command>`'s past in/output (-r
option).

`rlwrap` continually monitors `<command>`'s terminal settings, so that
it can do the right thing when it asks for single keypresses or
for a password.

Commands that already use Readline, or a similar library, will always
ask for (and get) single keypresses, so that `rlwrap`ping them doesn't
have any noticeable effect. To overcome this, one can use rlwrap with the
`--always-readline` (`-a`)  option; `rlwrap` will then use its own line
editing and history. Unforunately, in that case, `rlwrap` cannot
detect whether `<command>` asks for a password. This can be remedied
by giving the password prompt (excluding trailing space and possibly
the first few letters) as an argument to the -a option.
 
## EXAMPLES:

Run netcat with command-line editing:

    rlwrap nc localhost 80

Run lprolog and use its saved input history and lib.pl to build a
completion word list:
  
    rlwrap -f lib.pl -f . lprolog

Run smbclient (which already uses readline), add all input and output
to the completion list, complete local filenames, avoid showing (and
storing) passwords:

    rlwrap -cra -assword: smbclient '\\PEANUT\C' 



## LIMITATIONS

Programs that already have a sophisticated command-line (like fish or
fzf) will be unusable with rlwrap, as their virtuoso terminal handling 
is difficult to understand (rlwrap is not a terminal emulator!).
Of course, those programs won't *need* rlwrap, either. 

## FILTERS

Filters are `perl` or `python` plugins that enable complete (albeit
somewhat fragile) control over `rlwrap`'s input and output, echo,
prompt, history, completion and signal handling. They aren't used a
lot, and remain therefore somewhat untested. `rlwrap -z listing` lists
the installed filters, `rlwrap -z <filter>` displays a short help text
for `<filter>`

## AUTHORS

The GNU Readline library (written by Brian Fox and Chet Ramey) does
the hard work behind the scenes, the pty-handling code (written by
Geoff C. Wing) was taken practically unchanged from rxvt and
completion word lists are managed by Damian Ivereigh's libredblack
library. The rest was written by Hans Lub (hanslub42@gmail.com).

## HOMEPAGE
https://github.com/hanslub42/rlwrap
