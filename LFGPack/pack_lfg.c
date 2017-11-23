//
//  pack_lfg.c
//  LFGMake
//
//  Created by Kevin Seltmann on 11/5/16.
//  Copyright © 2016, 2017 Kevin Seltmann. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "implode.h"
#include "pack_lfg.h"

typedef struct
{
    long file_length;
    char filename[13];
    uint32_t length;
    char num_disks;
    uint32_t space_needed;
    FILE * fp;
} archive_info_type;

archive_info_type archive_info = {0};

typedef struct
{
    uint32_t length;
    char filename[13];
    uint32_t final_length;
} file_info_type;

file_info_type file_info = {0};

int file_count = 0;

char* lfg_string = "LFG!";
char* file_string = "FILE";
char archive_name[14]={0};
long length_location;
long space_needed_location;
long compressed_length_location;
long disk_count_location;
char full_archive_path[256];

FILE * fp_first;
FILE * fp_current_file_start;

struct
{
    int file_count;
    int bytes_written_so_far;
    int bytes_read_so_far;
    int current_disk;
    char cur_filename[256];
} disk_info = {0};

void write_le_word( unsigned int value, FILE* file)
{
    unsigned char buffer[4];
    
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
    
    fwrite(buffer, sizeof(unsigned char), 4, file);
}

int next_disk_size;
int disk_count;
unsigned int archive_total_length;
FILE *fp_out;


void remove_path(char* filename,
                 const char* pathfile,
                 int max_length)
{
    // Extract filename from path.
    // Start at end and move backwards until hit
    // '/' or '\'
    unsigned long length = strlen(pathfile);
    int a;
    
    for (a= (int)length-1; a>=0; a--)
    {
        if ((pathfile[a] == '/') || (pathfile[a] == '\\'))
            break;
    }
    
    strncpy(filename, &pathfile[a+1], max_length);
    
}


void find_best_implode( FILE * in_file,
                     unsigned int length,
                     unsigned int * literal_encode_mode,
                     implode_dictionary_type *dictionary_size,
                     unsigned int *optimization_level)
{
    // Profiling
    clock_t start, stop;
    implode_stats_type best_implode_stats;
    unsigned int bytes_written;
    int i,j,k;
    unsigned int min_size = 0xFFFFFFFF;
    
    for (i=0; i<2;i++)
    {
        for(j=4;j<7;j++)
        {
            for (k=1; k<4;k+=2)
            {
                // Time implode operation
                start = clock();

                bytes_written = implode(in_file,
                                        NULL,
                                        length,
                                        i,
                                        j,
                                        k,
                                        &best_implode_stats,
                                        NULL,
                                        NULL);
                fseek ( in_file, 0, SEEK_SET );
                
                stop = clock();
                
                if (bytes_written < min_size)
                {
                   
                    min_size = bytes_written;
                    *literal_encode_mode = i;
                    *dictionary_size = j;
                    *optimization_level = k;
                }
            }
        }
    }
}
                     

FILE* max_reached (FILE* current_file, unsigned int * max_length )
{
    // Calculate archive length and fill in
    unsigned int archive_length = (unsigned int)(ftell(current_file) - 8);
    fseek(current_file, length_location, SEEK_SET);
    write_le_word(archive_length, current_file);
    
    archive_total_length += archive_length + 8;
    
    //printf("Archive %s created. Length: %d bytes\n",
    //       archive_name, archive_length);
    
    // Close archive.
    if ((current_file != fp_first ) && (current_file != fp_current_file_start))
        fclose(current_file);
    
    *max_length = next_disk_size;
    disk_count++;

    // Take current archive filename. Add one to 5th char from end.
    // Open new file.
 
    // Create archive ...REDO
    full_archive_path[strnlen(full_archive_path,256)-5]++;
    //archive_name[7]++;
    current_file=fopen(full_archive_path, "wb+");
    
    if (current_file== 0)
    {
        printf("Error creating file %s for archive.\n\n", full_archive_path);
        return NULL;
    }
    
    // Write LFG!, placeholder for length.
    // Write out "LFG!" tag
    fwrite( lfg_string, sizeof(unsigned char), 4, current_file);
    
    // Placeholder for length of this archive.
    length_location = ftell(current_file);
    write_le_word(0, current_file);

    fp_out=current_file;
    
    *max_length-=8; //ftell(fp_out);
    
    return current_file;
}


