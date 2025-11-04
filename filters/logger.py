#!/usr/bin/env python3

"""
a demo for logging communcations between rlrwap and filter

Usage:
rlwrap -z './logger.py -l logger.log' telnet
"""

import os
import sys

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')

import rlwrapfilter
import argparse
import re

filter = rlwrapfilter.RlwrapFilter()

parser = argparse.ArgumentParser()

parser.add_argument('--long_format', '-l',action='store_true', dest = 'long_format')
parser.add_argument('--log-all', '-i', action='store_true', dest = 'log_all')
parser.add_argument('logfile', nargs='?',type=argparse.FileType('w'), default=open("/tmp/filterlog." + str(os.getpid()), mode='a'))                    

args = parser.parse_args()
fd = args.logfile

# a message_handler is seldom used (as it cannot change messages, only examine them) 
# It gets called with the tag as its second argument
def logit(message, tag):
    tagname = filter.tag2name(tag)
    tagname = re.sub(r'^TAG_', '', tagname)
    mangled = message
    mangled = re.sub(r'\n', r'\\n', mangled)
    mangled = re.sub(r'\r', r'\\r', mangled)
    mangled = re.sub(r'\t', r'\\t', mangled)
    mangled = re.sub(r'\x1b', r'\\e', mangled)

    if (args.logfile):
        fd.write("{0} {1}\n".format(tagname, mangled))
    else:
        fd.write("{0:10s} {1}\n".format(tagname, mangled))

    fd.flush()
    return message


filter = rlwrapfilter.RlwrapFilter(message_handler=logit);

# "do nothing" handler
def just_copy(arg):
    return arg

def just_copy_list(*arg):
    return arg

# "do nothing" completion handler  (returning unchanged list of completions, without  input line and prefix)
def just_copy_completions(*arg):
    return list(arg[2:])

if args.log_all:
    filter.prompt_handler = just_copy
    filter.completion_handler = just_copy_completions
    filter.history_handler = just_copy
    filter.input_handler = just_copy
    filter.output_handler = just_copy
    filter.echo_handler = just_copy
    filter.hotkey_handler = just_copy_list
    filter.signal_handler = just_copy



filter.help_text = "Usage: rlwrap -z 'logger [-i] [-l] [logfile]' <command>\n"\
                   + "log messages to a file (for debugging)\n"\
                   + "give logfile name as an argument (default: /tmp/filterlog.$$), -l for long format\n"\
                   + "useful in a pipeline (rlwrap -z 'pipeline logger in:filter:logger out')\n"\
                   + "the -i option will make logger log all message types, not just those that are\n"\
                   + "relevant for filters up- or downstream in the pipeline" 

#filter.help_text = parser.format_help()
 

filter.run()
