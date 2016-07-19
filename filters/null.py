#!/usr/bin/python3

"""a demo for doing nothing"""

import rlwrapfilter

filter = rlwrapfilter.RlwrapFilter()

filter.help_text = "Usage: rlwrap [-options] -z null.py <command>\na filter that does nothing"

filter.run()