int pack_lfg(lfg_dictionary_size_type dictionary_size,
             unsigned int literal_mode,
             const char* archive,
             char** file_list,
             int num_files,
             unsigned int first_disk_size,
             unsigned int disk_size,
             unsigned int optimize_level,
             bool verbose)
{
    
    FILE *fp_in = NULL;
    int file_num = 0;
    long length;
    unsigned int bytes_needed = 0;
    unsigned int archive_length = 0;

    char file_name[14] = {0};
    long compressed_length_location;
    unsigned int bytes_written;
    int file_count = 0;
    unsigned int space_left = first_disk_size;
    next_disk_size = disk_size;
    disk_count = 1;
    implode_dictionary_type dictionary_val;
    implode_stats_type implode_stats;
    unsigned int optimization_level;
    char filename[14] = {0};
    
    if (strlen(archive)>256)
    {
        printf("\nError. Input path too long.\n");
        return -1;
    }
    
    strncpy(full_archive_path, archive, 256);
    
    // Profiling
    clock_t start, stop;
    
    // currently archive must be filename only, no path
    // Create archive
    fp_out=fopen(archive, "wb+");
    
    fp_first = fp_out;  // for filling in overall length;
    // exists?
    
    if (fp_out== 0)
    {
        printf("Error creating file %s for archive.\n\n", archive);
        //free(full_archive_path);
        return -1;
    }
    
    remove_path(archive_name, full_archive_path, 13);
    //strncpy( archive_name, archive, 13);
    
    // Write out "LFG!" tag
    fwrite( lfg_string, sizeof(unsigned char), 4, fp_out);
    
    // Placeholder for length of this archive.
    length_location = ftell(fp_out);
    write_le_word(0, fp_out);
    
    // --
    
    // Write archive name (13 chars including null)
    fwrite( archive_name, sizeof(unsigned char), 13, fp_out);
    
    fputc( 0, fp_out);
    disk_count_location = ftell(fp_out);
    fputc( 1, fp_out); //disk_count
    fputc( 0, fp_out);
    
    space_needed_location = ftell(fp_out);
    write_le_word(0, fp_out);
    
    printf("\nImploding file(s) and creating archive %s...\n\n", archive_name);
    printf("                    Archived       Original             ");
    printf("Literal   Dictionary" );
    
    if (verbose)
        printf("   Literal  Dictionary      Min/Max     Min/Max     Elapsed  Optimization");
    
    printf("\n  Filename          size (B)       size (B)     Ratio   ");
    printf("   mode         size");
    
    if (verbose)
        printf("     count     lookups       offset      length    time (s)         level");
    
    printf("\n------------------------------------------------------------------------------");
    
    if (verbose)
        printf("-------------------------------------------------------------------------");
    
    printf("\n");
    
    // Account for initial archive header
    space_left-=28;
    
    while (file_num < num_files) {
        
        if (strlen(file_list[file_num])==0)
        {
            continue;
        }
        
        // Open for binary read
        fp_in=fopen(file_list[file_num], "rb");
        
        if (fp_in == 0)
        {
            printf("Error opening file %s.\n\n", file_list[file_num]);
            //free(full_archive_path);
            return -1;
        }
 
        //Remove path
        remove_path(filename, file_list[file_num], 13);
        
        printf("  %-13s", filename); //file_list[file_num]);
        
        // Find file length
        fseek ( fp_in, 0, SEEK_END );
        length = ftell( fp_in );
        fseek ( fp_in, 0, SEEK_SET );
        
        // Output "FILE" tag
        fwrite( file_string, sizeof(unsigned char), 4, fp_out);
        
        // Remember starting location
        compressed_length_location = ftell(fp_out);
        fp_current_file_start = fp_out;
        
        // Write 0 (4 bytes). This will be replaced later
        write_le_word(0, fp_out);
        
        // Write file name (max 13 char including null)
        strncpy( file_name, file_list[file_num++], 13);
        fwrite( file_name, sizeof(unsigned char), 13, fp_out);
        
        // Write 0
        fputc( 0, fp_out);
        
        // Write uncompressed length (4 bytes)
        write_le_word( (unsigned int)length, fp_out);
        
        // Unknown meaning. Value doesn't seem to matter to install.
        fputc( 2, fp_out);  //2
        fputc( 0, fp_out);
        write_le_word(1, fp_out);  // 1, 0, 0, 0

        // Track sum of uncompressed bytes
        bytes_needed += length;
        
        // Account for header
        space_left-=32;
        
        if (dictionary_size == LFG_DEFAULT)
        {
            if (length <= 1024)
            {
                dictionary_val = DICTIONARY_1K_SIZE;
            }
            else if (length <=2048)
            {
                dictionary_val = DICTIONARY_2K_SIZE;
            }
            else
            {
                dictionary_val = DICTIONARY_4K_SIZE;
            }
        }
        else
        {
            dictionary_val = (implode_dictionary_type) dictionary_size;
        }
        
        if (optimize_level==5)
        {
            find_best_implode( fp_in,
                              (unsigned int)length,
                              &literal_mode,
                              &dictionary_val,
                              &optimization_level);
        }
        else
        {
            optimization_level = optimize_level;
        }
        
        // Time implode operation
        start = clock();
        
        bytes_written = implode(fp_in,
                                fp_out,
                                (unsigned int)length,
                                literal_mode,
                                dictionary_val,
                                optimization_level,
                                &implode_stats,
                                &space_left,
                                max_reached);
        

        stop = clock();
        
        file_count++;
        
        fclose(fp_in);
        
        // Fill in compressed file length
        fseek(fp_current_file_start, compressed_length_location, SEEK_SET);
        bytes_written += 24;
        write_le_word(bytes_written, fp_current_file_start);
        if ((fp_current_file_start != fp_out) &&
            (fp_current_file_start != fp_first))
        {
            fclose(fp_current_file_start);
        }
        
        // Move back to end of file
        fseek ( fp_out, 0, SEEK_END );

        printf("   %10d ",  bytes_written+8);
        printf("    %10ld ", length);
        printf("%8.2f\%%", 100-(float)((bytes_written+8) * 100) / length);

        printf("         %d           %dK", literal_mode,
               1 << (dictionary_val - 4));
            
        if (verbose )
        {
            printf("%10d  %10d",
                   implode_stats.literal_count, implode_stats.dictionary_count);
                
            if (implode_stats.dictionary_count!=0)
            {
                printf("     %2d, %4d     %2d, %3d",
                       implode_stats.min_offset, implode_stats.max_offset,
                       implode_stats.min_length, implode_stats.max_length);
            }
            else
            {
                printf("          N/A         N/A");
            }
            printf("     %7.3f", (double)(stop - start) / CLOCKS_PER_SEC);
            printf("             %d", optimization_level);
        }
        printf("\n");
    }
    
    // Calculate archive length and fill in
    archive_length = (unsigned int)(ftell(fp_out) - 8);
    /// fp_start
    fseek(fp_out, length_location, SEEK_SET);
    write_le_word(archive_length, fp_out);
    
    archive_total_length += archive_length + 8;

    printf("------------------------------------------------------------------------------" );
    if (verbose)
        printf("-------------------------------------------------------------------------");
    printf("\n                  %10d     %10d  %7.2f\%%\n",
           archive_total_length, bytes_needed, 100-(float)(archive_total_length * 100) /
           bytes_needed);
    printf("Packed %d files onto %d disk file",
        file_count, disk_count);
    if (disk_count > 1) printf ("s");
    printf(".\n");

    // Fill in the disk
    fseek(fp_first, disk_count_location, SEEK_SET);
    fputc( (char)(disk_count & 0xFF), fp_first);
    
    // Fill in the overall bytes needed
    fseek(fp_first, space_needed_location, SEEK_SET);
    write_le_word(bytes_needed, fp_first);
    fclose(fp_first);
    
    if (fp_out) fclose(fp_out);
    if (fp_current_file_start) fclose(fp_current_file_start);
    
    return 0;
}
