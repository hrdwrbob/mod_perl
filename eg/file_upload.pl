#this does the same as file_upload.cgi from the CGI.pm examples/
#use with Apache::Registry
#use strict;
use CGI::Switch ();

my $query = new CGI::Switch;

$query->print(
    $query->header(),
	   
    $query->start_html("File Upload Example"),
);
$query->print(<<EOF);
<H1>File Upload Example</H1> 
This example demonstrates how to prompt the remote user to
select a remote file for uploading.  <STRONG>This feature
only works with Netscape 2.0 browsers.</STRONG>
<P>
Select the <VAR>browse</VAR> button to choose a text file
to upload.  When you press the submit button, this script
will count the number of lines, words, and characters in
the file.
EOF


# Start a multipart form.
$query->print(
    $query->start_multipart_form,
    "Enter the file to process:",
    $query->filefield('filename','',45),"<BR>\n",
);
my @types = ('count lines','count words','count characters');
$query->print(
    $query->checkbox_group('count',\@types,\@types),"\n<P>",
    $query->reset('Clear'), $query->submit('submit','Process File'),
    $query->endform,
);

my($file,$lines,$words,$characters,%stats,@words); 
# Process the form if there is a file name entered
if ($file = $query->param('filename')) {
    $query->print(
        "<HR>\n",
        "<H2>$file</H2>\n",
    );
    while (<$file>) { 
	$lines++;
	$words += @words=split(/\s+/);
	$characters += length($_);
    }
    grep($stats{$_}++,$query->param('count'));
    if (%stats) {
	$query->print("<STRONG>Lines: </STRONG>$lines<BR>\n")
	    if $stats{'count lines'};
	$query->print("<STRONG>Words: </STRONG>$words<BR>\n")
	    if $stats{'count words'};
	$query->print("<STRONG>Characters: </STRONG>$characters<BR>\n")
	    if $stats{'count characters'};
    } else {
	$query->print("<STRONG>No statistics selected.</STRONG>\n");
    }
}

$query->print(<<EOF);
<HR>
<A HREF="../cgi_docs.html">CGI documentation</A>
<HR>
<ADDRESS>
<A HREF="/~lstein">Lincoln D. Stein</A>
</ADDRESS><BR>
Last modified 22 Oct 1995.
EOF
    
$query->print($query->end_html);

