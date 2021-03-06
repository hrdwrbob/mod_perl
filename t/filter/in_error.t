use strict;
use warnings FATAL => 'all';

use Apache::Test;
use Apache::TestUtil;
use Apache::TestRequest;

plan tests => 1;

my $location = '/TestFilter__in_error';

my $res = POST $location, content => 'foo';
ok t_cmp($res->code, 500, "an error in a filter should cause 500");

