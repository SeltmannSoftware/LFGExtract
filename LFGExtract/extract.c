//
//  extract.c
//  LFGExtract
//
//  Created by Kevin Seltmann on 10/23/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//
//  Designed to extract the archiving used on LucasArts Classic Adventure
//  install files (*.XXX) and possibly other archives created with the PKWARE
//  Data Compression Library from ~1990.  Implementation for LFG file
//  extraction reverse-engineered from existing .XXX files.  Implementation
//  of explode algorithm based on specifications found on the internet.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "explode.h"
#include "extract.h"

// ----

// Check that first four bytes of file are 'LFG!'
// File pointer is set at 4 bytes from start of file.
bool isFileLFG( FILE *fp_in ) {
    
    char header[4];
    
    fseek ( fp_in, 0, SEEK_SET );
    
    if ( fread( header, sizeof header[0], 0x4, fp_in) == 0x4 ) {
        if( memcmp ( header, "LFG!", 4 ) == 0 ) {
            return true;
        }
    }
    
    return false;
}

// Check that next four bytes of file are 'FILE'
// File pointer is advanced 4 bytes
bool isFileNext( FILE *fp_in ) {
    
    char buffer[4];
    
    if ( fread( buffer, sizeof buffer[0], 0x4, fp_in) == 0x4 ) {
        if( memcmp ( buffer, "FILE", 4 ) == 0 ) {
            return true;
        }
    }
    
    return false;
}

// Read in the next four bytes. Treated as a value, stored with least
// significant byte first.
bool read_uint32( FILE *fp_in, uint32_t * result ) {
    unsigned char buffer[4];
    
    if ( fread( buffer, sizeof buffer[0], 0x4, fp_in ) == 0x4 ) {
        *result = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | (buffer[0]);
        return true;
    }
    
    return false;
}

// Read a block of bytes into a buffer
bool read_chunk( FILE *fp_in, char * buffer, int length ) {
    
    if ( fread( buffer, sizeof buffer[0], length, fp_in ) == length ) {
        return true;
    }
    
    return false;
}

// Read one byte, return whether it is expected or not.
bool read_expected_byte(FILE *fp_in,
                        char expectedByte)
{
    
    if (expectedByte == fgetc(fp_in))
    {
        return true;
    }
    return false;
}

// ---

bool open_archive(FILE **fp,
                  char* filename,
                  uint32_t* reported_length,
                  long* actual_length )
{
    FILE *fp_open;
    uint32_t length;
    long file_length;
    
    // Open for binary read
    fp_open=fopen(filename, "rb");
    
    // Open failed...
    if (fp_open == 0) {
        //printf("Error opening file %s.\n\n", filename);
        return false;
    }
    
    // Find file length
    fseek ( fp_open, 0, SEEK_END );
    file_length = ftell( fp_open );
    
    // -- Read LFG Header --
    
    //Check first four bytes
    if (!isFileLFG(fp_open)) {
        printf("%s does not appear to be a LFG archive "
               "('LFG!' tag not found).\n\n",
               filename);
        fclose (fp_open);
        return false;
    }
    
    // Read and check archive length
    if (!read_uint32(fp_open, &length)) {
        printf("%s does not appear to be a valid LFG archive.\n\n",
               filename);
        fclose (fp_open);
        return false;
    }
    
    // Sanity check on length
    if (file_length != length + 8)
    {
        printf("Warning: Actual archive file length (%ld)\n         "
               "does not match indicated length (%d + 8).\n",
               file_length, length);
    }
    
    // Update length field, file length, and file pointer.
    *reported_length = length;
    *actual_length = file_length;
    *fp = fp_open;
    
    return true;
}

// Prints filename, compressed length, uncompressed length, and ratio.
void print_file_info ( char* filename, uint32_t length, uint32_t final_length )
{
    printf("  %-12s\t   %7d bytes",  filename, length);
    printf("        %7d bytes", final_length);
    printf("     %7.2f\%%\n", 100-(float)(length * 100) / final_length);
}

/* --- */

