package TestFilter::lc;

use strict;
use warnings FATAL => 'all';

use Apache::Filter ();

use Apache::Const -compile => 'OK';

sub handler {
    my $filter = shift;

    while ($filter->read(my $buffer, 1024)) {
        $filter->print(lc $buffer);
    }

    Apache::OK;
}

1;
__DATA__

<Location /top_dir>
  PerlOutputFilterHandler TestFilter::lc
</Location>

Alias /top_dir @top_dir@