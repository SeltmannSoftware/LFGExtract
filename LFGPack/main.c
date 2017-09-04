//
//  main.c
//  LFGMake
//
//  Created by Kevin Seltmann on 6/17/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lfg_pack.h"
#include "implode.h"
#include "explode.h"

void print_usage ( void )
{
    printf("\nUsage: LFGMake [options] archive_name archive_file_1 archive_file_2 ... \n");
    printf("Creates an LFG-type archive.\n\n");
    printf("Options:\n");
    //printf("   -i    Show compression info only (do not create archive)\n");
    printf("   -d N  Use dictionary size of N k (where N=1,2,4)\n");
    printf("   -v    Print version info\n");
    printf("   -h    Display this help\n\n");
}

void print_version ( void )
{
    printf("\nLFGMake V1.0\n");
    printf("by Kevin Seltmann, 2016\n\n");
}

int main (int argc, const char * argv[])
{
    
    int file_arg = 1;
    int dictionary_bytes = 4096;
    implode_dictionary_type dictionary_size = DICTIONARY_4K_SIZE;
    const char* file_list = NULL;
    FILE * list_ptr = NULL;
    char buffer[256];
    char* file_list_ptr[256];
    

    for (int j = 1; j<argc; j++)
    {
        if (strcmp(argv[j], "-i") == 0)
        {
            //info_only = true;
            file_arg++;
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
                        dictionary_size = DICTIONARY_1K_SIZE;
                        dictionary_bytes = 1024;
                        break;
                        
                    case 2:
                        dictionary_size = DICTIONARY_2K_SIZE;
                        dictionary_bytes = 2048;
                        break;
                        
                    case 4:
                        dictionary_size = DICTIONARY_4K_SIZE;
                        dictionary_bytes = 4096;
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
        
        while ( fgets (buffer, 255, list_ptr)!=NULL )
        {
            buffer[strcspn(buffer, "\r\n")] = 0;
            long length = strlen( buffer );
            file_list_ptr[file_count] = malloc( sizeof(char) * length + 1);
            strcpy(file_list_ptr[file_count], buffer);
            file_count++;
        }
        
        fclose(list_ptr);
    }
    else
    {
        printf("%d %d\n", file_arg, argc);
        for (int i=file_arg+1; i<argc; i++)
        {
            long length = strlen( argv[i] );
            file_list_ptr[file_count] = malloc( sizeof(char) * length + 1);
            strcpy(file_list_ptr[file_count], argv[i]);
            file_count++;
        }
    }
    
    printf("file count %d\n", file_count);
    for (int i = 0; i< file_count; i++)
    {
        printf(" %d: %s\n", i, file_list_ptr[i]);
    }
    
    lfg_pack(dictionary_size,
             argv[file_arg],
             file_list_ptr,
             file_count);

    for (int i=0; i< file_count; i++)
    {
        free(file_list_ptr[i]);
    }
 
    
    /* verify by exploding what was just imploded
    fp_in=fopen(argv[file_arg], "rb");
    
    
    extract_and_explode( fp_in,
                         temp,
                            0,
                        NULL);
    
    
    fclose(fp_in);
    */
    
    return 0;
}

