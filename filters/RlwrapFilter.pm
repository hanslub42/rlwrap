package RlwrapFilter;

require 5.006;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD);

sub when_defined($@);
my $previous_tag = -1;
my $echo_has_been_handled = 0;
my $saved_output = "";


require Exporter;
require AutoLoader;
@ISA = qw(Exporter AutoLoader);

@EXPORT = qw(TAG_INPUT TAG_OUTPUT TAG_HISTORY TAG_COMPLETION TAG_PROMPT);
$VERSION = '0.01';

use Carp;

# constants for every tag we know about
use constant TAG_INPUT                         => 0;
use constant TAG_OUTPUT                        => 1;
use constant TAG_HISTORY                       => 2;
use constant TAG_COMPLETION                    => 3;
use constant TAG_PROMPT                        => 4;
use constant TAG_HOTKEY                        => 5;
use constant TAG_IGNORE                        => 251;
use constant TAG_ADD_TO_COMPLETION_LIST        => 252;
use constant TAG_REMOVE_FROM_COMPLETION_LIST   => 253;
use constant TAG_OUTPUT_OUT_OF_BAND            => 254;
use constant TAG_ERROR                         => 255;

use constant REJECT_PROMPT                    => "_THIS_CANNOT_BE_A_PROMPT_";

# we want to behave differently when running outside rlwrap
my $we_are_running_under_rlwrap = defined $ENV{RLWRAP_COMMAND_PID};


# die() and warn() must communicate via rlwrap, not via STDERR (unless we're running under perl -c)
unless ($^C){
  $SIG{__DIE__}  = \&die_with_error_message;
  $SIG{__WARN__} = \&warn_with_info_message;
}

# automagically have a setter/getter for every key of %$self
sub AUTOLOAD {
  my $self = shift;
  my $type = ref($self)
    or croak "$self is not an object";

  my $name = $AUTOLOAD;
  $name =~ s/.*://;             # strip fully-qualified portion

  unless (exists $self->{$name} ) {
    croak "There is no `$name' setter/getter in class $type";
  }

  if (@_) {
    return $self->{$name} = shift;
  } else {
    return $self->{$name};
  }
}

# open communication lines with rlwrap (or with the terminal when not running under rlwrap)
if ($we_are_running_under_rlwrap) {

  open CMD_IN,     ">&" . $ENV{RLWRAP_MASTER_PTY_FD};
  open CMD_OUT,    "<&" . $ENV{RLWRAP_MASTER_PTY_FD};

  open FILTER_IN,  "<&" . $ENV{RLWRAP_INPUT_PIPE_FD};
  open FILTER_OUT, ">&" . $ENV{RLWRAP_OUTPUT_PIPE_FD};
} else {
  open CMD_IN,     ">&STDOUT";
  open CMD_OUT,    "<&STDIN";

  open FILTER_IN,  "<&STDIN";
  open FILTER_OUT,  ">&STDOUT";
}


# create filter object
sub new {
  my ($this, %init) = @_;
  my $class = ref($this) || $this;
  my $self = {};
  my @accessors = qw(initialiser help_text input_handler
		   output_handler prompt_handler echo_handler
		   message_handler history_handler hotkey_handler completion_handler
		   echo_handler message_handler cloak_and_dagger_verbose
		   cumulative_output prompts_are_never_empty
		   minimal_rlwrap_version);
  foreach my $acc (@accessors) {
    $self->{$acc} = "";
  }
  bless $self, $class;
  foreach my $key (keys %init) {
    croak "There is no `$key' attribute in class $class" unless defined $self->{$key};
    $self -> {$key} = $init{$key};
    $self -> minimal_rlwrap_version($self->{$key}) if $key eq "minimal_rlwrap_version";
  }
  return $self;
}

