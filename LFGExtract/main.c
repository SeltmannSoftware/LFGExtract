//
//  main.c
//  LFGExtract V 1.1
//
//  Created by Kevin Seltmann on 6/11/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//

// TODO:
// option to provide list of files if they don't follow ___a.xxx convention
// verbose option?
// dictionary / window size, etc.
//
// write compresser!!!
// and reduce on install file!!! :)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "explode.h"

//#include <sys/time.h>

//struct timeval stop, start;

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

struct
{
    int file_count;
    int bytes_written_so_far;
    int bytes_read_so_far;
    int current_disk;
    char cur_filename[256];
} disk_info = {0};

// get lengh of input file
void get_file_length( FILE * fp_in )
{
    // Find file length
    fseek ( fp_in, 0, SEEK_END );
    archive_info.file_length = ftell( fp_in );
    
}

// Check that first four bytes are 'LFG!'
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

// Check that next four bytes are 'FILE'
bool isFileNext( FILE *fp_in ) {
    
    char buffer[4];
    
    if ( fread( buffer, sizeof buffer[0], 0x4, fp_in) == 0x4 ) {
        if( memcmp ( buffer, "FILE", 4 ) == 0 ) {
            return true;
        }
    }
    
    return false;
}

bool read_uint32( FILE *fp_in, uint32_t * result ) {
    unsigned char buffer[4];
    
    if ( fread( buffer, sizeof buffer[0], 0x4, fp_in ) == 0x4 ) {
        *result = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | (buffer[0]);
        return true;
    }

    return false;
}

bool read_chunk( FILE *fp_in, char * buffer, int length ) {
    
    if ( fread( buffer, sizeof buffer[0], length, fp_in ) == length ) {
        return true;
    }
    
    return false;
}

bool read_expected_byte( FILE *fp_in, char expectedByte) {
    
    if (expectedByte == getc(fp_in)) {
        return true;
    }
    
    return false;
}


bool open_archive( FILE **fp )
{
    FILE *fp_open;
    // Open for binary read
    fp_open=fopen(disk_info.cur_filename, "rb");
    
    if (fp_open == 0) {
        printf("Error opening file %s.\n\n", disk_info.cur_filename);
        return false;
    }
    
    get_file_length(fp_open);
    
    //Check first four bytes
    if (!isFileLFG(fp_open)) {
        printf("%s does not appear to be a LFG file.\n\n",disk_info.cur_filename);
        fclose (fp_open);
        return false;
    }
    
    //only if verbose
    //printf("Tag 'LFG!' found.\n\n");
    
    if (!read_uint32(fp_open, &archive_info.length)) {
        printf("%s does not appear to be a valid LFG file.\n\n", disk_info.cur_filename);
        fclose (fp_open);
        return false;
    }
    
    //if verbose
    //printf( "Archive length: \t%u bytes\t(0x%X)\n", archive_info.length, archive_info.length);
    
    *fp = fp_open;
    
    return true;
}


FILE* new_file( void )
{
    fclose(archive_info.fp);
    disk_info.cur_filename[7]++;  // won't work with relative paths.
    if (!open_archive(&archive_info.fp)) {
        return NULL;
    }
    
    return archive_info.fp;    
}

void print_usage ( void )
{
    printf("Usage: LFGExtract [-i] archive_name\n");
    printf("Extracts files from LFG archives used in older LucasArts games (.xxx extension).\n\n");
    printf("   -i    Show archive info only (do not extract)\n");
    printf("   -v    Print version info\n\n");
    
}

void print_version ( void )
{
    printf("LFGExtract V1.1\n");
    printf("by Kevin Seltmann, 2016\n\n");
}


