package Bundle::Apache;

$VERSION = '1.00';

1;

__END__

=head1 NAME

Bundle::Apache - Install Apache mod_perl and related modules

=head1 SYNOPSIS

C<perl -MCPAN -e 'install Bundle::Apache'>

=head1 CONTENTS

Apache - Perl interface to Apache server API

ExtUtils::Embed - Needed to build httpd

LWP::UserAgent - Web client to run mod_perl tests

HTML::TreeBuilder - Used for Apache::SSI

Devel::Symdump - Symbol table browsing with Apache::Status

HTTPD::UserAdmin - Apache::Authen stuff

CGI - CGI.pm

Apache::ePerl - Mark Imbriaco's mod_perl adaptation of 'ePerl'

=head1 DESCRIPTION

This bundle contains modules used by Apache mod_perl.

Asking CPAN.pm to install a bundle means to install the bundle itself
along with all the modules contained in the CONTENTS section
above. Modules that are up to date are not installed, of course.

=head1 AUTHOR

Doug MacEachern
