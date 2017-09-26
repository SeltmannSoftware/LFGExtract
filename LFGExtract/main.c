//
//  main.c
//  LFGExtract V 1.1
//
//  Created by Kevin Seltmann on 6/11/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "extract.h"

void print_usage ( void )
{
    printf("Usage: LFGExtract [-i] [-s] archive_name(s)\n");
    printf("Extracts files from archives used in older "
           "LucasArts games.\n\n");
    printf("   -d    Display process details\n");
    printf("   -f    Force overwrite of existing files during extraction.\n");
    printf("   -i    Show archive info only (do not extract)\n");
    printf("   -o    Extract to directory\n");
    printf("   -s    Display file stats\n");
    printf("   -v    Display version info\n\n");
}

void print_version ( void )
{
    printf("LFGExtract V1.3\n");
    printf("(c) Seltmann Software, 2016-2017\n\n");
}

int main (int argc, const char * argv[])
{
    int verbose = 1;
    bool info_only = false;
    bool show_stats = false;
    bool overwrite = false;
    int file_arg = 1;
    const char* output_dir = NULL;
    
    for (int j = 1; j<argc; j++)
    {
        if (strcmp(argv[j], "-i") == 0)
        {
            info_only = true;
            file_arg++;
        }
        else if (strcmp(argv[j], "-d") == 0)
        {
            verbose = 2;
            file_arg++;
        }
        else if (strcmp(argv[j], "-s") == 0)
        {
            show_stats = true;
            file_arg++;
        }
        else if (strcmp(argv[j], "-f") == 0)
        {
            overwrite = true;
            file_arg++;
        }
        else if (strcmp(argv[j], "-o") == 0)
        {
            j++;
            file_arg+=2;
            if (j<argc)
                output_dir = argv[j];
         }
        else if (strcmp(argv[j], "-v") == 0)
        {
            print_version();
            return 0;
        }
        else if (argv[j][0] == '-')
        {
            printf("Argument not recognized.\n");
            print_usage();
            return 0;
        }
        else
        {
            file_arg = j;
            break;
        }
    }
    
    if (file_arg >= argc)
    {
        print_usage();
        return 0;
    }
    
    while (file_arg < argc)
    {
        int result;
        
        result = extract_archive(argc - file_arg, &argv[file_arg],
                                 info_only, show_stats, verbose,
                                 overwrite, output_dir);
        
        if (result <= 0)
          result = 1;       // Extract failed, move to next file.
        
        file_arg+=result;
    }
    
    return 0;
}