# event loop
sub run {
    my ($self) = @_;

    if($ENV{RLWRAP_COMMAND_PID} == 0) { # when called as rlwrap -z <filter> (with no command) ..
      write_message(TAG_OUTPUT_OUT_OF_BAND, $self -> help_text . "\n"); # ... send help text
    }

    while(1) {
	my ($tag, $message) = read_message();

        $message = when_defined $self -> message_handler, "$message", $tag; # ignore return value
	my $response;

	if ($tag == TAG_INPUT) {
	  $response = when_defined $self -> input_handler, "$message";
	} elsif ($tag == TAG_OUTPUT) {
	  $response = $self -> handle_output($message);
	} elsif ($tag == TAG_HISTORY) {
	  $response = when_defined $self -> history_handler, "$message";
        } elsif ($tag == TAG_HOTKEY) {
          if ($self -> hotkey_handler) {
            my @params = split /\t/, $message;
            my @result = &{$self -> hotkey_handler}(@params);
            $response = join("\t", @result);
          } else {
            $response = $message;
          }
	} elsif ($tag == TAG_COMPLETION) {
	  my ($line, $prefix, $completions, @completions);
	  if ($self -> completion_handler) {
	    $message =~ s/[ ]+$//; # eat final space
	    ($line, $prefix, $completions) = split /\t/, $message;
	    @completions = split / /, $completions;
	    @completions = &{$self -> completion_handler}($line, $prefix, @completions);
	    $response = "$line\t$prefix\t". (join ' ', @completions) . " ";
	  } else {
	    $response = $message;
	  }
	} elsif ($tag == TAG_PROMPT) {
	  if ($message eq REJECT_PROMPT or
	      ($self -> {prompts_are_never_empty} and $message eq "")) {
			write_message($tag,REJECT_PROMPT);
			# don't update <previous_tag> and don't reset <cumulative_input>
			next;
		}

          if ($ENV{RLWRAP_IMPATIENT} and  $self->{cumulative_output} !~ /\n$/) { # cumulative output contains prompt: chop it off!
            $self->{cumulative_output} =~ s/(?<![^\n])[^\n]*$//                  # s/[^\n]*$// takes way too long on big strings,
                                                                                 # what is the optimal regex to do this?
          }

	  $response = when_defined $self -> prompt_handler, "$message";
	  croak "prompts may not contain newlines!" if $response =~ /\n/;
	} else {
	  $response = $message; # No error message, compatible with future rlwrap
	                        # versions that may define new tag types
	}

	unless (out_of_band($tag) and ($tag == TAG_PROMPT and $response eq REJECT_PROMPT)) {
          $self -> {previous_tag} = $tag;
          $self -> {previous_message} = $message;
        }
	write_message($tag, $response);

    }
}


# when_defined \&f, x, y, ... returns f(x, y, ...) if f is defined, x otherwise
sub when_defined($@) {
  my $maybe_ref_to_sub = shift;
  local $_ =  $_[0] ; # convenient when using anonymous subs as handlers: $filter -> blah_handler(sub{$_ if /blah/});
  if ($maybe_ref_to_sub) {
    if ((my $type  = ref($maybe_ref_to_sub))  ne 'CODE') {
      croak "improper handler <$maybe_ref_to_sub> of type $type (expected a ref to a sub)";
    }
    return &{$maybe_ref_to_sub}(@_);
  } else {
    return $_;
  }
}

sub out_of_band {
  my($tag) = @_;
  return $tag > 128;
}






# split output in echo and the rest and call the appropriate handlers on them
sub handle_output {
  my ($self, $message) = @_;
  my ($echo, $handled_echo, $nl);
  if (defined $self -> {previous_tag} and $self -> {previous_tag} == TAG_INPUT) {
    $self->{cumulative_output} = "";
    $echo_has_been_handled = 0;
  }

  if (not $echo_has_been_handled) {
    if ($message !~ /\n/) {
      $saved_output .= $message; # save all output until we have one *whole* echo line
      return "";
    } else {                    # ... then process it
      $message = $saved_output . $message;
      $echo_has_been_handled = 1;
      $saved_output = "";
      ($echo, $nl, $message) = ($message =~ /^([^\n\r]*)(\r?\n)?(.*)?/s); #@@@ This doesn't work for multi-line input!
      $handled_echo = when_defined $self -> echo_handler, "$echo";
    }
  }
  $self->{cumulative_output} .= $message;
  return $handled_echo . $nl . (when_defined $self -> output_handler, "$message");
}