typedef struct             // Archive Data (reported)
{
    long file_length;      // Length of archive disk/file. **
    char filename[14];     // Reported archive name.
    uint32_t length;       // Length of current archive disk/file.
    char num_disks;        // Number of disks/archive files.
    uint32_t space_needed; // Total bytes after expansion
} archive_info_type;

archive_info_type archive_info = {0};

verbose_level_enum verbose = 0;

struct
{
    int file_count;                 // number of files extracted
    int bytes_written_so_far;       // give/checks final length (add check?)
    int bytes_read_so_far;          // not used
    
    FILE* fp;                       // File pointer to current archive file
    long file_pos;
    
    char cur_filename[256];         // archive path & filename
    unsigned long filename_length;  // length of above
    char* file_name;                // short file name (no path)
    int   file_index;               // index in file list
    int   file_max;                 // entries in file list (rename?)
    const char ** file_list;        // list of archive files
} disk_info = {0};

typedef struct
{
    uint32_t length;           // Compressed length
    char filename[14];         // Name of compressed file
    uint32_t final_length;     // Uncompressed length
} file_info_type;

file_info_type file_info = {0};

// Used as a callback function.
// Closes old file pointer, opens next file in archive.
// First tries incrementing last letter in filename, ie
// INDY___C.XXX -> INDY___D.XXX
// If that fails, uses next filename in supplied list.
FILE* new_file(void)
{
    fclose(disk_info.fp);
    
    if (disk_info.file_pos >= archive_info.file_length)
    {
        disk_info.file_pos -= archive_info.file_length;
        disk_info.file_pos += 8;
        archive_info.num_disks--;
    }
    
    // Probably should only do this if last chars are "XXX"
    disk_info.cur_filename[disk_info.filename_length-5]++;
    
    if (!open_archive(&disk_info.fp, disk_info.cur_filename,
                      &archive_info.length, &archive_info.file_length))
    {
        
        // Try next file instead.  A little wonky with filelist.
        if (disk_info.file_index+1 < disk_info.file_max)
        {
            
            disk_info.filename_length = strlen(disk_info.
                                               file_list[disk_info.
                                                         file_index+1]);
            
            if (disk_info.filename_length < 256)
            {
                strcpy(disk_info.cur_filename,
                       disk_info.file_list[disk_info.file_index+1]);
            }
            else
            {
                printf("Error: Continued file not found. "
                       "Extraction incomplete.\n");
                return NULL;
            }
            
            disk_info.file_name = strrchr(disk_info.cur_filename, '/');
            if (!disk_info.file_name)
            {
                disk_info.file_name = strrchr(disk_info.cur_filename, '\\');
                if (!disk_info.file_name)
                    disk_info.file_name = disk_info.cur_filename;
                else
                    disk_info.file_name++;
            }
            else
                disk_info.file_name++;
            
            if (!open_archive(&disk_info.fp, disk_info.cur_filename,
                              &archive_info.length, &archive_info.file_length))
            {
                printf("Error: Continued file not found. "
                       "Extraction incomplete.\n");
                return NULL;
            }
            
            disk_info.file_index++;
        }
        else
        {
            printf("Error: Continued file not found. "
                   "Extraction incomplete.\n");
            return NULL;
        }
    }
    
    if (verbose == VERBOSE_LEVEL_HIGH)
        printf("\n%s\t   %7ld bytes\n", disk_info.file_name,
               archive_info.file_length);
    
    if (verbose != VERBOSE_LEVEL_SILENT)
    {
        print_file_info(file_info.filename, file_info.length,
                        file_info.final_length);
    }
    
    return disk_info.fp;
}


