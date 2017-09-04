//
//  lfg_pack.h
//  LFGExtract
//
//  Created by Kevin Seltmann on 11/5/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//

/*
 
 LucasFilm Games file archive format:
 ------------------------------- ALL -[8 bytes]---------------------------------
 0000-0003	4	'LFG!'
 0004-0007	4	Length of this part of archive file. Least significant
                byte first.
 ---------------------FIRST ARCHIVE FILE ONLY  ----[20 bytes]-------------------
 0008-0014	13	Archive filename, 0 terminated.
 0015		1	0
 0016		1   Number of disks (archive files) that make up the total archive.
 0017		1   0
 0018-001B	4	Total space needed for all expanded files (in bytes), least
                significant byte first.
 ---------------- FILE DATA, REPEAT FOR EACH FILE -[32 bytes]-------------------
            4	'FILE'
            4	Length of data (in bytes) that follows for the compressed file,
                including headers, until next 'FILE' marker or end of archive.
                Least significant byte first.
            13	File name, zero term
            1	0
            4	Final length of expanded file (in bytes).  Least significant
                byte first.
            6	Unknown   [2,0,1,0,0,0]  (suspect uint16 2, uint32 1)
            ... Compressed File Data, using PKZ 'implode' ...
 -------------------------------------------------------------------------------
 
 */


#ifndef lfg_pack_h
#define lfg_pack_h

#include <stdio.h>
#include "implode.h"

typedef enum {
    LFG_DICTIONARY_1K = 4,
    LFG_DICTIONARY_2K = 5,
    LFG_DICTIONARY_4K = 6,
    LFG_DEFAULT
} lfg_dictionary_size_type;

int lfg_pack(implode_dictionary_type dictionary_size,
             const char* archive,
             char** file_list,
             int num_files );

#endif /* lfg_pack_h */
