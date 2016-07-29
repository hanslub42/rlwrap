#!/usr/bin/python3
# -*- coding: utf-8 -*-

"""
Python 3 library for [rlwrap](https://github.com/hanslub42/rlwrap) filters

* Synopsis

    filter = rlwrapfilter.RlwrapFilter(message_handler=do_something)
    filter.help_text = 'useful help'
    filter.output_handler = lambda x: re.sub('apple', 'orange', x) # re−write output
    filter.prompt_handler = munge_prompt
    filter.completion_handler = complete_handler
    filter.history_handler = lambda x: re.sub(r'(identified\s+by\s+)(\S+)', r'\\1xXxXxXxX', x)
    filter.run()

This is an [RlwrapFilter](http://utopia.knoware.nl/~hlub/uck/rlwrap/RlwrapFilter.html)
clone written in Python 3. The focus is on providing the same API's
and usage of the Perl version of [RlwrapFilter](http://utopia.knoware.nl/~hlub/uck/rlwrap/RlwrapFilter.html)
as possible.

[rlwrap](http://utopia.knoware.nl/~hlub/uck/rlwrap) is a tiny
utility that sits between the user and any console command, in order
to bestow readline capabilities (line editing, history recall) to
commands that don't have them.

Since version 0.32, rlwrap can use filters to script almost every
aspect of rlwrap's interaction with the user: changing the history,
re-writing output and input, calling a pager or computing completion
word lists from the current input.

rlwrapfilter.py makes it very simple to write rlwrap
filters in Python 3. A filter only needs to instantiate a RlwrapFilter
object, change a few of its default handlers and then call its 'run'
method.
"""


import sys
import os
import io
import types
import struct
import select
import re
import traceback
import binascii
import collections
import numbers

TAG_INPUT                       = 0
TAG_OUTPUT                      = 1
TAG_HISTORY                     = 2
TAG_COMPLETION                  = 3
TAG_PROMPT                      = 4
TAG_HOTKEY                      = 5
TAG_IGNORE                      = 251
TAG_ADD_TO_COMPLETION_LIST      = 252
TAG_REMOVE_FROM_COMPLETION_LIST = 253
TAG_OUTPUT_OUT_OF_BAND          = 254
TAG_ERROR                       = 255


REJECT_PROMPT = '_THIS_CANNOT_BE_A_PROMPT_'


# we want to behave differently when running outside rlwrap
we_are_running_under_rlwrap = 'RLWRAP_COMMAND_PID' in os.environ

# rlwrap version
rlwrap_version = float(os.environ['RLWRAP_VERSION']) if 'RLWRAP_VERSION' in os.environ else 0.41

# open communication lines with rlwrap (or with the terminal when not running under rlwrap)
if (we_are_running_under_rlwrap):
    CMD_IN = int(os.environ['RLWRAP_MASTER_PTY_FD'])
    CMD_OUT = int(os.environ['RLWRAP_MASTER_PTY_FD'])

    FILTER_IN = int(os.environ['RLWRAP_INPUT_PIPE_FD'])
    FILTER_OUT = int(os.environ['RLWRAP_OUTPUT_PIPE_FD'])
else:
    CMD_IN = sys.stdout.fileno()
    CMD_OUT = sys.stdin.fileno()

    FILTER_IN = sys.stdout.fileno()
    FILTER_OUT = sys.stdin.fileno()



def when_defined(maybe_ref_to_sub, *args):
    """
    when_defined(f, x, y, ...) returns f(x, y, ...) if f is defined, x otherwise
    """
    if (maybe_ref_to_sub is not None):
        try:
            return maybe_ref_to_sub(*args)
        except Exception as e:
            send_error(
                "improper handler <{0}> of type {1} (expected a ref to a sub)\n{2}"
                .format(maybe_ref_to_sub, type(maybe_ref_to_sub), traceback.format_exc()),e)
    else:
        return args[0]


def out_of_band(tag):
    return tag > 128


def read_until(fh, stoptext, timeout):
    """
    read chunks from pty pointed to by fh until either inactive for timeout or stoptext is seen at end-of-chunk
    """
    res = ''
    while (True):
        chunk = read_chunk(fh, timeout);
        if(not chunk):
            # got "" back: timeout
            #send_warn("read_until: timeout")
            return res
        res = res + chunk
        # multi-line mode so that "^" matches a head of each line
        if re.search(stoptext, res, re.MULTILINE):
            return res


