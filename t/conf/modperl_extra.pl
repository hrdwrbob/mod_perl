use Socket (); #test DynaLoader vs. XSLoader workaround for 5.6.x
use IO::File ();

use Apache2 ();

use ModPerl::Util (); #for CORE::GLOBAL::exit

use Apache::RequestRec ();
use Apache::RequestIO ();
use Apache::RequestUtil ();

use Apache::Server ();
use Apache::ServerUtil ();
use Apache::Connection ();
use Apache::Log ();

use Apache::Const -compile => ':common';
use APR::Const -compile => ':common';

eval { require TestFilter::input_msg };

use APR::Table ();

unless ($ENV{MOD_PERL}) {
    die '$ENV{MOD_PERL} not set!';
}

#see t/modperl/methodobj
use TestModperl::methodobj ();
$TestModperl::MethodObj = TestModperl::methodobj->new;

#see t/response/TestModperl/env.pm
$ENV{MODPERL_EXTRA_PL} = __FILE__;

my $ap_mods = scalar grep { /^Apache/ } keys %INC;
my $apr_mods = scalar grep { /^APR/ } keys %INC;

Apache::Log->info("$ap_mods Apache:: modules loaded");
Apache::Server->log->info("$apr_mods APR:: modules loaded");

{
    my $server = Apache->server;
    my $vhosts = 0;
    for (my $s = $server->next; $s; $s = $s->next) {
        $vhosts++;
    }
    $server->log->info("base server + $vhosts vhosts ready to run tests");
}

sub ModPerl::Test::read_post {
    my $r = shift;

    $r->setup_client_block;

    return undef unless $r->should_client_block;

    my $len = $r->headers_in->get('content-length');

    my $buf;
    $r->get_client_block($buf, $len);

    return $buf;
}

sub ModPerl::Test::add_config {
    my $r = shift;

    #test adding config at request time
    my $errmsg = $r->add_config(['require valid-user']);
    die $errmsg if $errmsg;

    Apache::OK;
}

#<Perl handler=ModPerl::Test::perl_section>
# ...
#</Perl>
sub ModPerl::Test::perl_section {
    my($parms, $args) = @_;

    require Apache::CmdParms;
    require Apache::Directive;

    my $code = $parms->directive->as_string;
    my $package = $args->{package} || 'Apache::ReadConfig';

##   a real handler would do something like:
#    eval "package $package; $code";
#    die $@ if $@;
##   feed %Apache::ReadConfig:: to Apache::Server->add_config

    my $htdocs = Apache::server_root_relative($parms->pool, 'htdocs');

    my @cfg = (
       "Alias /perl_sections $htdocs",
       "<Location /perl_sections>",
#       "   require valid-user",
       "   PerlInitHandler ModPerl::Test::add_config",
       "   AuthType Basic",
       "   AuthName PerlSection",
       "   PerlAuthenHandler TestHooks::authen",
       "</Location>",
    );

    my $errmsg = $parms->server->add_config(\@cfg);

    die $errmsg if $errmsg;

    Apache::OK;
}

END {
    warn "END in modperl_extra.pl, pid=$$\n";
}

1;