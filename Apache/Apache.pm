package Apache;
use strict;
use vars qw($VERSION @ISA @EXPORT_OK);
use Apache::Constants qw(OK);
use DynaLoader ();

@ISA = qw(Exporter DynaLoader);
@EXPORT_OK = qw(exit warn);
$VERSION = "1.12";
$Apache::CRLF = "\015\012";

bootstrap Apache $VERSION;

*Apache::warn = \&Apache::log_error;

sub module {
    my($self, $module) = @_;
    eval "require $module;";
}

sub parse_args {
    my($wantarray,$string) = @_;
    return unless defined $string and $string;
    if(defined $wantarray and $wantarray) {
	return map { Apache::unescape_url_info($_) } split /[=&]/, $string;
    }
    $string;
}

sub content {
    my($r) = @_;
    my $ct = $r->header_in("Content-type");
    return unless $ct eq "application/x-www-form-urlencoded";
    my $buff;
    $r->read($buff, $r->header_in("Content-length"));
    parse_args(wantarray, $buff);
}

sub args {
    my($r) = @_;
    parse_args(wantarray, $r->query_string);
}

sub cgi_var {
    my($r, $key) = @_;
    my $val = $r->cgi_env($key);
    return $val;
}

*READ = \&read;
sub read {
    my($r, $bufsiz) = @_[0,2];
    my($nrd, $buf, $total);
    $nrd = $total = 0;
    $buf = "";
    $_[1] ||= "";
    $r->hard_timeout("Apache->read");

    while($bufsiz) {
	$nrd = $r->read_client_block($buf, $bufsiz) || 0;
	if(defined $nrd and $nrd > 0) {
	    $bufsiz -= $nrd;
	    $total += $nrd;
	    $_[1] .= $buf;
	    next if $bufsiz;
	    last;
	}
	else {
	    $_[1] = undef;
	}
    }
    $r->kill_timeout;
    return $total;
}

sub GETC { my $c; shift->read($c,1); $c; }

#shouldn't use <STDIN> anyhow, but we'll be nice
sub READLINE { 
    my $r = shift;
    my $line; 
    $r->read($line, $r->header_in('Content-length'));
    $line;
}

*PRINT = \&print;
sub print {
    my($r) = shift;
    if(!$r->sent_header) {
	$r->send_cgi_header(join '', @_);
	$r->sent_header(1);
    }
    else {
	$r->hard_timeout("Apache->print");
	$r->write_client(@_);
	$r->kill_timeout;
    }
}

sub send_cgi_header {
    my($r, $headers) = @_;
    my $dlm = "\015?\012"; #a bit borrowed from LWP::UserAgent
    my(@headerlines) = split /$dlm/, $headers;
    my($key, $val);

    foreach (@headerlines) {
	if (/^(\S+?):\s*(.*)$/) {
	    ($key, $val) = ($1, $2);
	    last unless $key;
	    if($key eq "Status") {
		$r->status_line($val);
		next;
	    }
	    elsif($key eq "Location") {
		if($val =~ m,^/,) {
		    #/* This redirect needs to be a GET no 
                    #   matter what the original
		    # * method was.

		    $r->method("GET");
		    $r->method_number(0); #M_GET 
		    $r->internal_redirect_handler($val);
		    return OK;
		}
		else {
		    $r->header_out(Location => $val);
		    $r->status(302);
		    next;
		}
	    }
	    elsif($key eq "Content-type") {
		$r->content_type($val);
		next;
	    }
	    else {
		$r->header_out($key,$val);
		next;
	    }
	}
	else {
	    warn "Illegal header '$_'";
	}
    }
    $r->send_http_header;
}

