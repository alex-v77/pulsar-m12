#!/usr/bin/perl

use strict;

while(<>) {
    if($_ =~ /^From /) {
        print;
    } else {
        s/[a-z,0-9]/x/g;
        s/[A-Z]/X/g;
        print;
    }
    
}
