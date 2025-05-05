#!/usr/bin/env python3

"""a simple filter to tame commands that don't terminate their output with a newline"""

import sys
import os

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')

import rlwrapfilter

filter = rlwrapfilter.RlwrapFilter()
filter.help_text = ("Usage: rlwrap [-options] -z \"nl_and_then_prompt.py ['<prompt>']\" <command>\n" +
                   "terminate <command> output with a newline and display new <prompt>")


substitute_prompt = sys.argv[1] if len(sys.argv) == 2 else " > "


def handle_prompt(last_line):
    filter.send_output_oob(f"{last_line}\n")
    return substitute_prompt
    

filter.prompt_handler = handle_prompt
filter.run()

