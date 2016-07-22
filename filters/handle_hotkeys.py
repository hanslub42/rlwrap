#!/usr/bin/python3

"""handle hotkeys"""

import subprocess
import sys
import os

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')

import rlwrapfilter

filter = rlwrapfilter.RlwrapFilter()

filter.help_text = "Usage: rlwrap [-options] -z handle_hotkeys.py <command>\n"\
                   + "a filter that handles hotkeys"


def handle_hotkey(key, prefix, postfix):
    keyc = chr(ord(key)+64)
    if keyc == 'T': # control-t
        time = subprocess.check_output(["date","+%H:%M"], universal_newlines=True).rstrip()
        return ("({0})".format(time), prefix, postfix)
    elif keyc == 'Y':
        selection = subprocess.check_output(["xsel","-o"], universal_newlines=True)
        return ("", prefix + selection, postfix)
    elif keyc == 'P':
        return ("",prefix.upper(), postfix.upper())
    else:
        filter.error("unknown hotkey CTRL+{0}".format(keyc), rlwrapfilter.RlwrapFilterError())



filter.hotkey_handler = handle_hotkey


filter.run()
