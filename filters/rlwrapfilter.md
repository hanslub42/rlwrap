Python library for [rlwrap](https://github.com/hanslub42/rlwrap) filters
==========================================================================

Synopsis
-------------------
```
    import os
    import sys
    if 'RLWRAP_FILTERDIR' in os.environ:
        sys.path.append(os.environ['RLWRAP_FILTERDIR'])
    else:
        sys.path.append('.')
    import rlwrapfilter
    filter = rlwrapfilter.RlwrapFilter(message_handler=do_something)
    filter.help_text = 'useful help'
    filter.output_handler = lambda x: re.sub('apple', 'orange', x) # re−write output
    filter.prompt_handler = munge_prompt
    filter.completion_handler = complete_handler
    filter.history_handler = lambda x: re.sub(r'(identified\s+by\s+)(\S+)', r'\\1xXxXxXxX', x)
    filter.run()
```


Description
-------------------
This is an [RlwrapFilter](https://github.com/hanslub42/rlwrap/wiki/RlwrapFilter.pm-manpage)
clone written in Python. The focus is on providing the same API's
and usage of the original Perl version [RlwrapFilter](https://github.com/hanslub42/rlwrap/wiki/RlwrapFilter.pm-manpage)
as possible.


Module contents
-------------------
The module defines a class, several functions, variables, constants, and an exception.

### class

rlwrapfilter.**RlwrapFilter**

```
f = rlwrapfilter.RlwrapFilter()

f = rlwrapfilter.RlwrapFilter(prompt_handler = lambda x: "Hi! > ", minimal_rlwrap_version = 0.35, ...)
```

returns a new RlwrapFilter object.

### exception

rlwrapfilter.**RlwrapFilterError**


RlwrapFilter object
-------------------


### handlers

Handler is an attribute to store a callable object that get called from `run` method.

##### prompt_handler

handles a prompt. It should accept one string argument and return a string as a prompt.

##### completion_handler

handles a completion list. It should accept `line`, `prefix`, `completions`:

|arguments|type|description|
|---------|----|-----------|
|`line`|string|string itself you entered to a prompt|
|`prefix`|string|prefix for completion. Generally, a last part of `line`.|
|`comletions`|list|list of completion candidates|

and return a handled list of completion candidates.

##### history_handler

handles a history. It should accept one string argument and return a string as a history.

##### input_handler

handles a input you entered to a prompt. It should accept one string argument and return a string as a input.

##### echo_handler

handles an echo (a string which echo-back to your screen as you entered something to a prompt). It should accept one string argument and return a string as an echo.

##### output_handler

handles a output which rlwrapped command returns. It should accept one string argument and return a string as a output.

##### message_handler

handles a message, a piece of communications which come and go between rlwrap filter and rlwrap. It should accept one string argument and one integer argument(tag) and return a string as a message.


### other attributes

[RlwrapFilter](#class) instances have the following attribute:

##### help_text

An attribute to store a useful help text. It is shown by `rlwrap -z`.

##### minimal_rlwrap_version

stores a minimal required version for your rlwrap filter. As you set a float value, it immediately warns unless the version of rlwrap is higher than or quals to the value.

##### cumulative_output

stores the cumulative output. It is immutable.

##### previous_tag

stores the previous tag. It is immutable.

##### prompt_rejected

stores `_THIS_CANNOT_BE_A_PROMPT_`. Immutable.

##### command_line

stores the rlwrapped command and its arguments. Immutable.

##### running_under_rlwrap

True if a filter is running under [rlwrap](https://github.com/hanslub42/rlwrap). Otherwise, False. The default value is False.

##### cloak_and_dagger_verbose

The verbosity of the `cloak_and_dagger` method. Boolean. The default value is False.

##### prompts_are_never_empty

If True, it rejects an empty prompt. The default value is False.


### methods

[RlwrapFilter](#class) instances have the following methods:

##### send_output_oob(text)

sends text to rlwrap to show text.

##### send_ignore_oob(text)

sends text to rlwrap, but rlwrap discards it. It is useful for debugging purpose.

##### add_to_completion_list(words)
##### remove_from_completion_list(words)

adds/removes the words in `words` to/from a list of complition candidates.

##### cloak_and_dagger(question, prompt, timeout)

sends `question` to command's input and read back everything that comes back until `prompt` is seen at "end-of-chunk", or no new chunks arrive for `timeout` seconds, whichever comes first. Return the response (without the final $prompt). rlwrap remains completely unaware of this conversation.

##### run()

starts an event loop.

##### cwd()

returns os.getcwd()


Install
-------------------
```
$ git clone https://github.com/hokuda/rlwrapfilter.py.git
$ cd rlwrapfilter.py
$ sudo python3 ./setup.py install
```


Acknowledgement
-------------------
Many thanks to [Hans Lub](https://github.com/hanslub42) for creating [the great tool](https://github.com/hanslub42/rlwrap).