def read_chunk(fh, timeout):
    """
    read chunk from pty pointed to by fh with timeout if timed out, returns 0-length string
    """
    if (len(select.select([fh], [], [], timeout)[0]) > 0):
        chunk = os.read(fh, 2**16); # read up-to 2^16=65536 bytes
        return chunk.decode(sys.stdin.encoding, errors="ignore")
    return ""


def read_patiently(fh, count):
    """
    keep reading until count total bytes were read from filehandle fh
    """
    already_read = 0
    buf = bytearray()
    while(already_read < count):
        buf += os.read(fh, count-already_read)
        nread = len(buf)
        if (nread == 0):
            break
        already_read += nread
    return buf


def write_patiently(fh, buffer):
    """
    keep writing until all bytes from $buffer were written to $fh
    """
    already_written = 0
    count = len(buffer)
    while(already_written < count):
        nwritten = os.write(fh, buffer[already_written:])
        if (nwritten <= 0):
            send_error("error writing: {0}".format(str(buffer)))
        already_written = already_written + nwritten


def read_message():
    """
    read message (tag, length word and contents) from FILTER_IN
    """
    if not we_are_running_under_rlwrap:
        return read_from_stdin()

    tag = int.from_bytes(read_patiently(FILTER_IN,1), sys.byteorder)
    length = int.from_bytes(read_patiently(FILTER_IN,4), sys.byteorder)
    message = read_patiently(FILTER_IN, length).decode(sys.stdin.encoding, errors = "ignore")
    # \Z matches only at the end of the string in python
    message = re.sub(r'\n\Z', '', str(message or ""))
    return tag, message


def write_message(tag, message):
    if (not we_are_running_under_rlwrap):
        return write_to_stdout(tag, message)

    message = '\n' if message is None else message + '\n'  # allow undefined message
    bmessage = bytearray(message, sys.stdin.encoding)
    length = len(bmessage)

    write_patiently(FILTER_OUT, tag.to_bytes(1, sys.byteorder, signed=False))
    write_patiently(FILTER_OUT, length.to_bytes(4, sys.byteorder, signed=False))
    write_patiently(FILTER_OUT, bmessage)


def read_from_stdin():
    tag = None
    prompt = None
    tagname = None
    while (not tag):
        m = re.match("(\S+) (.*?)\r?\n", sys.stdin.readline())
        if not m:
            sys.exit()
        tagname, message = m.groups()
        tag = name2tag(tagname)
    return tag, message


def tag2name(tag):
    """
    Convert the tag (an integer) to its name (e.g. " TAG_PROMPT ")
    """
    for name in ['TAG_REMOVE_FROM_COMPLETION_LIST',
                 'TAG_ADD_TO_COMPLETION_LIST',
                 'TAG_INPUT',
                 'TAG_PROMPT',
                 'TAG_COMPLETION',
                 'TAG_HOTKEY',
                 'TAG_HISTORY',
                 'TAG_OUTPUT_OUT_OF_BAND',
                 'TAG_ERROR',
                 'TAG_IGNORE',
                 'TAG_OUTPUT']:
        if (eval('{0} == {1}'.format(str(tag), name))):
            return name



def write_to_stdout(tag, message):
    print('{0}: {1}'.format(tag2name(tag), message))


def send_warn(message):
    """
    send message to rlwrap.
    """
    write_message(TAG_OUTPUT_OUT_OF_BAND, "{0}: {1}".format(__name__, message))


def send_error(message,e):
    """
    send message to rlwrap, and raise e.
    """
    write_message(TAG_OUTPUT_OUT_OF_BAND, "{0}: {1}".format(__name__, message))
    raise e


def intercept_error(func):
    """
    A decorator to intercept an exception, send the message to rlwrap, and raise an exception.
    """
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except Exception as e:
            write_message(
                TAG_ERROR,
                "{0}: {1}".format(__name__, '/'.join(map(str,e.args)))
            )
            raise e
    return wrapper


def is_string(value):
    """
    A decorator to intercept an exception, send the message to rlwrap, and raise an exception.
    """
    return isinstance(value, str) or value == None


def is_boolean(value):
    """
    A decorator to intercept an exception, send the message to rlwrap, and raise an exception.
    """
    return isinstance(value, bool) or value == None


def is_integer(value):
    """
    A decorator to intercept an exception, send the message to rlwrap, and raise an exception.
    """
    return isinstance(value, int) or value == None


def is_float(value):
    """
    A decorator to intercept an exception, send the message to rlwrap, and raise an exception.
    """
    return isinstance(value, numbers.Number) or value == None


