//
//  explode.h
//  LFGExtract V 1.0
//
//  Created by Kevin Seltmann on 6/12/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//

#ifndef explode_h
#define explode_h

#include <stdio.h>

int extract_and_explode( FILE* in_fp,
                         char* out_filename,
                         int expected_length,
                         FILE* (*eof_reached)(void));

#endif /* explode_h */
