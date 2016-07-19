#!/usr/bin/python3

"""
a demo for simple prompt-munging

Usage:
rlwrap -z ./count_in_prompt.py telnet
"""
import rlwrapfilter

N = 0

filter = rlwrapfilter.RlwrapFilter()

def munge_prompt(prompt):
    global N

    if (prompt and filter.previous_tag == rlwrapfilter.TAG_OUTPUT):
        N = N + 1
        return "prompt {0} > ".format(N)
    else:
        return prompt

filter.help_text = '\n'.join(
    ["Usage: rlwrap -z {0} <command>".format(__file__),
     "replace prompt by simple counter",
     "(demonstrates some simple prompt-munging techniques)"]
)

filter.prompt_handler = munge_prompt

filter.run()