def is_callable(value):
    """
    A decorator to intercept an exception, send the message to rlwrap, and raise an exception.
    """
    return isinstance(value, collections.Callable) or value == None


@intercept_error
def test_intercept():
    print('test intercept!!! + + + +')
    raise Exception('test exception = = = = = .........')


class RlwrapFilterError(Exception):
    """
    A custom exception for rlwrap
    """
    def __init__(self, *args):
        super().__init__(args)


class RlwrapFilter:
    """
    class for rlwrap filters
    """

    def __setattr__(self, name, value):
        if not name in self._fields:
            self.warn("There is no '{0}' attribute in class {1}\n".format(name, self.__class__.__name__))

        is_valid_type = self._field_types[name]
        if not is_valid_type(value):
            self.warn("{0} should not be '{1}'\n".format(name, type(value)))

        if name == 'minimal_rlwrap_version' and (value > rlwrap_version):
            self.warn("requires rlwrap version {0} or newer.\n".format(str(value)))

        self.__dict__[name] = value


    """
    def __getattr__(self, name):
        if(name in self.fields):
            return self.__dict__[name]
        elif(name in self.handlers):
            return self.__dict__[name]
        else:
            send_error("There is no '{0}' attribute in class {1}"
                       .format(name, self.__class__.__name__))
    """


    def __init__(self, **kwargs):
        self.__dict__['_field_types'] = {
            'input_handler':is_callable,
            'output_handler':is_callable,
            'prompt_handler':is_callable,
            'hotkey_handler':is_callable,
            'echo_handler':is_callable,
            'message_handler':is_callable,
            'history_handler':is_callable,
            'completion_handler':is_callable,
            'help_text':is_string,
            'cloak_and_dagger_verbose':is_boolean,
            'cumulative_output':is_string,
            'prompts_are_never_empty':is_boolean,
            'previous_tag':is_integer,
            'previous_message':is_string,
            'echo_has_been_handled':is_boolean,
            'saved_output':is_string,
            'prompt_rejected':is_string,
            'command_line':is_string,
            'running_under_rlwrap':is_boolean,
            'minimal_rlwrap_version':is_float,
            'name':is_string
        }

        self.__dict__['_fields'] = self.__dict__['_field_types'].keys()

        for field in self._fields:
            self.__dict__[field] = None

        self.previous_tag = -1
        self.echo_has_been_handled = False
        self.saved_output = ''
        self.cumulative_output = ''
        self.minimal_rlwrap_version = rlwrap_version
        self.command_line = os.environ['RLWRAP_COMMAND_LINE'] if 'RLWRAP_COMMAND_LINE' in os.environ else None
        self.running_under_rlwrap = 'RLWRAP_COMMAND_PID' in os.environ
        self.name = os.path.basename(sys.argv[0])

        for key in kwargs:
            exec('self.{0} = kwargs[key]'.format(key))



    def handle_output(self, message):
        """
        split output in echo and the rest and call the appropriate handlers on them
        """
        (handled_echo, nl) = ('', '')
        if (self.previous_tag is not None and self.previous_tag == TAG_INPUT):
            self.cumulative_output = ""
            self.echo_has_been_handled = False

        if (not self.echo_has_been_handled):
            if (re.match(r'\n', message)):
                # save all output until we have one *whole* echo line
                self.saved_output = self.saved_output + message
                return ""
            else:
                # ... then process it
                message = self.saved_output + message
                self.echo_has_been_handled = True
                self.saved_output = ""
                (echo, nl, message) = re.match(r'^([^\n\r]*)(\r?\n)?(.*)?', message, re.DOTALL).groups()
                handled_echo = when_defined(self.echo_handler, echo)
        self.cumulative_output = self.cumulative_output + message
        return handled_echo + str(nl or "") + (when_defined(self.output_handler, message))


    def add_to_completion_list(self, *args):
        write_message(TAG_ADD_TO_COMPLETION_LIST, ' '.join(args))


    def remove_from_completion_list(self, *args):
        write_message(TAG_REMOVE_FROM_COMPLETION_LIST, ' '.join(args))


    def cloak_and_dagger(self, question, prompt, timeout):
        """
        have a private chat with the rlwrapped command. This relies very much om the assumption that command stops. 
        talking, and only listens, when it has displayed the prompt
        """
        write_patiently(bytearray(question + "\n", sys.stdin.encoding))
        if (self.cloak_and_dagger_verbose):
            self.send_output_oob("cloak_and_dagger question: {0}\n".format(question))
        response = read_until(CMD_OUT, prompt, timeout)
        response = re.sub('^.*?\n', '', response) # chop off echoed question;
        response = re.sub('{0}$'.format(prompt), '', response) # chop off prompt;
        if (self.cloak_and_dagger_verbose):
            self.send_output_oob("cloak_and_dagger response: {0}\n".format(response))
        return response


    def vacuum_stale_message(self, prompt, timeout):
        """
        Some command returns messages asynchronously
        and tends to delay message when invoking multiple `cloak_and_dagger`.
        You may want to drop message at such time.

        rlwrap_filter.cloak_and_dagger(command1, prompt, timeout)
        rlwrap_filter.cloak_and_dagger(command2, prompt, timeout)
        ...
        time.sleep(1)
        rlwrap_filter.vacuum_stale_message(prompt, timeout)
        """
        response = read_until(CMD_OUT, prompt, timeout)
        return response


    def name2tag(self, name):
        """
        Convert a valid tag name like " TAG_PROMPT " to a tag (an integer)
        """
        try:
            tag = eval(name)
        except Exception as e:
            raise SystemExit('unknown tagname {0}'.format(name))
        return tag


    def tag2name(self, tag):
        """
        Convert the tag (an integer) to its name (e.g. " TAG_PROMPT ")
        """
        return tag2name(tag)

    def warn(self, message):
        """
        send message to rlwrap.
        """
        send_warn(message)


    def error(self, message,e):
        """
        send message to rlwrap, and raise e.
        """
        send_error(message, e)


    def send_output_oob(self, text):
        write_message(TAG_OUTPUT_OUT_OF_BAND, text)


    def send_ignore_oob(self, text):
        write_message(TAG_IGNORE, text)


    def cwd(self):
        return os.getcwd()

    def run(self):
        """
        event loop
        """

        # $RLWRAP_COMMAND_PID can be undefined (e.g. when run interactively, or under rlwrap -z listing
        # or == "0" (when rlwrap is called without a command name, like in rlwrap -z filter.py)
        # In both cases: print help text
        if 'RLWRAP_COMMAND_PID' not in os.environ or os.environ['RLWRAP_COMMAND_PID'] == "0":
            write_message(TAG_OUTPUT_OUT_OF_BAND, self.help_text + '\n')

        while(True):
            tag, message = read_message()

            message = when_defined(self.message_handler, message, tag) # ignore return value

            if (tag == TAG_INPUT):
                response = when_defined(self.input_handler, message)
            elif (tag == TAG_OUTPUT):
                response = self.handle_output(message)
            elif (tag == TAG_HISTORY):
                response = when_defined(self.history_handler, message)
            elif (tag == TAG_COMPLETION):
                if (self.completion_handler is not None):
                    message = re.sub('[ ]+$', '', message) # eat final space
                    (line, prefix, completions) = message.split('\t')
                    completions = completions.split(' ')
                    completions = self.completion_handler(line, prefix, completions)
                    response = "{0}\t{1}\t".format(line, prefix) + ' '.join(completions) + ' '
                else:
                    response = message
            elif (tag == TAG_HOTKEY):
                if (self.hotkey_handler is not None):
                    params = message.split("\t")
                    result = self.hotkey_handler(*params)
                    response = "\t".join(result)
                else:
                    response = message
            elif (tag == TAG_PROMPT):
                if (message == REJECT_PROMPT or
                    (self.prompts_are_never_empty is not None and message == '')):
                    write_message(tag,REJECT_PROMPT);
                    # don't update <previous_tag> and don't reset <cumulative_input>
                    next
                if (os.environ['RLWRAP_IMPATIENT'] and not re.match('\n$', self.cumulative_output)):
                    # cumulative output contains prompt: chop it off!
                    # s/[^\n]*$// takes way too long on big strings,
                    # what is the optimal regex to do this?
                    self.cumulative_output = re.sub('(?<![^\n])[^\n]*$', '', self.cumulative_output)

                response = when_defined(self.prompt_handler, message)
                if (re.match('\n', response)):
                    send_error('prompts may not contain newlines!')
            else:
                # No error message, compatible with future rlwrap
                # versions that may define new tag types
                response = message

            if (not (out_of_band(tag) and (tag == TAG_PROMPT and response == REJECT_PROMPT))):
                self.previous_tag = tag
                self.previous_message = message

            write_message(tag, response)




if __name__ == '__main__':
    import doctest
    doctest.testmod()
