#!/usr/bin/env python

"""filter that feeds some input to rlwrapped command at startup. This input will not be
echo'ed to the terminal"""

import sys, os
import argparse
sys.path.append(os.environ.get('RLWRAP_FILTERDIR',".")) # only needed when testing this filter from the command line (outside of  rlwrap)
import rlwrapfilter


parser = argparse.ArgumentParser(description=__doc__, add_help = False, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('input', nargs='?',type=str,help='input for command (use escape codes like \\n etc for line breaks)')                    
parser.add_argument('-t', '--timeout', type=float, help='timeout (how long command needs to digest input)', default = 0.1, dest='t')
parser.add_argument('-p', '--prompt', type=str, help='prompt (output by command, this can shorten the waiting time)', default = "", dest='prompt')


# Show the user how to incorporate the filter invocation in the rlwrap command line:
usage = parser.format_usage().removeprefix("usage: ")
parser.epilog = f"Use as follows: rlwrap -z '{usage}' command ... (don't forget the quotes!)"

args = parser.parse_args()

filter = rlwrapfilter.RlwrapFilter()


def input_handler(input):
    if not input_handler.initial_input_has_been_handled:
       filter.cloak_and_dagger(bytes(args.input, "utf-8").decode("unicode_escape"),args.prompt, args.t)
       input_handler.initial_input_has_been_handled = True
    return input

input_handler.initial_input_has_been_handled = False
    


filter = rlwrapfilter.RlwrapFilter(input_handler = input_handler) # pass filter functions at filter creation time ...


filter.help_text = parser.format_help() # let argparse write the manual
filter.help_text += f"\nExample: rlwrap  -z '{os.path.basename(__file__)} \"line1\\nline2\\n\"' command ..." 


filter.run()
