//
//  lfg_pack.c
//  LFGMake
//
//  Created by Kevin Seltmann on 11/5/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "implode.h"
#include "lfg_pack.h"

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

bool verbose = true;
int file_count = 0;

char* lfg_string = "LFG!";
char* file_string = "FILE";
char archive_name[14]={0};
long length_location;
long space_needed_location;
long compressed_length_location;
long disk_count_location;

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


FILE *fp_out;

FILE* max_reached (FILE* current_file, unsigned int * max_length )
{

    // Calculate archive length and fill in
    unsigned int archive_length = (unsigned int)(ftell(current_file) - 8);
    fseek(current_file, length_location, SEEK_SET);
    write_le_word(archive_length, current_file);
    printf("--Archive length: %d\n", archive_length);
    
    // Close archive.
    if ((current_file != fp_first ) && (current_file != fp_current_file_start))
        fclose(current_file);
    
    *max_length = next_disk_size;
    disk_count++;

    // Take current archive filename. Add one to 5th char from end.
    // Open new file.
 
    // Create archive
    archive_name[7]++;
    current_file=fopen(archive_name, "wb+");
    
    if (current_file== 0)
    {
        printf("Error creating file %s for archive.\n\n", archive_name);
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


int lfg_pack(implode_dictionary_type dictionary_size,
             const char* archive,
             char** file_list,
             int num_files )
{
    
    FILE *fp_in = NULL;
    FILE *fp_out;
    int file_num = 0;
    long length;
    unsigned int bytes_needed = 0;
    unsigned int archive_length = 0;
    char file_name[14] = {0};
    long compressed_length_location;
    unsigned int bytes_written;
    int dictionary_bytes = 1 << (dictionary_size + 6);
    int file_count = 0;
    unsigned int space_left = 4294967295;
    disk_count = 0;
    
    // Profiling
    clock_t start, stop;
    
    // Create archive
    fp_out=fopen(archive, "wb+");
    
   // exists?
    
    if (fp_out== 0)
    {
        printf("Error creating file %s for archive.\n\n", archive);
        return -1;
    }
    
    strncpy( archive_name, archive, 13);
    
    // Write out "LFG!" tag
    fwrite( lfg_string, sizeof(unsigned char), 4, fp_out);
    
    
    // Placeholder for length of this archive.
    length_location = ftell(fp_out);
    write_le_word(0, fp_out);

    printf("Length loc: %ld\n", length_location);
    
    // Write archive name (13 chars including null)
    fwrite( archive_name, sizeof(unsigned char), 13, fp_out);
    
    fputc( 0, fp_out);
    fputc( 1, fp_out); //disks
    fputc( 0, fp_out);
    
    space_needed_location = ftell(fp_out);
    write_le_word(0, fp_out);
    
    printf("Imploding file(s) and creating archive %s.\n\n", archive_name);
    printf("Filename       Dictionary Size   Original Size    Imploded Size  Savings\n" );
    printf("--------------------------------------------------------------------------\n" );
    
    while (file_num < num_files) {
        
        // Open for binary read
        fp_in=fopen(file_list[file_num], "rb");
        
        printf("  %-12s ", file_list[file_num]);
        
        if (fp_in == 0)
        {
            printf("Error opening file %s.\n\n", file_list[file_num]);
            return -1;
        }
        
        // Find file length
        fseek ( fp_in, 0, SEEK_END );
        length = ftell( fp_in );
        fseek ( fp_in, 0, SEEK_SET );
        
        // Output "FILE" tag
        fwrite( file_string, sizeof(unsigned char), 4, fp_out);
        
        // Remember starting location
        compressed_length_location = ftell(fp_out);
        
        // Write 0 (4 bytes). This will be replaced later
        write_le_word(0, fp_out);
        
        // Write file name (max 13 char including null)
        strncpy( file_name, file_list[file_num++], 13);
        fwrite( file_name, sizeof(unsigned char), 13, fp_out);
        
        // Write 0
        fputc( 0, fp_out);
        
        // Track sum of uncompressed bytes
        bytes_needed += length;
        
        // Write uncompressed length (4 bytes)
        write_le_word( (unsigned int)length, fp_out);
        
        fputc( 2, fp_out);
        fputc( 0, fp_out);
        write_le_word(1, fp_out);  // 1, 0, 0, 0
        
        // Time implode operation
        start = clock();
        
        bytes_written = implode(fp_in,
                                fp_out,
                                (unsigned int)length,
                                dictionary_size,
                                &space_left,
                                max_reached);
        file_count++;
        stop = clock();
        
        fclose(fp_in);
        
        // Fill in compressed file length
        fseek(fp_out, compressed_length_location, SEEK_SET);
        bytes_written += 24;
        write_le_word(bytes_written, fp_out);
        
        // Move back to end of file
        fseek ( fp_out, 0, SEEK_END );
        
        printf("     %4d bytes  %8ld bytes   %8d bytes %7.2f\%%\n",
               (int)dictionary_bytes,
               length, bytes_written, 100-(float)(bytes_written * 100) /
               length);
        
        printf("  Implode took %f seconds.\n",
               (double)(stop - start) / CLOCKS_PER_SEC);
        
    }
    
    printf("Total files: %d\n", file_count);

    // Calculate archive length and fill in
    archive_length = (unsigned int)(ftell(fp_out) - 8);
    fseek(fp_out, length_location, SEEK_SET);
    write_le_word(archive_length, fp_out);

    // Fill in the overall bytes needed
    fseek(fp_out, space_needed_location, SEEK_SET);
    write_le_word(bytes_needed, fp_out);
    
    printf("--Archive length: %d\n", archive_length);
    printf("--Archive Total length: %d\n", bytes_needed);
    
    fclose(fp_out);

    return 0;
}


int lfg_pack_disks(implode_dictionary_type dictionary_size,
                   const char* archive,
                   char** file_list,
                   int num_files,
                   int first_disk_size,
                   int disk_size)
{
    
    FILE *fp_in = NULL;
    int file_num = 0;
    long length;
    unsigned int bytes_needed = 0;
    unsigned int archive_length = 0;

    char file_name[14] = {0};
    long compressed_length_location;
    unsigned int bytes_written;
    int dictionary_bytes = 1 << (dictionary_size + 6);
    int file_count = 0;
    unsigned int space_left = first_disk_size-1;
    next_disk_size = disk_size-1;
    disk_count = 1;
    
    // Profiling
    clock_t start, stop;
    
    
    // currently archive must be filename only, no path
    // Create archive
    fp_out=fopen(archive, "wb+");
    
    fp_first = fp_out;  // for filling in overall length;
    // exists?
    
    // exists?
    
    if (fp_out== 0)
    {
        printf("Error creating file %s for archive.\n\n", archive);
        return -1;
    }
    
    strncpy( archive_name, archive, 13);
    
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
    
    printf("Imploding file(s) and creating archive %s.\n\n", archive_name);
    printf("Filename       Dictionary Size   Original Size    Imploded Size  Savings\n" );
    printf("--------------------------------------------------------------------------\n" );
    
    space_left-=28; //ftell(fp_out);
    
    while (file_num < num_files) {
        
        // Open for binary read
        fp_in=fopen(file_list[file_num], "rb");
        
        printf("  %-12s ", file_list[file_num]);
        
        if (fp_in == 0)
        {
            printf("Error opening file %s.\n\n", file_list[file_num]);
            return -1;
        }
        
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
        
        // Track sum of uncompressed bytes
        bytes_needed += length;
        
        // Write uncompressed length (4 bytes)
        write_le_word( (unsigned int)length, fp_out);
        
        fputc( 2, fp_out);
        fputc( 0, fp_out);
        write_le_word(1, fp_out);  // 1, 0, 0, 0
        
        space_left-=32;
        
        // Time implode operation
        start = clock();
        
        bytes_written = implode(fp_in,
                                fp_out,
                                (unsigned int)length,
                                dictionary_size,
                                &space_left,
                                max_reached);
        
        file_count++;
        stop = clock();
        
        fclose(fp_in);
        
        // Fill in compressed file length
        // fp_first_
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
        
        printf("     %4d bytes  %8ld bytes   %8d bytes %7.2f\%%\n",
               (int)dictionary_bytes,
               length, bytes_written, 100-(float)(bytes_written * 100) /
               length);
        
        printf("  Implode took %f seconds.\n",
               (double)(stop - start) / CLOCKS_PER_SEC);
        
    }
    
    printf("Total files: %d\n", file_count);
    
    // Calculate archive length and fill in
    archive_length = (unsigned int)(ftell(fp_out) - 8);
    /// fp_start
    fseek(fp_out, length_location, SEEK_SET);
    write_le_word(archive_length, fp_out);
    
    printf("--Archive length: %d\n", archive_length);
    printf("--Disk count: %d\n", disk_count);
    printf("--Archive Total length: %d\n", bytes_needed);
    
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