sub as_string {
    my($r) = @_;
    my($k,$v,@retval);
    my(%headers_in) = $r->headers_in;

    push @retval, $r->the_request;
    while(($k,$v) = each %headers_in) {
	push @retval, "$k: $v";
    }

    push @retval, "";

    push @retval, $r->status_line;
    for (qw(err_headers_out headers_out)) {
	my(%headers_out) = $r->$_();

	while(($k,$v) = each %headers_out) {
	    push @retval, "$k: $v";
	}
    }    
    join "\n", @retval, "";
}

sub TIEHANDLE {
    my($class, $r) = @_;
    $r ||= Apache->request;
}

#some backwards-compatible stuff
*Apache::TieHandle::TIEHANDLE = \&TIEHANDLE;
*CGI::Apache::exit = \&Apache::exit;

1;

__END__

=head1 NAME

Apache - Perl interface to the Apache server API

=head1 SYNOPSIS

   use Apache ();

=head1 DESCRIPTION

This module provides a Perl interface the Apache API.  It is here
mainly for B<mod_perl>, but may be used for other Apache modules that
wish to embed a Perl interpreter.  We suggest that you also consult
the description of the Apache C API at http://www.apache.org/docs/.

=head1 THE REQUEST OBJECT

The request object holds all the information that the server needs to
service a request.  Apache B<Perl*Handler>s will be given a reference to the
request object as parameter and may choose update or use it in various
ways.  Most of the methods described below obtain information from or
updates the request object.
The perl version of the request object will be blessed into the B<Apache> 
package, it is really a C<request_rec *> in disguise.

=over 4

=item Apache->request([$r])

The Apache->request method will return a reference to the request object.

B<Perl*Handler>s can obtain a reference to the request object when it
is passed to them via C<@_>.  However, scripts that run under 
L<Apache::Registry>, for example, need a way to access the request object.
L<Apache::Registry> will make a request object availible to these scripts
by passing an object reference to C<Apache->request($r)>.
If handlers use modules such as C<Apache::CGI> that need to access
L<Apache->request>, they too should do this (e.g. Apache::Status).

=item $r->as_string

Returns a string representation of the request object.

=item $r->main

If the current request is a sub-request, this method returns a blessed 
reference to the main request structure.

=item $r->prev

This method returns a blessed reference to the previous (internal) request
structure.

=item $r->next

This method returns a blessed reference to the next (internal) request
structure.

=item $r->is_main

Returns true if the current request object is for the main request.

=item $r->is_initial_req

Returns true if the current request is the first internal request, 
returns false if the request is a sub-request or interal redirect.  

=back

=head1 CLIENT REQUEST PARAMETERS

First we will take a look at various methods that can be used to
retrieve the request parameters sent from the client.
In the following examples, B<$r> is a request object blessed into the 
B<Apache> class, obtained by the first parameter passed to a handler subroutine
or I<Apache-E<gt>request>

=over 4

=item $r->method( [$meth] )

The $r->method method will return the request method.  It will be a
string such as "GET", "HEAD" or "POST".
Passing an argument will set the method, mainly used for internal redirects.

=item $r->method_number( [$num] )

The $r->method_number method will return the request method number.
Each number corresponds to a string representation such as 
"GET", "HEAD" or "POST".
Passing an argument will set the method_number, mainly used for internal redirects and testing authorization restriction masks.

=item $r->the_request

The request line send by the client, handy for logging, etc.

=item $r->proxyreq

Returns true if the request is proxy http.
Mainly used during the filename translation stage of the request, 
which may be handled by a C<PerlTransHandler>.

=item $r->header_only

Returns true if the client is asking for headers only, 
e.g. if the request method was B<HEAD>.

=item $r->protocol

The $r->protocol method will return a string identifying the protocol
that the client speaks.  Typical values will be "HTTP/1.0" or
"HTTP/1.1".

=item $r->uri( [$uri] )

The $r->uri method will return the requested URI, optionally changing
it with the first argument.

=item $r->filename( [$filename] )

The $r->filename method will return the result of the I<URI --E<gt>
filename> translation, optionally changing it with the first argument
if you happen to be doing the translation.

