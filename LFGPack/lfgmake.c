//
//  main.c
//  LFGMake
//
//  Created by Kevin Seltmann on 6/17/16.
//  Copyright © 2016,2017 Kevin Seltmann. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "pack_lfg.h"


void print_usage ( void )
{
    printf("\nUsage: LFGMake [options] archive_name archive_file_1 archive_file_2 ... \n");
    printf("Creates an LFG-type archive.\n\n");
    printf("Options:\n");
    //printf("  -i                    Show compression info only (do not create archive)\n");
    printf("  -d N                  Use dictionary size of N k (where N=1,2,4)\n");
    printf("  -f filelist           Use filelist (text file) as archive file list\n");
    printf("  -h                    Display this help\n\n");
    printf("  -l M                  Set literal mode (1: encode; 0: no coding)\n");
    printf("  -m initial_size size  Set max size for first and subsequent archive files\n");
    printf("  -s                    Print stats for each encode\n");
    printf("  -v                    Print version info\n");
}

void print_version ( void )
{
    printf("\nLFGMake V1.1\n");
    printf("(c) Seltmann Software, 2016-2017\n\n");
}

int main (int argc, const char * argv[])
{    
    int file_arg = 1;
    lfg_dictionary_size_type dictionary_size = LFG_DEFAULT;
    const char* file_list = NULL;
    FILE * list_ptr = NULL;
    char buffer[256];
    char* file_list_ptr[256];
    unsigned int disk_size = 4294967295;
    unsigned int first_disk = 4294967295;
    bool verbose = false;
    unsigned int literal_mode = 0;
    unsigned int optimize_level = 3;
    
    for (int j = 1; j<argc; j++)
    {
        if (strcmp(argv[j], "-i") == 0)
        {
            //info_only = true;
            file_arg++;
        }
        else if (strcmp(argv[j], "-l") == 0)
        {
            j++;
            file_arg+=2;
            if (j >= argc)
            {
                print_version();
                return 0;
            }
            literal_mode = atoi(argv[j]);
        }
        else if (strcmp(argv[j], "-o") == 0)
        {
            j++;
            file_arg+=2;
            if (j >= argc)
            {
                print_version();
                return 0;
            }
            optimize_level = atoi(argv[j]);
        }
        else if (strcmp(argv[j], "-f") == 0)
        {
            j++;
            file_arg+=2;
            if (j >= argc)
            {
                print_version();
                return 0;
            }
            file_list = argv[j];
        }
        else if (strcmp(argv[j], "-m") == 0)
        {
            j++;
            file_arg+=3;
            if (j >= argc-1)
            {
                print_version();
                return 0;
            }
            first_disk = atoi(argv[j++]);
            disk_size = atoi(argv[j]);
        }
        else if (strcmp(argv[j], "-v") == 0)
        {
            print_version();
            return 0;
        }
        else if (strcmp(argv[j], "-h") == 0)
        {
            print_usage();
            return 0;
        }
        else if (strcmp(argv[j], "-s") == 0)
        {
            verbose = true;
            file_arg++;
        }
        else if (strcmp(argv[j], "-d") == 0)
        {
            file_arg+=2;
            j++;
            
            if (j<argc)
            {
                int value;
                value = atoi(argv[j]);
                switch ( value )
                {
                    case 1:
                        dictionary_size = LFG_DICTIONARY_1K;
                        break;
                        
                    case 2:
                        dictionary_size = LFG_DICTIONARY_2K;
                        break;
                        
                    case 4:
                        dictionary_size = LFG_DICTIONARY_4K;
                        break;
                        
                    default:
                        print_usage();
                        return 0;
                }
            }
        }
    }
    
    if (file_arg >= argc)
    {
        print_usage();
        return 0;
    }
    
    int file_count = 0;
    
    // if read from file
    if (file_list != NULL)
    {
        list_ptr = fopen(file_list, "r");
        
        if (!list_ptr)
        {
            printf("%s not found!\n", file_list);
            return 0;
        }
        
        while ( fgets (buffer, 255, list_ptr)!=NULL )
        {
            buffer[strcspn(buffer, "\r\n")] = 0;
            long length = strlen( buffer );
            if (length)
            {
                file_list_ptr[file_count] = malloc( sizeof(char) * length + 1);
                strcpy(file_list_ptr[file_count], buffer);
                file_count++;
            }
        }
        
        fclose(list_ptr);
    }
    else
    {
        for (int i=file_arg+1; i<argc; i++)
        {
            long length = strlen( argv[i] );
            file_list_ptr[file_count] = malloc( sizeof(char) * length + 1);
            strcpy(file_list_ptr[file_count], argv[i]);
            file_count++;
        }
    }
    
    //if -debug
    //printf("LFGMake will compress the following files:\n");
    //for (int i = 0; i< file_count; i++)
    //{
    //    printf(" %d: %s\n", i+1, file_list_ptr[i]);
    //}
    
    pack_lfg(dictionary_size,
             literal_mode,
             argv[file_arg],
             file_list_ptr,
             file_count,
             first_disk,
             disk_size,
             optimize_level,
             verbose);
    
    // Free file list
    for (int i=0; i< file_count; i++)
    {
        free(file_list_ptr[i]);
    }
    
    return 0;
}

