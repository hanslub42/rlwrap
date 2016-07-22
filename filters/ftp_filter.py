#!/usr/bin/python3

# this filter demonstrates a (rather whacky and extreme) use of 'cloak_and_dagger'
# cloak_and_dagger is used for
#   - finding the working directories (local and remote)
#   - finding the legal ftp commands
#   - completing (local or remote) filenames and directories

"""
A vanilla ftp is not bundled in modern Linux today.
For example, ftp on Fedora 21 is readline-enabled.

$ ldd /usr/bin/ftp 
        linux-vdso.so.1 =>  (0x00007ffd015e7000)
        libreadline.so.6 => /lib64/libreadline.so.6 (0x00007fd213777000)
        libncurses.so.5 => /lib64/libncurses.so.5 (0x00007fd21354f000)
        libtinfo.so.5 => /lib64/libtinfo.so.5 (0x00007fd213324000)
        libc.so.6 => /lib64/libc.so.6 (0x00007fd212f67000)
        libdl.so.2 => /lib64/libdl.so.2 (0x00007fd212d63000)
        /lib64/ld-linux-x86-64.so.2 (0x00007fd2139dd000)

However, the feature can be disabled by '-e' option in Fedora case.
This example was tested on the pair of ftp bundled in RHEL6/7, Fedora21 and vsftpd in RHEL6.
Please see `man ftp` to find out how to disable it, if this filter does not work for you.
"""

import sys
import os

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')

import rlwrapfilter
import re


############################ subroutines ####################################

def listing (dir, where, what):
    dir = '.' if dir == None else dir

    command = '!ls -la {0}|cat'.format(dir) if where == 'local' else 'ls -la'
    lines = re.split(r'\r?\n', filter.cloak_and_dagger(command, ftp_prompt, 2))
    if dir_filename_column[where] == None: # find out which column of listing has the filename
        dotdotline = [l for l in lines if re.search(r'(^|\s+)\.\.(\s|$)', l)][0]; # .. should always be there 
        fields = re.split(r'\s+', dotdotline)
        try:
            dir_filename_column[where] = fields.index("..")
        except ValueError as e:
            rlwrapfilter.rlwrap_error(
                "couldn't determine filename column of $where listing", e)

    pattern = "^d" if what == "directories" else "^-"
    lines = [l for l in lines if re.search(pattern, l)] # makes only sense if there is a column with drwxr-xr-x
    results = [re.split('\s+', l)[dir_filename_column[where]] for l in lines]
    return results


def pwd(where):
    command = "!pwd" if where == "local" else "pwd"
    result = filter.cloak_and_dagger(command, ftp_prompt, 1)
    pattern = "(.*?)\r?\n" if where == "local" else '"(.*?)"'
    m = re.search(pattern, result)
    pwd = m.group(1)
    return pwd


def prompt_handler(prompt):
    if prompt == ftp_prompt:
        if len(ftp_commands) == 0:
            ftp_commands.extend(commands())
    else:
        return prompt
    local = pwd('local')
    remote = pwd('remote')
    local = local.replace(os.environ['HOME'], '~')
    rtext = '(remote: ' + remote + ')' if remote else '(not connected)'
    return '{0} {1}> '.format(local,rtext)


"""
sub test {
  my $listing = join ', ', listing (".", "remote", "directories");
  $filter -> send_output_oob("\n Hier: <$listing>\n");
}

"""

def commands():
    help_text = filter.cloak_and_dagger("help", ftp_prompt, 0.5)
    lines = help_text.split('\n')
    del lines[0:2] # remove the first 2 lines
    commands = [command for command in re.split(r'\s+', ''.join(lines))]
    return commands

def complete_handler(line, prefix, completions):

    nwords = len(re.split('\s+', line))
    if prefix == None:
        nwords += 1 # TAB at start of a new (empty) argument

    if nwords <= 1:
        completions.extend([c for c in ftp_commands if re.search(r'^' + prefix, c)])
        return completions

    command = re.search('\s*(\S+)', line).group(1)
    prefix_match = re.search('((.*)/)?([^/]*)', prefix)
    dir = prefix_match.group(2)
    name_prefix = prefix_match.group(3)
    #dir = '.' if dir == None else dir
    dir = '' if dir == None else dir
    narg = nwords-2
    try:
        completion_types[command][narg*2]
        candidates = listing(dir,
                             completion_types[command][2*narg],
                             completion_types[command][2*narg+1])
        completions.extend([os.path.join(dir, c) for c in candidates if re.search(r'^' + name_prefix, c)])
    except ValueError as e:
        pass
    completions = list(set(completions))
    return completions


if __name__ == '__main__':
    if __file__ == "./doctest":
        import doctest
        doctest.testmod()
        sys.exit(0)


    ftp_prompt = "ftp> "
    ftp_commands = []
    completion_types = {
        'cd'  : ['remote', 'directories'],
        'lcd' : ['local', 'directories'],
        'get' : ['remote','files','local','directories'],
        'put' : ['local', 'files','remote','directories']
    }
    
    
    filter = rlwrapfilter.RlwrapFilter()
    dir_filename_column = {'local':None, 'remote':None}
    #dir_filename_column['local'] = None
    #dir_filename_column['remote'] = None
    
    filter.help_text = '\n'.join([
        "usage: rlwrap -c [-aword:] -z ftp_filter.py ftp (-e) [hostname]",
        "run plain Netkit ftp with completion for commands, local and remote files",
        "(demo filter to show the use of the cloak_and_dagger method)"])
    filter.prompt_handler = prompt_handler
    filter.completion_handler = complete_handler
    filter.cloak_and_dagger_verbose = 0 # set to 1 to spy on cloak_and_dagger dialogue
    
    
    if (not 'RLWRAP_COMMAND_PID' in os.environ) or re.match(r'^ftp', os.environ['RLWRAP_COMMAND_LINE']):
        raise SystemExit("This filter works only with plain vanilla ftp\n")

    filter.run()
