#!/usr/bin/perl

use warnings;
use strict;

# for giving a % complete status on fsl bedpost logs
# expect data directory name as parameter

@ARGV == 1 or die "Usage: $0 <data directory>\n";
my ($datadir) = @ARGV;

my $fsldir = $ENV{'FSLDIR'};

my $nslices = `"$fsldir"/bin/fslval "$datadir"/data dim3`;
$nslices or die "$0 cannot determine number of slices\n";
print "number of slices $nslices";
exit;
my @lognums = (0 .. $nslices - 1);
my @lognames;
foreach my $lognum (@lognums) {
  $lognum =~ /^\d$/ and $lognum = "000$lognum";
  $lognum =~ /^\d\d$/ and $lognum = "00$lognum";
  $lognum =~ /^\d\d\d$/ and $lognum = "0$lognum";
  push @lognames, "log$lognum";
} 

my $logdir = "$datadir.bedpostX/logs";
my %complete;

my $alldone = 0;
while (!$alldone) {
  foreach my $log (@lognames) {
    # seems that log0000 is not being filled sometimes, hack fix
    if ($log eq "log0000") {
      $complete{$log} = 100;
      next;
    }

    my $longlog = "$logdir/$log";
    if (!open LOG, "<", $longlog) {
      $complete{$log} = 0;
      next;
    }
    my @lines = <LOG>;
    close LOG;

    my ($numer, $denom) = $lines[$#lines] =~ /\d+/g;
    if (!$denom) {
      $complete{$log} = 0;
      next;
    }
    my $percent = $numer / $denom * 100;
    $complete{$log} = $percent;
  }

  print "\033[2J"; # clear the screen
  print "\033[0;0H"; # jump to 0,0
  $alldone = 1;
  my $total = 0;
  foreach my $l (sort keys %complete) {
    $total += $complete{$l};
    $complete{$l} == 100 or $alldone = 0;
    if ($complete{$l} > 0 and $complete{$l} < 100) {
      my $slice = $l;
      $slice =~ s/log0*//; 
      printf "Slice %2s is %3.0f%% done\n", $slice, $complete{$l};
    }
  }
  $total /= $nslices;
  printf "\nTotal %9.1f%% done\n", $total;

  # sometimes stalls on last log, so check if eye.mat is present
  if (-f "$datadir.bedpostX/xfms/eye.mat" ) {
    $alldone = 1;
  }

  if ($alldone) {
    if (-f "$datadir.bedpostX/xfms/eye.mat" ) {
      print "$datadir bedpost complete\n";
      exit 0;
    }
    else {
      print "waiting for index transform to be written\n";
      $alldone = 0;
    }
  }
  sleep 5;
}

exit 0;