int extract_archive(int file_max,
                    const char * file_list[],
                    bool info_only,
                    bool show_stats,
                    verbose_level_enum verbose_level,
                    bool overwrite_flag,
                    const char* output_dir)
{
    verbose = verbose_level;
    int file_index = 0;
    long file_pos = 0;
    bool isNotEnd = true;
    char temp_buff[6];
    int temp_length = 0;
    const char exp_buff[6] = {2,0,1,0,0,0};
    
    disk_info.file_index = file_index;
    disk_info.filename_length = strlen(file_list[disk_info.file_index]);
    disk_info.file_max = file_max;
    disk_info.file_list = file_list;
    
    if (disk_info.filename_length < 256)
    {
        strcpy(disk_info.cur_filename, file_list[disk_info.file_index]);
    }
    else
    {
        return 0;
    }
    
    disk_info.file_name = strrchr(disk_info.cur_filename, '/');
    if (!disk_info.file_name)
    {
        disk_info.file_name = strrchr(disk_info.cur_filename, '\\');
        if (!disk_info.file_name)
            disk_info.file_name = disk_info.cur_filename;
        else
            disk_info.file_name++;
    }
    else
        disk_info.file_name++;
    
    if (!open_archive(&disk_info.fp, disk_info.cur_filename,
                      &archive_info.length, &archive_info.file_length))
    {
        printf("Error opening file %s.\n\n", disk_info.cur_filename);
        return 0;
    }
    
    bool file_error = false;
    
    file_error |= !read_chunk(disk_info.fp, archive_info.filename, 13);
    file_error |= !read_expected_byte(disk_info.fp, 0);
    file_error |= !read_chunk(disk_info.fp, &archive_info.num_disks, 1);
    file_error |= !read_expected_byte(disk_info.fp, 0);
    file_error |= !read_uint32(disk_info.fp, &archive_info.space_needed);
    
    if (file_error)
    {
        printf("%s does not appear to be a valid initial LFG archive.\n\n",
               disk_info.cur_filename);
        fclose (disk_info.fp);
        return 0;
    }
    
    if (archive_info.num_disks == 0)
    {
        printf("Warning: Disk count of 0 indicated. File may be corrupted.\n");
    }
    
    if (verbose != VERBOSE_LEVEL_SILENT)
    {
        printf( "Reported archive name: \t\t\t%s\n", archive_info.filename );
        printf( "Disk count: \t\t\t\t%u\n", archive_info.num_disks );
        printf("Space needed for extraction: \t\t%u bytes\n",
               archive_info.space_needed);
        printf("\n");
        
        if (!info_only)
        {
            if (output_dir)
                printf("Extracting files to %s...\n", output_dir);
            else
                printf( "Extracting files...\n" );
        }
        else
            printf( "Archived file info:\n" );
        
        printf("Filename           Archived Size        "
               "Exploded Size      Savings\n" );
        printf("---------------------------------------"
               "-----------------------------\n" );
        
        if (verbose == VERBOSE_LEVEL_HIGH)
        {
            printf("%s\t   %7ld bytes\n", disk_info.file_name,
                   archive_info.file_length);
        }
    }
    
    while (isNotEnd && isFileNext(disk_info.fp))
    {
        
        file_error |= !read_uint32(disk_info.fp, &file_info.length);
        
        file_pos = ftell(disk_info.fp);
        disk_info.file_pos = file_pos;
        
        // From here down....deal with eof?? (and call next_file()?
        
        file_error |= !read_chunk(disk_info.fp, file_info.filename, 13);
        
        // Meaning of this value unknown.
        file_error |= !read_chunk(disk_info.fp, temp_buff, 1);
        
        file_error |= !read_uint32(disk_info.fp, &file_info.final_length);
        
        // Meaning of value  unknown
        file_error |= !read_chunk(disk_info.fp, temp_buff, 6);
        
        if (file_error)
        {
            printf("Unexpected end of file %s.\n\n", disk_info.cur_filename);
            fclose (disk_info.fp);
            return 0;
        }
        
        //if debug
        //for (int j=0; j<13; j++) {
        //    printf(" %02X", (unsigned char)file_info.filename[j]);
        //}
        //printf (" %02X\n", temp_buff[0]);
        
        if (memcmp(temp_buff, exp_buff, 6) != 0)
        {
            printf("Warning: Unexpected values in header. "
                   "File may be corrupted.\n");
        }
        
        disk_info.file_pos += file_info.length;
        file_pos += file_info.length;
        if ( file_pos <= archive_info.file_length)
        {
            if (verbose != VERBOSE_LEVEL_SILENT)
            {
                print_file_info(file_info.filename, file_info.length,
                                file_info.final_length);
            }
            
            disk_info.file_count++;
            //temp_length = file_info.length;
        }
        else
        {
            if (verbose == VERBOSE_LEVEL_HIGH)
                printf( "  %-12s\t    (incomplete)\n", file_info.filename);
            //          printf( "  %-12s\t    %7ld bytes", file_info.filename,
            //                file_info.length - (file_pos - archive_info.file_length));
            
            //temp_length = (uint32_t)(file_pos - archive_info.file_length);  // test
        }
        
      //  disk_info.bytes_read_so_far += temp_length;
        disk_info.bytes_read_so_far += file_info.length;
        disk_info.bytes_written_so_far  += file_info.final_length;
        
        if ( file_pos >= archive_info.file_length )
        {
            isNotEnd = false;
           // archive_info.num_disks--;
        }
        
        printf("1. file pos %ld, file_length %ld\n", file_pos, archive_info.file_length);
        
        if (!info_only)
        {
            // Output (extracted) file pointer.
            FILE* out_fp;
            
            // Profiling
            clock_t start, stop;
            
            // Use supplied path if exists.
            char* complete_filename;
            long file_length = 0;
            
            if (output_dir)
            {
                file_length = strlen(output_dir) + 1;
            }
            
            file_length += strlen(file_info.filename) + 1;
            complete_filename = malloc(file_length);
            
            if (output_dir)
            {
                strcpy(complete_filename, output_dir);
                strcat(complete_filename, "/");
                strcat(complete_filename, file_info.filename);
            }
            else
            {
                strcpy(complete_filename, file_info.filename);
            }
            

            // Check if file exists. Not handling any race condition
            // in which file is created after check by another process.
            // (Not worth the trouble here at least)
            if ((!overwrite_flag) &&
                ((out_fp = fopen(complete_filename, "r"))))
            {
                fclose(out_fp);
                
                printf("Error: File %s already exists.\n",
                       complete_filename);
                
                return -1;
            }
            
            // Open the output file.
            out_fp=fopen(complete_filename, "wb+");
            if (out_fp == 0)
            {
                printf("Error: Failure while creating file %s.\n",
                       complete_filename);
                return -1;
            }
            
            start = clock();
            
            (void) extract_and_explode( disk_info.fp, out_fp,
                                        file_info.final_length,
                                        show_stats,
                                        &new_file );
            
            stop = clock();
            
            fclose(out_fp);
            free(complete_filename);
            
            if (show_stats)
            {
                printf("    Explode took %f seconds.\n",
                       (double)(stop - start) / CLOCKS_PER_SEC);
                printf("\n");
            }
        }
        
        printf("2. file pos %ld, file_length %ld\n", file_pos, archive_info.file_length);
        
 //       if( file_pos < archive_info.file_length )
        {
        }
        if (info_only && archive_info.num_disks && (file_pos >= archive_info.file_length ))
        {
            (void)new_file();
        }
        fseek(disk_info.fp, disk_info.file_pos, SEEK_SET);
        
        printf("num_disks %d  isnotend %d\n", archive_info.num_disks, (int)isNotEnd);
        
        // check for error in new_file here.(below)
        if (archive_info.num_disks && !isNotEnd)
        {
            isNotEnd = true;
            
            //
            if (verbose)
                printf("\n%s\t   %7ld bytes\n", disk_info.file_name,
                       archive_info.file_length);
            
            disk_info.file_count++;
            
    //        if( 8+temp_length <= archive_info.file_length )
            {
    //            fseek(disk_info.fp, 8+temp_length, SEEK_SET);
            }
    //        else
            {
    //            isNotEnd = false;
            }

        }
    }
    
    if( file_pos < archive_info.file_length ) {
        printf( "Warning: Unexpected end of file data.\n" );
    }
    
    if (verbose != VERBOSE_LEVEL_SILENT)
    {
        printf("---------------------------------"
               "-----------------------------------\n" );
        printf("  %d files\t                        %7d bytes\n",
               disk_info.file_count,
               disk_info.bytes_written_so_far );
        printf ("\n");
    }
    
    fclose(disk_info.fp);
    
    return ++disk_info.file_index;
}
