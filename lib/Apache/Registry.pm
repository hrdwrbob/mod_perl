package Apache::Registry;
use Apache ();
use Apache::Debug ();
use Apache::Constants qw(:common &OPT_EXECCGI);
use FileHandle ();

use vars qw($VERSION $Debug);
#$Id: $
$VERSION = (qw$Revision: 1.21 $)[1];

$Debug ||= 0;
# 1 => log recompile in errorlog
# 2 => Apache::Debug::dump in case of $@
# 4 => trace pedantically

sub handler {
    my($r) = @_;
    Apache->request($r);
    my $filename = $r->filename;

    if (-r $filename && -s _) {
	if (!($r->allow_options & OPT_EXECCGI)) {
	    $r->log_reason("Options ExecCGI is off in this directory", 
			   $filename);
	    return FORBIDDEN;
	}
	if (-d _) {
	    $r->log_reason("attempt to invoke directory as script", $filename);
	    return FORBIDDEN;
	}
	unless (-x _) {
	    $r->log_reason("file permissions deny server execution", 
			   $filename);
	    return FORBIDDEN;
	}

	my $mtime = -M _;

	# turn into a package name
	$r->log_error("Apache::Registry::handler checking out $script_name")
	    if $Debug & 4;
	my $script_name = 
	    substr($r->uri, 0, length($r->uri)-length($r->path_info));

	# Escape everything into valid perl identifiers
	$script_name =~ s/([^A-Za-z0-9\/])/sprintf("_%2x",unpack("C",$1))/eg;
	# second pass only for words starting with a digit
	$script_name =~ s|/(\d)|sprintf("/_%2x",unpack("C",$1))|eg;

	# Dress it up as a real package name
	$script_name =~ s|/|::|g;
	my $package = "Apache::ROOT$script_name";
	$r->log_error("Apache::Registry::handler determined package as $package") 
	   if $Debug & 4;

	if (
	    defined $Apache::Registry->{$package}{mtime}
	    &&
	    $Apache::Registry->{$package}{mtime} <= $mtime
	   ){
	    # we have compiled this subroutine already, nothing left to do
	} else {
           $r->log_error("Apache::Registry::handler reading $filename")
	       if $Debug & 4;
	    my $fh = new FileHandle $filename;
	    local($/);
	    undef $/;
	    my $sub = <$fh>;
	    $fh->close;
	    undef $fh;
	    # compile this subroutine into the uniq package name
            $r->log_error("Apache::Registry::handler eval-ing") if $Debug & 4;
            my $eval = qq{package $package; sub handler { $sub; }};
            {
                # hide our variables within this block
                my($r,$filename,$script_name,$mtime,$package,$sub);
                eval $eval;
            }
	    if ($@) {
		$r->log_error($@);
		return SERVER_ERROR unless $Debug & 2;
		return Apache::Debug::dump($r, SERVER_ERROR);
	    }
            $r->log_error(qq{Compiled package \"$package\" for process $$})
	       if $Debug & 1;
	    $Apache::Registry->{$package}{mtime} = $mtime;
	}

	eval {$package->handler;};
	if ($@) {
	    $r->log_error($@);
	    return SERVER_ERROR unless $Debug & 2;
	    return Apache::Debug::dump($r, SERVER_ERROR);
	}
	return $r->status;
    } else {
	return NOT_FOUND unless $Debug & 2;
	return Apache::Debug::dump($r, NOT_FOUND);
    }
}

1;

__END__

=head1 NAME

Apache::Registry - Run (mostly) unaltered CGI scripts through mod_perl

=head1 SYNOPSIS

 #in srm.conf

 PerlAlias /perl/ /perl/apache/scripts/ #optional
 PerlModule Apache::Registry 
 
 <Location /perl>
 SetHandler perl-script
 PerlHandler Apache::Registry
 ...
 </Directory>


=head1 DESCRIPTION

URIs in the form of:
 http://www.host.com/perl/file.pl

Will be compiled as the body of a perl subroutine and executed.
Each server process or 'child' will compile the subroutine once 
and store it in memory until the file is updated on the disk.

The file looks much like a "normal" script, but it is compiled or 'evaled'
into a subroutine.

Here's an example:

 my $r = Apache->request;
 $r->content_type("text/html");
 $r->send_http_header;
 $r->print("Hi There!");

Apache::Registry::handler will preform the same checks as mod_cgi
before running the script.

=head1 DEBUGGING

You may set the debug level with the $Apache::Registry::Debug bitmask

 1 => log recompile in errorlog
 2 => Apache::Debug::dump in case of $@
 4 => trace pedantically
 
=head1 SEE ALSO

perl(1), Apache(3), Apache::Debug(3)

=head1 AUTHORS

Andreas Koenig <andreas.koenig@franz.ww.tu-berlin.de> and 
Doug MacEachern <dougm@osf.org>

