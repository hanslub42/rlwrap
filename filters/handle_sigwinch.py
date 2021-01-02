#!/usr/bin/env python3

"""a demo of using a filter to handle signals. I am still not convined of the usefulness of signal filtering"""

import sys, os, signal, argparse, re

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')


parser = argparse.ArgumentParser()
parser.add_argument("format", help='output format', nargs='?', default="LINES=%L;COLUMNS=%C")
parser.add_argument('-v', help='be verbose (show formatted output, and response)', action='store_true', dest = 'verbose')
parser.add_argument('-p', help='expected prompt', default="$ ", dest = 'prompt')


import rlwrapfilter

filter = rlwrapfilter.RlwrapFilter()

filter.help_text = "Usage: rlwrap [-rlwrap-options] -z 'handle_sigwinch [options] [<format>]' <command>\n"\
                   + "outputs <format> to <command>'s stdin, where %L is replaced by the new height, \n"\
                   + "and %C with the new width (default: 'export LINES=%L COLUMNS=%C')\n\nFilter options:\n" \
                   + parser.format_help()
                   

args = parser.parse_args()

filter.cloak_and_dagger_verbose =  args.verbose


def handle_signal(signo):
    if int(signo) == int(signal.SIGWINCH):                # signo is passed as a string, e.g. "11"
       new_cols, new_lines = os.get_terminal_size()       # stdin is still the users terminal
       output = re.sub('%L', str(new_lines), args.format)
       output = re.sub('%C', str(new_cols), output)      
       filter.cloak_and_dagger(output, args.prompt, 0.1)  # cloak_and_dagger: we don't want to see the dialogue (except when verbose)
    return signo                                          # don't mess with the signal itself
                   
filter.signal_handler = handle_signal

filter.run()