int main (int argc, const char * argv[])
{
    
    FILE *fp = NULL;
    long file_pos = 0;
    bool isNotEnd = true;
    char temp_buff[6];
    bool info_only = false;
    int file_arg = 1;
    int temp_length = 0;
    const char exp_buff[6] = {2,0,1,0,0,0};
    
    for (int j = 1; j<argc; j++)
    {
        if (strcmp(argv[j], "-i") == 0) {
            info_only = true;
            file_arg++;
        }
        else if (strcmp(argv[j], "-v") == 0) {
            print_version();
            return 0;
        } else
        {
            file_arg = j;
        }
    }

    if (file_arg >= argc) {
        print_usage();
        return 0;
    }
    
    strcpy(disk_info.cur_filename, argv[file_arg]);  // check max, remove path

    if (!open_archive(&fp)) {
        return 0;
    }
    
    archive_info.fp = fp;
    
    if (!read_chunk(fp, archive_info.filename, 13)) {
        printf("%s does not appear to be a valid LFG file.\n\n",
               disk_info.cur_filename);
        fclose (archive_info.fp);
        return 0;
    }
    
    archive_info.filename[12] = 0;  // failsafe
    
    if (!read_expected_byte(archive_info.fp, 0)) {
        printf("%s does not appear to be a valid initial LFG file.\n\n",
               disk_info.cur_filename);
        fclose (archive_info.fp);
        return 0;
    }
    
    if (!read_chunk(archive_info.fp, &archive_info.num_disks, 1)) {
        printf("%s does not appear to be a valid initial LFG file.\n\n",
               disk_info.cur_filename);
        fclose (archive_info.fp);
        return 0;
    }

    if (!read_expected_byte(archive_info.fp, 0)) {
        printf("%s does not appear to be a valid initial LFG file.\n\n",
               disk_info.cur_filename);
        fclose (archive_info.fp);
        return 0;
    }

    printf( "Reported name: \t\t%s\n", archive_info.filename );
    printf( "Disk count: \t\t%u\n", archive_info.num_disks );

    if (archive_info.num_disks == 0)
    {
        printf("Warning: Disk count of 0 indicated. File may be corrupted.\n");
    }
    
    if (archive_info.file_length != archive_info.length + 8) {
        printf("Warning: Actual archive file length (%ld)\n         does not match indicated length (%d + 8).\n",
               archive_info.file_length, archive_info.length);
    }

    
    if (!read_uint32(archive_info.fp, &archive_info.space_needed)) {
        printf("%s does not appear to be a valid LFG file.\n\n",
               disk_info.cur_filename);
        fclose (archive_info.fp);
        return 0;
    }
    
    printf( "Space needed: \t\t%u bytes\n", archive_info.space_needed );

    printf( "\n" );
    if (!info_only)
        printf( "Extracting files:\n" );
    else
        printf( "Archived file info:\n" );
    printf( "Filename           Imploded Size         Exploded Size       Ratio\n" );
    printf( "--------------------------------------------------------------------\n" );
    
    if (verbose)
    {
      printf("%s\t   %7ld bytes\n", disk_info.cur_filename,
             archive_info.file_length);
   
    }
    
    while (isNotEnd && isFileNext(archive_info.fp))
    {
        
        if (!read_uint32(archive_info.fp, &file_info.length)) {
            printf("Unexpected end of file %s.\n\n", disk_info.cur_filename);
            fclose (archive_info.fp);
            return 0;
        }

        file_pos = ftell(archive_info.fp);
        
        if (!read_chunk(archive_info.fp, file_info.filename, 13)) {
            printf("Unexpected end of file %s.\n\n", disk_info.cur_filename);
            fclose (archive_info.fp);
            return 0;
        }
        
        // value meaning unknown
        if (!read_chunk(archive_info.fp, temp_buff, 1)) {
            printf("Unexpected end of file %s.\n\n", disk_info.cur_filename);
            fclose (archive_info.fp);
            return 0;
        }

        //if debug
        //for (int j=0; j<13; j++) {
        //    printf(" %02X", (unsigned char)file_info.filename[j]);
        //}
        //printf (" %02X\n", temp_buff[0]);

        file_info.filename[12] = 0;  // failsafe
        
        if (!read_uint32(archive_info.fp, &file_info.final_length)) {
            printf("Unexpected end of file %s.\n\n", disk_info.cur_filename);
            fclose (archive_info.fp);
            return 0;
        }
  
        // value meaning unknown
        if (!read_chunk(archive_info.fp, temp_buff, 6)) {
            printf("Unexpected end of file %s.\n\n", disk_info.cur_filename);
            fclose (archive_info.fp);
            return 0;
        }
        
        if (memcmp(temp_buff, exp_buff, 6) != 0)  {
            printf("Warning: Unexpected values in header. "
                   "File may be corrupted.\n");
        }

        file_pos += file_info.length;
        if ( file_pos <= archive_info.file_length) {
            printf( "  %-12s\t   %7d bytes",  file_info.filename,
                   file_info.length);
            printf( "         %7d bytes", file_info.final_length);
            printf("        %3d\%% \n",
                   (file_info.length * 100) / file_info.final_length);
            disk_info.file_count++;
        } else {
            printf( "  %-12s\t    (incomplete)\n", file_info.filename);
//          printf( "  %-12s\t    %7ld bytes", file_info.filename,
//                file_info.length - (file_pos - archive_info.file_length));
            temp_length = file_info.length;
            file_info.length = (uint32_t)(file_pos - archive_info.file_length);
        }
        
        disk_info.bytes_read_so_far += file_info.length;
        disk_info.bytes_written_so_far  += file_info.final_length;
        
        if ( file_pos >= archive_info.file_length )
        {
            isNotEnd = false;
            archive_info.num_disks--;
        }
        
        if (!info_only)
        {
            //gettimeofday(&start, NULL);

            (void) extract_and_explode(archive_info.fp, file_info.filename,
                                       file_info.final_length, &new_file);

            //gettimeofday(&stop, NULL);
            //if (verbose) {
            // printf("  Explode took %u uS.\n",
            //(unsigned int)(stop.tv_usec - start.tv_usec));
            // printf("  Explode took %ld uS.\n", stop.tv_sec - start.tv_sec);
            //
            // }
        }
        
        if( file_pos < archive_info.file_length )
        {
            fseek(archive_info.fp, file_pos, SEEK_SET);
        } else if (info_only && archive_info.num_disks)
        {
            (void)new_file();
        }
        
        if (archive_info.num_disks && !isNotEnd) {
            
            isNotEnd = true;
            
            printf("\n%s\t   %7ld bytes\n", disk_info.cur_filename,
                   archive_info.file_length);

            disk_info.file_count++;
            
            if( 8+file_info.length <= archive_info.file_length ) {
                printf( "  %-12s\t   %7d bytes",  file_info.filename,
                    temp_length);
                printf( "         %7d bytes", file_info.final_length);
                printf("        %3d\%% \n",
                       (temp_length * 100) / file_info.final_length);
                
                fseek(archive_info.fp, 8+file_info.length, SEEK_SET);
            } else {
                isNotEnd = false;
            }
        }
    }
    
    printf(" --------------------------------"
           "-----------------------------------\n" );
    printf("  %3d files                              %7d bytes\n",
           disk_info.file_count,
           disk_info.bytes_written_so_far );
    
    if( file_pos < archive_info.file_length ) {
        printf( "Warning: Unexpected end of file data." );
    }
    
    printf ("\n");
    
    fclose(archive_info.fp);
    
    return 0;
    
}

