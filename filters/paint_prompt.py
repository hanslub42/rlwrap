#!/usr/bin/python3

"""
a demo for painting prompt

this filter tests rlwrap's tolerance for very heavily cooked prompts.
it uses the seldom-used xterm 256-colour mode (which may or may not work in your 
other terminal emulators like rxvt) 

try `rlwrap -z './paint_prompt.py 0087ff--ff0000' telnet`
"""

import sys
import os

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')

import rlwrapfilter
import re

rampsize = 0xe7 - 0x10; # works on my gnome terminal

# create table to map rgb to xterm color id, eg,
# rgb2xterm['000000'] == '16', rgb2xterm['00005f'] == '17',...etc
rgb2xterm = {}
xtermcolor = 16
for r in (0, 0x5f, 0x87, 0xaf, 0xdf, 0xff):
    for g in (0, 0x5f, 0x87, 0xaf, 0xdf, 0xff):
        for b in (0, 0x5f, 0x87, 0xaf, 0xdf, 0xff):
            rgb = hex((r<<16) + (g<<8) + b)[2:] # remove the prefix '0x'
            rgb2xterm[rgb] = str(xtermcolor)
            xtermcolor += 1

filter = rlwrapfilter.RlwrapFilter()
name = filter.name
filter.help_text = '\n'.join([
    "Usage: rlwrap -z '$name <colour1>--<colour2> <command>",
    "paint the prompt in colour gradient between colours <colour1> and <colour2>",
    "colour names must be a 6 digit hex code of rgb,",
    "e.g. 00ff00--ff0000"
    ])

m = re.match(r'(.*)--(.*)', sys.argv[1])
colour1 = m.group(1)
colour2 = m.group(2)


def ramp(val1, val2, frac):
    """intermediate value 100*$frac % between $val1 and $val2"""
    val = int(val1 + frac * (val2-val1))

    # round off
    if (0 <= val < (0+0x5f)/2):
        val = 0x0
    elif ((0+0x5f)/2<= val < (0x5f+0x87)/2):
        val = 0x5f
    elif ((0x5f+0x87)/2<= val < (0x87+0xaf)/2):
        val = 0x87
    elif ((0x87+0xaf)/2<= val < (0xaf+0xdf)/2):
        val = 0xaf
    elif ((0xaf+0xdf)/2<= val < (0xdf+0xff)/2):
        val = 0xdf
    elif ((0xdf+0xff)/2<= val <= 0xff):
        val = 0xff
    return val


def intermediate_color(frac):
    r1 = int(colour1[0:2], 16)
    g1 = int(colour1[2:4], 16)
    b1 = int(colour1[4:6], 16)
    r2 = int(colour2[0:2], 16)
    g2 = int(colour2[2:4], 16)
    b2 = int(colour2[4:6], 16)
    r = ramp(r1, r2, frac)
    g = ramp(g1, g2, frac)
    b = ramp(b1, b2, frac)
    rgb = (r<<16) + (g<<8) + b
    return hex(rgb)[2:] # remove the prefix '0x'


def apply_ramp(text):
    buf = []

    # text -> ['t', 'e', 'x', 't']
    i = 0
    for c in text:
        # ESC[38;5;$colour is the xterm code to swithc to $colour, the \x01 and \x02 are
        # readline "ignore codes" to tell readline that the sequence is unprintable 
        buf.append(
            ''.join([
                "\x01\x1b[38;5;{0}m\x02".format(rgb2xterm[intermediate_color(i/len(text))]),
                c
            ]))
        i += 1

    return ''.join(buf) + "\x01\x1b[0m\x02"


filter.prompt_handler = apply_ramp

filter.run()
