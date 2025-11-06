#!/usr/bin/env python

"""a demo for doing nothing (using argparse)"""

import sys, os
import argparse
sys.path.append(os.environ.get('RLWRAP_FILTERDIR',".")) # only needed when testing this filter from the command line (outside of  rlwrap)
import rlwrapfilter


parser = argparse.ArgumentParser(description=__doc__, add_help = False)
parser.add_argument('--some-flag', '-s', action='store_true', dest = 'some_flag')

# Show the user how to incorporate the filter invocation in the rlwrap command line:
usage = parser.format_usage().removeprefix("usage: ")
parser.epilog = f"Use as follows: rlwrap -z '{usage}' command ... (don't forget the quotes!)"

args = parser.parse_args()

def do_nothing(s):
    return s


filter = rlwrapfilter.RlwrapFilter(input_handler = do_nothing) # pass filter functions at filter creation time ...
filter.output_handler = do_nothing                             # ... or add them later

filter.help_text = parser.format_help() # let argparse write the manual

filter.run()
