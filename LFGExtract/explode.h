//
//  explode.h
//  LFGExtract V 1.1
//
//  Created by Kevin Seltmann on 6/12/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//
//  Designed to extract the archiving used on LucasArts Classic Adventure
//  install files (*.XXX) and possibly other archives created with the PKWARE
//  Data Compression Library from ~1990.  Implementation based on
//  specification found on the internet
//  Tested on all LucasArts Classic Adventure games and produces indentical
//  output.

#ifndef explode_h
#define explode_h

#include <stdio.h>

int extract_and_explode( FILE* in_fp,
                         char* out_filename,
                         int expected_length,
                         FILE* (*eof_reached)(void));

#endif /* explode_h */