=item $r->path_info( [$path_info] )

The $r->path_info method will return what is left in the path after the
I<URI --E<gt> filename> translation, optionally changing it with the first 
argument if you happen to be doing the translation.

=item $r->args

The $r->args method will return the contents of the URI I<query
string>.  When called in a scalar context, the entire string is
returned.  When called in a list context, a list of parsed I<key> =>
I<value> pairs are returned, i.e. it can be used like this:

  $query = $r->args;
  %in    = $r->args;

=item $r->headers_in

The $r->headers_in method will return a %hash of client request
headers.  This can be used to initialize a perl hash, or one could use
the $r->header_in() method (described below) to retrieve a specific
header value directly.

=item $r->header_in( $header_name, [$value] )

Return the value of a client header.  Can be used like this:

  $ct = $r->header_in("Content-type");

  $r->header_in($key, $val); #set the value of header '$key'

=item $r->content

The $r->content method will return the entity body read from the
client, but only if the request content type is
C<application/x-www-form-urlencoded>.
When called in a scalar context, the entire string is
returned.  When called in a list context, a list of parsed I<key> =>
I<value> pairs are returned.  *NOTE*: you can only ask for this once,
as the entire body is read from the client.

=item $r->read_client_block($buf, $bytes_to_read)

Read from the entity body sent by the client.  Example of use:

  $r->read_client_block($buf, $r->header_in('Content-length'));

=item $r->read($buf, $bytes_to_read)

This method uses read_client_block() to read data from the client, 
looping until it gets all of C<$bytes_to_read> or a timeout happens.

In addition, this method sets a timeout before reading with
C<$r->hard_timeout>

=item $r->get_remote_host

Lookup the client DNS hostname.  Might return I<undef> if the
hostname is not known.

=back

More information about the client can be obtained from the
B<Apache::Connection> object, as described below.

=over 4

=item $c = $r->connection

The $r->connection method will return a reference to the request
connection object (blessed into the B<Apache::Connection> package).
This is really a C<conn_rec*> in disguise.  The following methods can
be used on the connection object:

$c->remote_host

$c->remote_ip

$c->remote_logname

$c->user; #Returns the remote username if authenticated.

$c->auth_type; #Returns the authentication scheme used, if any.

$c->aborted; #returns true if the client stopped talking to us


=back

=head1 SERVER CONFIGURATION INFORMATION

The following methods are used to obtain information from server
configuration and access control files.

=over 4

=item $r->dir_config( $key )

Returns the value of a per-directory variable specified by the 
C<PerlSetVar> directive.

 #<Location /foo/bar>
 #SetPerlVar  Key  Value
 #</Location>

 my $val = $r->dir_config('Key');

=item $r->requires

Returns an array reference of hash references, containing information
related to the B<require> directive.  This is normally used for access
control, see L<Apache::AuthzAge> for an example.

=item $r->allow_options

The $r->allow_options method can be used for
checking if it is ok to run a perl script.  The B<Apache::Options>
module provide the constants to check against.

 if(!($r->allow_options & OPT_EXECCGI)) {
     $r->log_reason("Options ExecCGI is off in this directory", 
		    $filename);
 }

=item $s = $r->server

Return a reference to the server info object (blessed into the
B<Apache::Server> package).  This is really a C<server_rec*> in
disguise.  The following methods can be used on the server object:

=item $s->server_admin

Returns the mail address of the person responsible for this server.

=item $s->server_hostname

Returns the hostname used by this server.

=item $s->port

Returns the port that this servers listens too.

=item $s->is_virtal

Returns true if this is a virtual server.

=item $s->names

Returns the wildcarded names for HostAlias servers. 

=back

=head1 SETTING UP THE RESPONSE

The following methods are used to set up and return the response back
to the client.  This typically involves setting up $r->status(), the
various content attributes and optionally some additional
$r->header_out() calls before calling $r->send_http_header() which will
actually send the headers to the client.  After this a typical
application will call the $r->print() method to send the response
content to the client.