sub read_until { # read chunks from pty pointed to by $fh until either inactive for $timeout or
                 # $stoptext is seen at end-of-chunk
  my ($fh, $stoptext, $timeout) = @_;
  my ($res);
  while (1){
    my $chunk = read_chunk($fh, $timeout);
    return $res unless $chunk; # got "" back: timeout
    $res .= $chunk;
    return $res if $res =~ /$stoptext$/;
  }
}


# read chunk from pty pointed to by $fh with timeout $timeout
sub read_chunk {
  my ($fh, $timeout) = @_;
  my ($rin, $rout, $chunk);
  vec($rin, fileno($fh), 1) = 1;
  my ($nfound, undef) = select($rout=$rin, undef, undef, $timeout);
  if ($nfound > 0) {
    my $nread = sysread($fh, $chunk, 256);
    if ($nread > 0) {
      return $chunk;
    }
  }
  return "";
}


# keep reading until $count total bytes were read from filehandle $fh
sub read_patiently {
  my($fh, $count) = @_;
  my $already_read = 0;
  my $result;
  while($already_read < $count) {
    my $nread = sysread($fh, $result, $count-$already_read, $already_read);
    if ($nread == 0) {
      exit 0;
    } elsif ($nread < 0) {
      die_with_errormessage("error reading: $!");
    }
    $already_read += $nread;
  }
  return $result;
}

# keep writing until all bytes from $buffer were written to $fh
sub write_patiently {
  my($fh, $buffer) = @_;
  my $already_written = 0;
  my $count = length($buffer);
  while($already_written < $count) {
    my $nwritten = syswrite($fh, $buffer, $count-$already_written, $already_written);
    if ($nwritten <= 0) {
      die_with_errormessage("error writing: $!");
    }
    $already_written += $nwritten;
  }
}


# read message (tag, length word and contents) from FILTER_IN
sub read_message {
  return read_from_stdin() unless $we_are_running_under_rlwrap;

  my $tag = unpack("C", read_patiently(*FILTER_IN,1));
  my $length = unpack("L",read_patiently(*FILTER_IN,4));
  my $message = read_patiently(*FILTER_IN, $length);
  $message =~ s/\n$//;
  return ($tag, $message);
}


sub write_message {
  my($tag, $message) = @_;
  return write_to_stdout($tag, $message) unless $we_are_running_under_rlwrap;

  $message ||= ""; # allow undefined messages

  write_patiently(*FILTER_OUT, pack("C", $tag));
  write_patiently(*FILTER_OUT, pack("L", (length $message) + 1));
  write_patiently(*FILTER_OUT, "$message\n");
}

sub read_from_stdin {
  my ($tag, $prompt, $tagname, $message);
  while (not defined $tag) {
    print $prompt;
    ($tagname, $message) = (<STDIN> =~ /(\S+) (.*?)\r?\n/);
    exit unless $tagname;
    $tag = name2tag(undef, $tagname); # call as function, not method
    $prompt = "again > ";
  }
  return ($tag, $message)
}

sub write_to_stdout {
  my($tag, $message) = @_;
  print tag2name(undef, $tag) . " $message\n";
}


sub add_to_completion_list {
  my ($self, @words) = @_;
  write_message(TAG_ADD_TO_COMPLETION_LIST, join(' ', @words));
}

sub remove_from_completion_list {
  my ($self, @words) = @_;
  write_message(TAG_REMOVE_FROM_COMPLETION_LIST, join(' ', @words));
}


sub cwd {
  my ($self) = @_;
  my $command_pid = $ENV{RLWRAP_COMMAND_PID};
  my $pwd = "/proc/$command_pid/cwd";
  croak  "cannot read commands working directory as $pwd doesn't exist" unless -e $pwd;
  return (-l $pwd ? readlink ($pwd) : $pwd);
}



