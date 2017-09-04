//
//  implode.h
//  LFGMake
//
//  Created by Kevin Seltmann on 6/17/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//

#ifndef implode_h
#define implode_h

#include <stdio.h>

typedef enum {
    DICTIONARY_1K_SIZE = 4,
    DICTIONARY_2K_SIZE = 5,
    DICTIONARY_4K_SIZE = 6
} implode_dictionary_type;

unsigned int implode( FILE * in_file,
                      FILE * out_file,
                      unsigned int length,
                      implode_dictionary_type dictionary_size,
                      unsigned int initial_max_length,
                      unsigned int next_max_length,
                      FILE* (*max_reached)( FILE*) );

#endif /* implode_h */