=over 4

=item $r->send_http_header

Send the response line and all headers to the client.  (This method
will actually call $r->basic_http_header first).

This method will create headers from the $r->content_xxx() and
$r->no_cache() attributes (described below) and then append the
headers defined by $r->header_out (or $r->err_header_out if status
indicates an error).

=item $r->get_basic_auth_pw

If the current request is protected by Basic authentication, 
this method will return 0, otherwise -1.  
The second return value will be the decoded password sent by the client.

    ($ret, $sent_pw) = $r->get_basic_auth_pw;

=item $r->note_basic_auth_failure

Prior to requiring Basic authentication from the client, this method 
will set the outgoing HTTP headers asking the client to authenticate 
for the realm defined by the configuration directive C<AuthName>.

=item $r->handler( [$meth] )

Set the handler for a request.
Normally set by the configuration directive C<AddHandler>.
  
 $r->handler( "perl-script" );

=item $r->content_type( [$newval] )

Get or set the content type being sent to the client.  Content types
are strings like "text/plain", "text/html" or "image/gif".  This
corresponds to the "Content-Type" header in the HTTP protocol.  Example
of usage is:

   $previous_type = $r->content_type;
   $r->content_type("text/plain");

=item $r->content_encoding( [$newval] )

Get or set the content encoding.  Content encodings are string like
"gzip" or "compress".  This correspond to the "Content-Encoding"
header in the HTTP protocol.

=item $r->content_language( [$newval] )

Get or set the content language.  The content language corresponds to the
"Content-Language" HTTP header and is a string like "en" or "no".

=item $r->status( $integer )

Get or set the reply status for the client request.  The
B<Apache::Constants> module provide mnemonic names for the status codes.

=item $r->status_line( $string )

Get or set the response status line.  The status line is a string like
"HTTP/1.0 200 OK" and it will take precedence over the value specified
using the $r->status() described above.


=item $r->headers_out

The $r->headers_out method will return a %hash of server response
headers.  This can be used to initialize a perl hash, or one could use
the $r->header_out() method (described below) to retrieve or set a specific
header value directly.

=item $r->header_out( $header, $value )

Change the value of a response header, or create a new one.  You
should not define any "Content-XXX" headers by calling this method,
because these headers use their own specific methods.  Example of use:

   $r->header_out("WWW-Authenticate" => "Basic");

   $val = $r->header_out($key);

=item $r->err_headers_out

The $r->err_headers_out method will return a %hash of server response
headers.  This can be used to initialize a perl hash, or one could use
the $r->err_header_out() method (described below) to retrieve or set a specific
header value directly.

The difference between headers_out and err_headers_out is that the
latter are printed even on error, and persist across internal redirects
(so the headers printed for ErrorDocument handlers will have them).

=item $r->err_header_out( $header, [$value] )

Change the value of an error response header, or create a new one.
These headers are used if the status indicates an error.

   $r->err_header_out("Warning" => "Bad luck");

   $val = $r->err_header_out($key);

=item $r->no_cache( $boolean )

This is a flag that indicates that the data being returned is volatile
and the client should be told not to cache it.

=item $r->print()

This method sends data to the client with C<$r->write_client>, but first
sets a timeout before sending with C<$r->hard_timeout>.

=item $r->send_fd( $filehandle )

Send the contents of a file to the client.  Can for instance be used
like this:

  open(FILE, $r->filename) || return 404;
  $r->send_fd(FILE);
  close(FILE);

=item $r->internal_redirect_handler( $newplace )

Redirect to a location in the server namespace without 
telling the client. For instance:

  $r->internal_redirect_handler("/home/sweet/home.html");

=back

=head1 SERVER CORE FUNCTIONS

=over 4

=item $r->soft_timeout($message)

=item $r->hard_timeout($message)

=item $r->kill_timeout

