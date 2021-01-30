#!/usr/bin/env python3

"""
a demo for completing paths in sh wrapperd shell

Usage:
rlwrap -z './sh_file_completion.py' sh
"""

import os
import sys
import shlex
import rlwrapfilter

def get_files_by_prefix(path):
        command = "ls -dp1 -- %s* 2>/dev/null" % shlex.quote(path)
        output = filter.cloak_and_dagger(command, "\$ ", 1)

        return output.replace('\r','').split('\n')

def completion_handler(line, path, completion_list):
    return get_files_by_prefix(path)

filter = rlwrapfilter.RlwrapFilter(completion_handler = completion_handler);
filter.set_completion_list_options(word_break_chars = [' ', '\t', '\n', '\"', '\\', '`', '@' '$', '>', '<', '=', ';', '|', '&', '{', '('])
filter.run()
