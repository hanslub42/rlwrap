#!/usr/bin/python3

"""
a demo for masking sensitive input in the history

Usage:

rlwrap -H history -f censor_passwords.py sqlplus scott@<hostname>:1521/ORCL
SQL > create user test identified by secret;

"""

import sys
import os

if 'RLWRAP_FILTERDIR' in os.environ:
    sys.path.append(os.environ['RLWRAP_FILTERDIR'])
else:
    sys.path.append('.')

import rlwrapfilter
import re

filter = rlwrapfilter.RlwrapFilter()

filter.help_text = "rlwrap -H history -z censor_passwords.py sqlplus scott@<hostname>:1521/ORCL\n"\
                   +"This filter removes the password from SQL 'identified by' clauses\n"

filter.history_handler = lambda x: re.sub(r'(identified\s+by\s+)(\S+)', r'\1xXxXxXxX', x)

# uncomment the following line for ldapmodify
#filter.history_handler = lambda x: re.sub(r'(userpassword:\s+)(\S+)', r'\1xXxXxXxX', x, flags=re.IGNORECASE)
# Sample usage:
# rlwrap -H history -f censor_passwords.py ldapmodify -D <bindDN> -w <password>
# dn: uid=user1, ou=people, dc=redhat, dc=com
# objectclass: inetOrgPerson
# uid: user3
# cn: user3
# sn: user3
# userpassword: password

filter.run()
