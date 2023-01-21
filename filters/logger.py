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

parser.add_argument('--logfile', '-l', nargs='?',
                    type=argparse.FileType('a'),
                    default=open("/tmp/filterlog." + str(os.getpid()), mode='a'))
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


filter.help_text = "Usage: rlwrap -z 'logger [-l] logfile' <command>\n"\
                   + "log messages to a file (for debugging)\n"\
                   + "give logfile name as an argument, -l for long format\n"\
                   + "useful in a pipeline "\
                   + "(rlwrap -z 'pipeline logger in:filter:logger out')"


filter.run()
