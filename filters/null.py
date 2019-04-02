#!/usr/bin/env python3

"""a demo for doing nothing"""

import sys
import os

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')

import rlwrapfilter

filter = rlwrapfilter.RlwrapFilter()

filter.help_text = "Usage: rlwrap [-options] -z null.py <command>\n"\
                   + "a filter that does nothing"

filter.run()
