#!/user/local/bin/perl

require Apache::CGI;
my $q = new Apache::CGI;

$q->print(
   $q->header,	
   $q->start_html(),	  
   "Can you tell if I've been run under CGI or Apache::Registry?<p>",
   $q->start_form(),
   $q->textfield(-name => "textfield"),
   $q->submit(-value => "Submit"),
   $q->end_form,
   "<p>textfield = ", $q->param("textfield"),
   $q->dump,
   $q->end_html,
);