# have a private chat with the rlwrapped command. This relies very much om the assumption that command stops
# talking, and only listens, when it has displayed the $prompt
sub cloak_and_dagger {
  my ($self, $question, $prompt, $timeout) = @_;
  $prompt ||= $self -> last('prompt');
  write_patiently(*CMD_IN, "$question\n");
  $self -> send_output_oob("cloak_and_dagger question: $question\n") if $self -> {cloak_and_dagger_verbose};
  my $response = read_until(*CMD_OUT, $prompt, $timeout);
  $response =~ s/.*?\n//; # chop off echoed question;
  $response =~ s/$prompt$//; # chop off prompt;
  $self -> send_output_oob("cloak_and_dagger response: $response\n") if $self -> {cloak_and_dagger_verbose};
  return $response;
}



sub tag2name {
  my ($self, $tag) = @_;
  for my $name (qw(TAG_REMOVE_FROM_COMPLETION_LIST TAG_ADD_TO_COMPLETION_LIST TAG_INPUT TAG_PROMPT TAG_COMPLETION
		   TAG_HOTKEY TAG_HISTORY TAG_OUTPUT_OUT_OF_BAND TAG_ERROR  TAG_IGNORE TAG_OUTPUT)) {
    return $name if (eval "$tag == $name");
  }
  croak "unknown tag $tag";
}

sub name2tag {
  my ($self, $name ) = @_;
  my $tag = eval uc $name;
  #croak "unknown tagname $name " if $@;
  return $tag;
}

sub send_output_oob {
  my ($self, $text) = @_;
  write_message(TAG_OUTPUT_OUT_OF_BAND, $text);
}



sub  send_ignore_oob {
  my ($self, $text) = @_;
  write_message(TAG_IGNORE, $text);
}

sub die_with_error_message {
  my ($error_message) = @_;
  die $error_message if $^S; # make die() within eval do the right thing
  my $myself = $0;
  $myself =~ s#^.*/([^.]+)$#$1#;
  write_message(TAG_ERROR, "$myself: $error_message");
  sleep 2;
  exit 1;
}


sub warn_with_info_message {
  my ($warning) = @_;
  my $myself = $0;
  $myself =~ s#^.*/([^.]+)$#$1#;
  write_message(TAG_OUTPUT_OUT_OF_BAND, "$myself: $warning");

}

sub minimal_rlwrap_version {
  my ($self, $wanted) = @_;
  my $found = $ENV{RLWRAP_VERSION} || "0.34";
  die "This filter requires rlwrap version $wanted or newer!\n"
    unless !$we_are_running_under_rlwrap or $wanted le $found;
}


sub command_line {
  my $commandline = $ENV{RLWRAP_COMMAND_LINE};
  return (wantarray ? split /\s+/, $commandline : $commandline);
}

sub running_under_rlwrap {
  return $we_are_running_under_rlwrap;
}

sub prompt_rejected {
  my ($self) = @_;
  $self->minimal_rlwrap_version("0.35");
  return REJECT_PROMPT;
}

