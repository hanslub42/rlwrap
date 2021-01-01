#!/usr/bin/env python3

"""handle hotkeys"""

import subprocess
import sys
import os
import tempfile

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')

import rlwrapfilter


# A hotkey handler is called with five parameters:
#   1: the key (e.g. 14 for CTRL+M)
#   2: the prefix, i.e. the input line up to (but not including) the cursor
#   3: the postfix: the rest of the input line (without a concluding newline, of course)
#   4: the whole history (all lines, oldest first, interspersed with newlines: "line 1\nline2\n ...line N")
#   5: the history position at the moment of the hotkey press (oldest line = 0)
#
# If the hotkey was bound to "rlwrap-hotkey-without-history" the last two parameters will be empty and can be ignored
# The return value is a similar list (where all values may be changed: the input line could be re-written,
# the history revised, etc. The first parameter makes no sense as a return value: if it is != $key
# it will pe put in the "echo area". If the key was bound to rlwrap-hotkey-without-history the last two elements of the 
# returned list are ignored.
#
# If the postfix is returned with a concluding newline, the resulting input line is accepted immediately, otherwise
# it is put in readlines input buffer again, with the cursor at the beginning of the returned postfix 
#
# Summary: ($echo, $prefix, $postfix, $history, $histpos) = handler($key, $prefix, $postfix, $history, $histpos)


# generic hotkey handler, that dispatches on the value of $key (using the hash %$keymap defined at the top of this file

@rlwrapfilter.intercept_error
def hotkey(key, *other_params): # key = e.g.  "<CTRL-Y>"
    ukey = uncontrol(key)
    if ukey not in keymap:
        return (key, *other_params) # a filter further downstream may want to handle this hotkey
    handler = keymap[ukey]
    result = handler(0, *other_params)
    return result


############################# A few handlers ###############################################
#
# After dispatch (on the value of $key) by the hotkey() function the value of $key is not relevant anymore.
# its place is now taken by a parameter $doc :
#
# ($echo, $prefix, $postfix, $history, $histpos) = myfunc(0,  $prefix, $postfix, $history, $histpos)
# "docstring"                                    = myfunc(1, @not_interesting)



def yank_clipboard(doc, prefix, postfix, *boring):
    if doc:
        return "insert from clipboard"
    selection = safe_backtick(qw('xsel -o'))
    return ("", prefix + selection, postfix, *boring)


def date_in_echo_area(doc, *boring):
    if doc:
        return "show current time in echo area"
    date = safe_backtick(qw('date +%H:%M'))
    return ("({0}) ".format(date), *boring)


def edit_history(doc, prefix, postfix, history, histpos):
    if doc:
        return "edit current history"
    if not histpos.isdigit():
        syst.exit("$histpos is not a number - did you bind this key to 'rlwrap-hotkey-without-history'?")
    editfile = tempfile.NamedTemporaryFile()
    editfilename = editfile.name
    lineno = int(histpos) + 1
    colno = len(prefix) + 1
    editfile.write(history.encode(sys.stdin.encoding))
    editfile.flush()
    if 'RLWRAP_EDITOR' in os.environ:
        editor = os.environ['RLWRAP_EDITOR']
    else:
        editor = "vi +%L"
    editor = editor.replace('%L', str(lineno))
    editor = editor.replace('%C', str(colno))
    os.system(editor + ' ' + editfilename)
    editfile.seek(0,0)
    lines = map(lambda l: l.decode(sys.stdin.encoding), editfile.readlines())
    new_history = []
    counter = 0
    empty_counter = 0
    last_counter = 0
    last_empty_counter = 0
    for line in lines:
        line = line.replace('\t', '')
        line = line.lstrip()
        line = line.rstrip()
        if not line == '':
            if (empty_counter > 0):
                # remember position of last line after an empty line,
                # and the number of empty lines:
                (last_counter, last_empty_counter) = (counter, empty_counter)
            empty_counter = 0;
            counter = counter + 1 # We count 0-based, so increment only now
            new_history.append(line)
        else:
            empty_counter = empty_counter + 1
    if last_empty_counter > 0:
        histpos = str(last_counter)
        prefix = new_history[last_counter]
        postfix = ""
    return ("", prefix, postfix, '\n'.join(new_history), histpos)




def fuzzy_filter_history(doc, prefix, postfix, history, histpos, command):
    '''filter history through command (either 'peco' or the very similar  'fzf') '''
    if doc:
        return "{} current history".format(command)
    command_line = [command, '--select-1', '--query',  prefix ]
    select_1 = ''
    with subprocess.Popen(command_line
                          ,stdin=subprocess.PIPE
                          ,stdout=subprocess.PIPE
                          ,universal_newlines=True) as p:
        (select_1, error) = p.communicate(input=history)
    select_1 = select_1.rstrip()
    return ("", select_1, postfix, history, histpos)

def peco_history(doc, prefix, postfix, history, histpos):
    return fuzzy_filter_history(doc, prefix, postfix, history, histpos, 'peco')

def fzf_history(doc, prefix, postfix, history, histpos):
    return fuzzy_filter_history(doc, prefix, postfix, history, histpos, 'fzf')


# change the table below if you like, but don't forget to bind the corresponding keys  to 'rlwrap-hotkey' in your .inputrc, or the hotkeys won't work!

keymap = {
    "y" : yank_clipboard,
    "n" : edit_history,
    "p" : peco_history,
    "f" : fzf_history,
    "r" : peco_history,
    "t" : date_in_echo_area
}

############################## helper functions #########################################################

def qw(s):
#    return tuple(s.split())
    return s.split()


def document_all_hotkeys():
    doclist = ''
    dontcare = (None, None, None, None) # dummy arguments for getting the docstring
    for k in 'abcdefghijklmnopqrstuvwxyz':
        try:
            handler = keymap[k]
            if (handler):
                doclist = doclist + "CTRL+{0}:   ".format(k) + handler(1, *dontcare) + "\n"
        except:
            
            pass
    inputrc = "{0}/.inputrc".format(os.environ['HOME'])
    doclist = doclist + "Currently bound hotkeys in .inputrc:\n"
    doclist = doclist + safe_backtick(["grep", "rlwrap-hotkey", inputrc])
    return doclist


def safe_backtick(command_args):
    with subprocess.Popen(command_args, stdout=subprocess.PIPE, universal_newlines=True) as p:
        #result = map(lambda b: b.decode("utf-8"), p.stdout.readlines())
        (result, error) = p.communicate()
    return ''.join(result).rstrip()


# give back corresponding CTRL-key. E.g: control("m") = "\0x13"
def uncontrol(key):
    return chr(ord(key) + 64).lower()


############################################ The Filter #################################################

if __name__ == '__main__':
    rlwrap_filter = rlwrapfilter.RlwrapFilter()
    name = rlwrap_filter.name
    rlwrap_filter.help_text = '\n'.join([
        "Usage: rlwrap -z {0} <command>\n".format(name),
        "handle hotkeys (but only if bound to 'rlwrap-hotkey' in your .inputrc):\n",
        document_all_hotkeys()
    ])

    rlwrap_filter.hotkey_handler = hotkey

    rlwrap_filter.run()


