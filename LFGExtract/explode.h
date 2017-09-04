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

/* Extract a file from an archive file and explode it.
   in_fp:           Pointer to imploded data start in archive file.
   out_filename:    Output filename [consider making this fp_out].
   expected_length: Expected length of file (0 if not provided).
   eof_reached():   Callback that indicates archive EOF is reached.
                    Callback should return new file pointer with
                    the continued data for the imploded file.
*/
int extract_and_explode( FILE* in_fp,
                         FILE* out_fp,
                         int   expected_length,
                         bool print_stats,
                         FILE* (*eof_reached)(void) );

#endif /* explode_h */