sub name {
  my ($name) = ($0 =~ m#([^/]+)$#);
  $name ||= $0;
  return $name;
}


1

__END__


=head1 NAME

RlwrapFilter - Perl class for B<rlwrap> filters

=head1 SYNOPSIS

  use lib $ENV{RLWRAP_FILTERDIR};
  use RlwrapFilter;

  $filter = new RlwrapFilter;

  $filter -> output_handler(sub {s/apple/orange/; $_}); # re-write output
  $filter -> prompt_handler(\&pimp_the_prompt); # change prompt
  $filter -> history_handler(sub {s/with password \w+/with password ****/; $_}); # keep passwords out of history

  $filter -> run;

=head1 DESCRIPTION

B<rlwrap> (1) (L<http://utopia.knoware.nl/~hlub/uck/rlwrap>) is a tiny
utility that sits between the user and any console command, in order
to bestow readline capabilities (line editing, history recall) to
commands that don't have them.

Since version 0.32, rlwrap can use filters to script almost every
aspect of rlwrap's interaction with the user: changing the history,
re-writing output and input, calling a pager or computing completion
word lists from the current input.

B<RlwrapFilter> makes it very simple to write rlwrap
filters in perl. A filter only needs to instantiate a RlwrapFilter
object, change a few of its default handlers and then call its 'run'
method.

There is also a Python 3 module B<rlwrapfilter.py>, distributed
together with B<rlwrap>, that provides  more or less the same API as
its B<perl> counterpart.

=head1 PUBLIC METHODS

=head2 CONSTRUCTOR

=over 4

=item $f = new RlwrapFilter

=item $f = RlwrapFilter -> new(prompt_handler => sub {"Hi! > "}, minimal_rlwrap_version => "0.35", ...)

Return a new RlwrapFilter object.

=back

=head2 SETTING/GETTING HANDLERS

Handlers are user-defined callbacks that specify one or more of an
RlwrapFilter object's handler methods (handle_input, handle_prompt)
They get called from the 'run' method in response to a message sent
from B<rlwrap>.  Messages consist of a tag indicating which handler
should be called (e.g. TAG_INPUT, TAG_HISTORY) and the message
text. Usually, a filter overrides only one or at most two methods.

=head3 CALLING CONVENTIONS

In many cases (e.g. TAG_INPUT, TAG_OUTPUT, TAG_PROMPT) the message
text is a simple string. Their handlers are called with the message
text (i.e. the un-filtered input, output, prompt) as their only
argument. For convenience, $_ is set to the same value. They should
return the re-written message text.

Some handlers (those for TAG_COMPLETION and TAG_HOTKEY) are a little
more complex: their message text (accessible via $_) is a
tab-separated list of fields; they get called with multiple arguments
and are evaluated in list context.


The message handlers are called in a fixed cyclic order: prompt,
completion, history, input, echo, output, prompt, ... etc ad
infinitum. Rlwrap may always skip a handler when in direct mode; on
the other hand, completion and output handlers may get called more
than once in succession. If a handler is left undefined, the result is
as if the message text were returned unaltered.

It is important to note that the filter, and hence all its handlers,
are bypassed when I<command> is in direct mode, i.e. when it asks for
single keystrokes (and also, for security reasons, when it doesn't
echo, e.g. when asking for a password). If you don't want this to happen, use
B<rlwrap -a> to force B<rlwrap> to remain in readline mode and to
apply the filter to I<all> of I<command>'s in- and output. This will
make editors and pagers (which respond to single keystrokes) unusable,
unless you use rlwrap's B<-N> option (linux only)


The getters/setters for the respective handlers are listed below:

=over 4



=item $handler = $f -> prompt_handler, $f -> prompt_handler(\&handler)

The prompt handler re-writes prompts and gets called when rlwrap
decides it is time to "cook" the prompt, by default some 40 ms after
the last output has arrived. Of course, B<rlwrap> cannot read the mind
of I<command>, so what looks like a prompt to B<rlwrap> may actually
be the beginning of an output line that took I<command> a little
longer to formulate. If this is a problem, specify a longer "cooking"
time with rlwrap's B<-w> option, use the B<prompts_are_never_empty>
method or "reject" the prompt (cf. the B<prompt_rejected> method)


=item $handler = $f -> completion_handler, $f -> completion_handler(\&handler)

The completion handler gets called with three arguments: the entire input
line, the prefix (partial word to complete), and rlwrap's own completion list.
It should return a (possibly revised) list of completions.
As an example, suppose the user has typed "She played for
AE<lt>TABE<gt>". The handler will be called like this:

     myhandler("She played for A", "A", "Arsenal", "Arendal", "Anderlecht")

it could then return a list of stronger clubs: ("Ajax", "AZ67",  "Arnhem")

=item $handler = $f -> history_handler, $f -> history_handler(\&handler)

Every input line is submitted to this handler, the return value is put
in rlwrap's history. Returning an empty or undefined value will keep
the input line out of the history.

=item $handler = $f -> hotkey_handler, $f -> hotkey_handler(\&handler)

If, while editing an input line, the user presses a key that is bound
to "rlwrap_hotkey" in B<.inputrc>, the handler is called with five
arguments: the hotkey, the prefix (i.e.  the part of the current input
line before the cursor), the remaining part of the input line
(postfix), the history as one string ("line 1\nline 2\n...line N", and
the history position. It has to return a similar list, except that the
first element will be printed in the "echo area" if it is changed from
its original value.


B<Example:> if the current input line is  "pea soup" (with the cursor on the
space), and the user presses CTRL+P, which happens to be bound to "rlwrap-hotkey"
in B<.inputrc>, the handler is called like this:

    my_handler("\0x10", "pea", " soup", "tomato soup\nasparagus..", 12) # 16 = CTRL-P

If you prefer peanut soup, the handler should return

    ("Mmmm!", "peanut", " soup", "asparagus..", 11)

after which the input line will be "peanut soup" (with the cursor
again on the space), the echo area will display "Mmmm!", and any reference
to inferior soups will have been purged from the history.


=item $handler = $f -> input_handler, $f -> input_handler(\&handler)

Every input line is submitted to this handler, The handler's return
value is written to I<command>'s pty (pseudo-terminal).

=item $handler = $f -> echo_handler, $f -> echo_handler(\&handler)

The first line of output that is read back from I<command>'s pty is
the echo'ed input line. If your input handler alters the input line,
it is the altered input that will be echo'ed back. If you don't want
to confuse the user, use an echo handler that returns your original
input.

If you use rlwrap in --multi-line mode, additional echo lines will
have to be handled by the output handler


=item $handler = $f -> output_handler, $f -> output_handler(\&handler)

All I<command> output after the echo line is submitted to the output
handler (including newlines). This handler may get called many times in succession,
dependent on the size of I<command>'s write() calls, and the whims of
your system's scheduler. Therefore your handler should be prepared to
rewrite your output in "chunks", where you even don't have the
guarantee that the chunks contain entire unbroken lines.

If you want to handle I<command>'s entire output in one go, you can
specify an output handler that returns an empty string, and then use
$filter -> cumulative_output in your prompt handler to send the
re-written output "out-of-band" just before the prompt:

    $filter -> output_handler(sub {""});

    $filter -> prompt_handler(
                  sub{ $filter -> send_output_oob(mysub($filter -> cumulative_output));
                       "Hi there > "
                     });


Note that when rlwrap is run in --multi-line mode the echo handler will still
only handle the first echo line.  The remainder will generally
be echoed back preceded by a continuation prompt; it is up to the
output handler what to do with it.


=item $handler = $f -> message_handler, $f -> message_handler(\&handler)

This handler gets called (as handler($message, $tag)) for every
incoming message, and every tag (including out-of-band tags), before
all other handlers. Its return value is ignored, but it may be useful
for logging and debugging purposes. The $tag is an integer that can be
converted to a tag name by the 'tag2name' method

=back

=head2 OTHER METHODS

=over 4

=item $f -> help_text("Usage...")

Set the help text for this filter. It will be displayed by rlwrap -z
<filter>. The second line of the help text is used by C<rlwrap -z listing>;
it should be a short description of what the filter does.

=item $f -> minimal_rlwrap_version("x.yy")

Die unless rlwrap is version x.yy or newer

=item $dir = $f -> cwd

return the name of I<command>'s current working directory. This uses
the /proc filesystem, and may only work on newer linux systems (on
older linux and on Solaris, it will return something like
"/proc/12345/cwd", useful to find the contents of I<command>'s working
directory, but not its name)


=item $text = $f -> cumulative_output

return the current cumulative output. All (untreated) output gets
appended to the cumulative output after the output_handler has been
called. The cumulative output starts with a fresh slate with every
OUTPUT message that directly follows an INPUT message (ignoring out-of-band
messages and rejected prompts)

When necessary (i.e. when B<rlwrap> is in "impatient mode") the prompt
is removed from $filter->cumulative_output by the time the prompt
handler is called.

=item $tag = $f -> previous_tag

The tag of the last preceding in-band message. A tag is an integer between 0 and
255, its name can be found with the following method:

=item  $name = $f -> tag2name($tag)

Convert the tag (an integer) to its name (e.g. "TAG_PROMPT")

=item  $name = $f -> name2tag($tag)

Convert a valid tag name like "TAG_PROMPT" to a tag (an integer)

=item $f -> send_output_oob($text)

Make rlwrap display C<$text>. C<$text> is sent "out-of-band":
B<rlwrap> will not see it until just  after it has sent the next
message to the filter

=item $f -> send_ignore_oob($text)

Send an out-of-band TAG_IGNORE message to rlwrap. B<rlwrap> will silently
discard it, but it can be useful when debugging filters

=item $f -> add_to_completion_list(@words)

=item $f -> remove_from_completion_list(@words)

Permanently add or remove the words in C<@words> to/from rlwrap's completion list.

=item $f -> cloak_and_dagger($question, $prompt, $timeout);

Send C<$question> to I<command>'s input and read back everything that
comes back until C<$prompt> is seen at "end-of-chunk", or no new
chunks arrive for $timeout seconds, whichever comes first.  Return the
response (without the final C<$prompt>).  B<rlwrap> remains completely
unaware of this conversation.

=item $f -> cloak_and_dagger_verbose($verbosity)

If $verbosity evaluates to a true value, make rlwrap print all
questions sent to I<command> by the C<cloak_and_dagger> method, and
I<command>'s responses. By default, $verbosity = 0; setting it to
1 will mess up the screen but greatly facilitate the (otherwise rather tricky) use of
C<cloak_and_dagger>

=item $self -> prompt_rejected

A special text ("_THIS_CANNOT_BE_A_PROMPT_") to be returned by a
prompt handler to "reject" the prompt. This will make rlwrap skip
cooking the prompt.  $self->previous_tag and $self->cumulative_output
will not be touched.

=item $text = $f -> prompts_are_never_empty($val)

If $val evaluates to a true value, automatically reject empty prompts.

=item $f -> command_line

In scalar context: the rlwrapped command and its arguments as a string ("command -v blah")
in list context: the same as a list ("command", "-v", "blah")

=item $f -> running_under_rlwrap

Whether the filter is run by B<rlwrap>, or directly from the command line

=item $f -> run

Start an event loop that reads rlwrap's messages from the input pipe,
calls the appropriate handlers and writes the result to the output
pipe.  This method never returns.

=back



=head1 LOW LEVEL PROTOCOL

B<rlwrap> communicates with a filter through messages consisting of a tag
byte (TAG_OUTPUT, TAG_PROMPT etc. - to inform the filter of what is
being sent), an unsigned 32-bit integer containing the length of the
message, the message text and an extra newline. For every message
sent, rlwrap expects, and waits for an answer message with the same
tag. Sending back a different (in-band) tag is an error and instantly
kills rlwrap, though filters may precede their answer message with
"out-of-band" messages to output text (TAG_OUTPUT_OUT_OF_BAND), report
errors (TAG_ERROR), and to manipulate the completion word list
(TAG_ADD_TO_COMPLETION_LIST and TAG_REMOVE_FROM_COMPLETION_LIST)
Out-of-band messages are not serviced by B<rlwrap> until right after
it has sent the next in-band message - the communication with the
filter is synchronous and driven by rlwrap.

Messages are received and sent via two pipes. STDIN, STDOUT and STDERR
are still connected to the user's terminal, and you can read and write
them directly, though this may mess up the screen and confuse the user
unless you are careful. A filter can even communicate with the
rlwrapped command behind rlwrap's back (cf the cloak_and_dagger()
method)

The protocol uses the following tags (tags E<gt> 128 are out-of-band)

 TAG_INPUT       0
 TAG_OUTPUT      1
 TAG_HISTORY     2
 TAG_COMPLETION  3
 TAG_PROMPT      4
 TAG_HOTKEY      5

 TAG_IGNORE                      251
 TAG_ADD_TO_COMPLETION_LIST      252
 TAG_REMOVE_FROM_COMPLETION_LIST 253
 TAG_OUTPUT_OUT_OF_BAND          254
 TAG_ERROR                       255


To see how this works, you can eavesdrop on the protocol
using the 'logger' filter.

The constants TAG_INPUT, ... are exported by the RlwrapFilter.pm module.

=head1 SIGNALS

As STDIN is still connected to the users teminal, one might expect the filter
to receive SIGINT, SIGTERM, SIGTSTP directly from the terminal driver if
the user presses CTRL-C, CTRL-Z etc Normally, we don't want this - it
would confuse rlwrap, and the user (who thinks she is talking straight
to the rlwapped command) probably meant those signals to be sent to
the command itself. For this reason the filter starts with all signals blocked.

Filters that interact with the users terminal (e.g. to run a pager)
should unblock signals like SIGTERM, SIGWINCH.

=head1 FILTER LIFETIME

The filter is started by B<rlwrap> after I<command>, and stays alive
as long as B<rlwrap> runs. Filter methods are immediately usable. When
I<command> exits, the filter stays around for a little longer in order
to process I<command>'s last words. As calling the cwd and
cloak_and_dagger methods at that time will make the filter die with an
error, it may be advisable to wrap those calls in eval{}

If a filter calls die() it will send an (out-of-band) TAG_ERROR
message to rlwrap before exiting. rlwrap will then report the message
and exit (just after its next in-band message - out-of-band messages
are not always processed immediately)

die() within an eval() sets $@ as usual.

=head1 ENVIRONMENT

Before calling a filter, B<rlwrap> sets the following environment variables:

    RLWRAP_FILTERDIR      directory where RlwrapFilter.pm and most filters live (set by B<rlwrap>, can be
                          overridden by the user before calling rlwrap)

    PATH	          rlwrap automatically adds $RLWRAP_FILTERDIR to the front of filter's PATH

    RLWRAP_VERSION        rlwrap version (e.g. "0.35")

    RLWRAP_COMMAND_PID    process ID of the rlwrapped command

    RLWRAP_COMMAND_LINE   command line of the rlwrapped command

    RLWRAP_IMPATIENT      whether rlwrap is in "impatient mode" (cf B<rlwrap (1)>). In impatient mode,
                          the candidate prompt is filtered through the output handler (and displayed before
                          being overwritten by the cooked prompt).

    RLWRAP_INPUT_PIPE_FD  File descriptor of input pipe. For internal use only

    RLWRAP_OUTPUT_PIPE_FD File descriptor of output pipe. For internal use only

    RLWRAP_MASTER_PTY_FD File descriptor of I<command>'s pty.


=head1 DEBUGGING FILTERS

While RlwrapFilter.pm makes it easy to write simple filters, debugging
them can be a problem. A couple of useful tricks:

=head2 LOGGING

When running a filter, the in- and outgoing messages can be logged by
the B<logger> filter, using a pipeline:

  rlwrap -z 'pipeline logger incoming : my_filter : logger outgoing' command


=head2 RUNNING WITHOUT B<rlwrap>

When called by rlwrap, filters get their input from
$RLWRAP_INPUT_PIPE_FD and write their output to
$RLWRAP_OUTPUT_PIPE_FD, and expect and write messages consisting of a
tag byte, a 32-bit length and the message proper. This is not terribly
useful when running a filter directly from the command line (outside
rlwrap), even if we set the RLWRAP_*_FD ourselves.

Therefore, when run directly from the command line, a filter expects
input messages on its standard input of the form

TAG_PROMPT myprompt >

(i.a. a tag name, one space and a message followed by a newline) and it will respond in the
same way on its standard output


=head1 SEE ALSO

B<rlwrap> (1), B<readline> (3)
