#!/usr/bin/perl

# use Data::Dumper;

%Authors = (roessler => 'Thomas Roessler <roessler@does-not-exist.org>',
	    me => 'Michael Elkins <me@mutt.org>');

@Changes = ();
$change = {};

while (<>) {
    chomp $_;
    if ($_ =~ /^Working file: (.*)$/) {
	$workfile = $1;
	$change->{workfile} = $workfile;
	$change->{logmsg}   = '';
	$change->{author}   = '';
	$change->{revision} = '';

#	print STDERR "workfile: ", $workfile, "\n";
	
    } 
    elsif ($_ =~ /^(=======|------)/) {
	if ($change->{revision}) {
	    
	    # Some beautifications: Date, author.
	    if ($change->{author} =~ /^(.*?)\s?\<(.*)\>/) {
		$change->{author} = "$1  <$2>";
	    }
	    $change->{date} =~ s!/!-!g;
	    
	    # Record the change.
	    push @Changes, $change unless $change->{logmsg} =~ /^#/;
	    $change = {};
	    $change->{workfile} = $workfile;
	    $change->{logmsg}   = '';
	    $change->{author}   = '';
	}
    } elsif ($_ =~ /^revision ([0-9.]*)/) {
	$change->{revision} = $1;
    } elsif ($_ =~ /^date: ([^; ]*) ([^; ]*);  author: ([^;]*);/) {
	$change->{date} = $1;
	$change->{hour} = $2;
	$change->{author} = $Authors{$3} ? $Authors{$3} : $3;
    } elsif ($_ =~ /^From: (.*)$/) {
	$change->{author} = $1;
    } elsif ($change->{revision}) {
	if ($change->{logmsg} || $_) {
	    $change->{logmsg} .= $_ . "\n";
	}
    }
}

# print Dumper @Changes;

undef $last_logmsg; undef $last_author; undef $last_date; undef $last_hour;
$files = [];

for my $k (sort {($b->{date} cmp $a->{date}) || ($b->{hour} cmp $a->{hour}) || ($a->{author} cmp $b->{author}) || ($a->{workfile} cmp $b->{workfile})} @Changes) {
    
    if (!($last_date eq $k->{date}) || !($last_author eq $k->{author})) {
	if (@$files) {
	    &print_entry ($files, $last_logmsg);
	    $files = [];
	}
	&print_header ($k->{author}, $k->{date}, $k->{hour});
    }
 
    if (@$files && !($last_logmsg eq $k->{logmsg})) {
	&print_entry ($files, $last_logmsg);
	$files = [];
    }
      
    
    $last_logmsg = $k->{logmsg};
    $last_author = $k->{author};
    $last_date   = $k->{date};
    $last_hour   = $k->{hour};
    
    push @$files, $k->{workfile};
}

if (@$files) {
    &print_entry ($files, $last_logmsg);
}

sub print_header {
    my $author = shift;
    my $date = shift;
    my $hour = shift;
    
    print $date, " ", $hour, "  ", $author, "\n\n";
}

sub print_entry  {
    my $files = shift;
    my $logmsg = shift;
    

    print "\t* ";
    my $c = '';
    
    for my $f (@$files) {
	print $c, $f;
	$c = ', ' unless $c;
    }
    
    print ": ";
    
    my $t = '';
    for my $l (split ('\n', $logmsg)) {
	print $t, $l, "\n";
	$t = "\t" unless $t;
    }
    print "\n";
}
