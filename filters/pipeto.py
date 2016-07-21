#!/usr/bin/python3

# this is maybe the most practical of the filter examples. Is is also a test for rlwraps signal handling.
# At present, a CTRL-C in a pager will also kill rlwrap (bad)

import sys
import os

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')

import rlwrapfilter
import re

filter = rlwrapfilter.RlwrapFilter()
me = filter.name

raw_input = ''

filter.help_text = '\n'.join([
    "Usage: rlwrap -z {0} <command>".format(me),
    "Allow piping of <command>'s interactive output through pagers or other shell commands.",
    "When input of the form \"| shell pipeline\" is seen, <command>'s following output is sent through the pipeline\n"
    ])

pipeline = ''
prompt = ''
out_chunkno = 0
wait_text = "wait ..."


def input(message):
    global raw_input
    global pipeline
    input = None
    raw_input = message
    (input, bar, pipeline) = re.match(r'([^\|]*)(\|(.*))?', message).groups()
    return input


def output(message):
    global out_chunkno
    if (pipeline):
        if (out_chunkno == 0):
            out_chunkno += 1
            return wait_text
        else:
            return ""
    else:
        return message # replace first chunk by $wait_text


def prompt_handler(message):
    global pipeline
    prompt = message
    out_chunkno = 0
    if (pipeline):
        filter.send_output_oob("\x08" * len(wait_text) + "\n") # erase $wait_text and go to new line
        pipe = os.popen(pipeline, 'w')
        pipe.write(filter.cumulative_output)
        pipe.close()
        pipeline = None
        filter.send_output_oob("\n") # start prompt on new line
    return prompt


filter.prompts_are_never_empty = True
filter.input_handler = input
filter.output_handler = output
filter.prompt_handler = prompt_handler
filter.echo_handler = (lambda x: raw_input)

filter.run()