=item $r->reset_timeout

(Documentation borrowed from http_main.h)

There are two functions which modules can call to trigger a timeout
(with the per-virtual-server timeout duration); these are hard_timeout
and soft_timeout.

The difference between the two is what happens when the timeout
expires (or earlier than that, if the client connection aborts) ---
a soft_timeout just puts the connection to the client in an
"aborted" state, which will cause http_protocol.c to stop trying to
talk to the client, but otherwise allows the code to continue normally.
hard_timeout(), by contrast, logs the request, and then aborts it
completely --- longjmp()ing out to the accept() loop in http_main.
Any resources tied into the request resource pool will be cleaned up;
everything that is not will leak.

soft_timeout() is recommended as a general rule, because it gives your
code a chance to clean up.  However, hard_timeout() may be the most
convenient way of dealing with timeouts waiting for some external
resource other than the client, if you can live with the restrictions.

When a hard timeout is in scope, critical sections can be guarded
with block_alarms() and unblock_alarms() --- these are declared in
alloc.c because they are most often used in conjunction with
routines to allocate something or other, to make sure that the
cleanup does get registered before any alarm is allowed to happen
which might require it to be cleaned up; they * are, however,
implemented in http_main.c.

kill_timeout() will disarm either variety of timeout.

reset_timeout() resets the timeout in progress.

=back

=head1 CGI SUPPORT

We also provide some methods that make it easier to support the CGI
type of interface.

=over 4

=item $r->cgi_env

Return a %hash that can be used to set up a standard CGI environment.
Typical usage would be:

  %ENV = $r->cgi_env

B<NOTE:> The $ENV{GATEWAY_INTERFACE} is set to C<'CGI-Perl/1.1'> so
you can say:

  if($ENV{GATEWAY_INTERFACE} =~ /^CGI-Perl/) {
      #do mod_perl stuff
  }
  else {
     #do normal CGI stuff
  }

When given a key => value pair, this will set an environment variable.

 $r->cgi_env(REMOTE_GROUP => "camels");

=item $r->cgi_var($key);

Calls $r->cgi_env($key) in a scalar context to prevent the mistake
of calling in a list context.

    my $doc_root = $r->cgi_env('DOCUMENT_ROOT');

=item $r->send_cgi_header()

Take action on certain headers including I<Status:>, I<Location:> and
I<Content-type:> just as mod_cgi does, then calls
$r->send_http_header().  Example of use:

  $r->send_cgi_header("
  Location: /foo/bar
  Content-type: text/html 
  
  ");

=back

=head1 ERROR LOGGING

The following methods can be used to log errors. 

=over 4

=item $r->log_reason($message, $file)

The request failed, why??  Write a message to the server errorlog.

   $r->log_reason("Because I felt like it", $r->filename);

=item $r->log_error($message)

Uh, oh.  Write a message to the server errorlog.

  $r->log_error("Some text that goes in the error_log");

=item $r->warn($message)

An alias for Apache->log_error.

=back

=head1 UTILITY FUNCTIONS

=over 4

=item Apache::unescape_url($string)

Handy function for unescapes.  Use this one for filenames/paths.
Use unescape_url_info for the result of submitted form data.

=item Apache::unescape_url_info($string)

Handy function for unescapes submitted form data.
In opposite to unescape_url it translates the plus sign to space.

=item Apache::perl_hook($hook)

Test to see if a callback hook is enabled

 for (qw(Access Authen Authz Fixup HeaderParser Log Trans Type)) {
    print "$_ hook enabled\n" if Apache::perl_hook($_);
 }  

=back

=head1 SEE ALSO

perl(1),
Apache::Constants(3),
Apache::Registry(3),
Apache::CGI(3),
Apache::Debug(3),
Apache::Options(3)

=head1 AUTHORS

Gisle Aas <aas@sn.no> and Doug MacEachern <dougm@osf.org>

=cut
