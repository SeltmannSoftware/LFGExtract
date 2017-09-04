//
//  explode.c
//  LFGExtract V 1.1
//
//  Created by Kevin Seltmann on 6/12/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//
//  Designed to extract the archiving used on LucasArts Classic Adventure
//  install files (*.XXX) and possibly other archives created with the PKWARE
//  Data Compression Library from ~1990.  Implementation based on
//  specifications found on the internet.

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// -- BIT READ ROUTINES --
typedef struct {

    // Input file pointer.
    FILE* file_pointer;

    // CB function for handling when in file EOF is reached. Used for
    // multi-file archives; handler can return a new file pointer
    // but must be at correct point in the data stream.
    FILE* (*eof_reached) ( void );

    // The current byte from input stream that bits are being pulled from.
    unsigned char current_byte_value;

    // Bit position in current byte. Next bit will come from this position.
    int current_bit_position;

    // Signals a read error
    int error_flag;
    
    // Stats.  Used to track number of bits for a length/offset combo.
    unsigned int bitcount;
    
    // Stats. Used to track total number of encoded bits read.
    unsigned long total_bits;
    
} read_bitstream_type;

read_bitstream_type read_bitstream;

// Returns number of bits read, then resets count.
// Used for statistics.
unsigned int read_bitcount( void )
{
    unsigned int bitcount = read_bitstream.bitcount;
    read_bitstream.bitcount = 0;
    return bitcount;

}

// Read bit from bitstream byte.
unsigned int read_next_bit( void )
{
    int ch, value;
    
    // Start of byte, need to load new byte.
    if (read_bitstream.current_bit_position == 0)
    {
        ch = fgetc(read_bitstream.file_pointer);
        
        // Check that end of file wasn't reached.
        if ((ch==EOF) && (read_bitstream.eof_reached != NULL))
        {
            read_bitstream.file_pointer = read_bitstream.eof_reached();
            
            if (read_bitstream.file_pointer)
            {
                // New file. Now try to get a byte.
                ch = fgetc(read_bitstream.file_pointer);
            } else {
                // No new file
                read_bitstream.error_flag = true;
            }
        }
        
        // Error if eof still occurs or a different error is reported.
        if (ch < 0)
        {
            printf("Error: Unexpected end of file or file error.\n");
            read_bitstream.error_flag = true;
        }
        
        read_bitstream.current_byte_value = ch;
    }
    
    value  = (read_bitstream.current_byte_value >>
              read_bitstream.current_bit_position) & 0x1;
    
    read_bitstream.current_bit_position++;
    read_bitstream.current_bit_position%=8;
    
    read_bitstream.bitcount++;
    read_bitstream.total_bits++;
    
    return (unsigned int) value;
}

// General function to read bits, and assemble them with MSBs first. Note
// that bits are always read from the byte stream lsb first. What matters here
// is how they are reassembled.
unsigned int read_bits_msb_first( int bit_count )
{
    unsigned int temp = 0;
    
    if (bit_count <= 8)
    {
        for (int i=0; i<bit_count;i++)
        {
            temp = (temp << 1) | read_next_bit();
        }
    }
    return temp;
}

// General function to read bits, and assemble them with LSBs first.
unsigned int read_bits_lsb_first( int bit_count )
{
    unsigned int temp = 0;
    
    if (bit_count <= 8)
    {
        for (int i=0; i<bit_count;i++)
        {
            temp = (read_next_bit() << i) | temp;
        }
    }
    
    return temp;
}


// -- BYTE WRITE BUFFER ROUTINES --

#define WRITE_BUFF_SIZE      0x4000   // ( 16k)

typedef struct {

    // Output file pointer.
    FILE* file_pointer;
    
    // write position in buffer
    unsigned int buffer_position;
    
    // total bytes written
    unsigned int bytes_written;
    
    // Signals a write error
    int error_flag;
    
    // Write memory buffer
    // Must write out buffer every time the window fills or at file end.
    unsigned char buffer[ WRITE_BUFF_SIZE ];
    
} write_buffer_type;

write_buffer_type write_buffer;

// Writes output buffer to file
void write_to_file( void )
{
    fwrite(write_buffer.buffer, sizeof(write_buffer.buffer[0]),
           write_buffer.buffer_position,
           write_buffer.file_pointer);
    
    if (ferror(write_buffer.file_pointer))
    {
        write_buffer.error_flag = true;
    }
    
    write_buffer.bytes_written += write_buffer.buffer_position;
}

// Write a byte out to the output stream.
void write_byte( unsigned char next_byte )
{
    write_buffer.buffer[write_buffer.buffer_position++] = next_byte;
    
    if (write_buffer.buffer_position == WRITE_BUFF_SIZE)
    {
        write_to_file();
    }
    
    write_buffer.buffer_position %= WRITE_BUFF_SIZE;
    
}

// Read byte from the *output* stream
unsigned char read_byte_from_write_buffer( int offset )
{
    return write_buffer.buffer[(write_buffer.buffer_position-offset)
                                  % WRITE_BUFF_SIZE];
}

// -- EXPLODE IMPLEMENTATION --

struct {
    
    // length for copying from dictionary.
    int length;
    
    // offset for copying from dictionary.
    int offset;
    
    // Flags when the end of file marker is read (when length = 519)
    bool end_marker;
    
    // Statistics
    unsigned int literal_count;
    unsigned int dictionary_count;
    int max_offset;
    int min_offset;
    int max_length;
    int min_length;
    int length_histogram[520];
    
} explode;


// Header info
struct {
    uint8_t literal_mode;
    uint8_t dictionary_size;
} header;


// Copy offset table; indexed by bit length
struct {
    // Number of codes of this length (number of bits)
    unsigned int count;
    
    // Base output value for this length (number of bits)
    unsigned int base_value;
    
    // Base input bits for this number of bits.
    unsigned int base_bits;
    
} offset_bits_to_value_table[] =
{
    { 0, 0x00, 0x00},
    { 0, 0x00, 0x00},
    { 1, 0x00, 0x03},
    { 0, 0x00, 0x00},
    { 2, 0x02, 0x0A},
    { 4, 0x06, 0x10},
    {15, 0x15, 0x11},
    {26, 0x2F, 0x08},
    {16, 0x3F, 0x00}
};


// Read copy length codes. A fairly brute force method as there is an odd
// switching of msb first to lsb first in the interpretation and the
// values 2 and 3 do not follow the natural huffman-like coding.
int read_copy_length( void )
{
    int length = 0;
    
    // First two bits (xx)
    switch (read_bits_msb_first(2)) {
        
        case 0:            
            // Next 2 bits (00xx)
            switch (read_bits_msb_first( 2)) {
            
                case 0:                   
                    // Next 2 bits (0000xx)
                    switch (read_bits_msb_first( 2)) {
                    
                        case 0:                        
                            // Next bit (000000x)
                            if (read_next_bit())
                                // 0000001xxxxxxx
                                length = 136 + read_bits_lsb_first(7);
                            else
                                // 0000000xxxxxxxx
                                length = 264 + read_bits_lsb_first(8);
                            break;
                            
                        case 1:
                            // 000001xxxxxx
                            length = 72 + read_bits_lsb_first(6);
                            break;
                            
                        case 2:
                            // 000010xxxxx
                            length = 40 + read_bits_lsb_first(5);
                            break;
                            
                        case 3:
                            // 0000
                            length = 24 + read_bits_lsb_first(4);
                    }
                    break;
                    
                case 1:
                	// Next bit (0001x)
                    if (read_next_bit())
                        // 00011xx
                        length = 12 + read_bits_lsb_first(2);
                    else
                        // 00010xxx
                        length = 16 + read_bits_lsb_first(3);
                    break;
                    
                case 2:
                	// Next bit (0010x)
                    if (read_next_bit()) {
                        // 00101
                        length = 9;
                    } else {
                        // 00100x
                        length = 10 + read_next_bit();
                    }
                    break;
                    
                case 3:
                    // 0011
                    length = 8;
            }
            break;
           
        case 1:
			// Next bit (01x)
            if (read_next_bit()) {
                // 011
                length = 5;
            } else {
            	// Next bit (010x)
                if (read_next_bit()) {
                    // 0101
                    length = 6;
                } else {
                    // 0100
                    length = 7;
                }
            }
            break;
            
        case 2:
        	// Next bit (10x)
            if (read_next_bit()) {
                // 101
                length = 2;
            } else {
                // 100
                length = 4;
            }
            break;
            
        case 3:
            // 11
            length = 3;
    }
    
    if (length == 0) printf("Error: Copy length returned zero value.\n");
    
    return length;
    
}

// Read the offset part of a length/offset reference
int read_copy_offset( void )
{
    int offset = 0;         // The offset value we are looking for.
    int offset_bits = 0;    // Input bits for offset.
    int diff;               // Difference used in calulating with table.
    int num_lsbs;           // Number of lsbs to use.
    int length;             //
    
    // Get 6 MS bits of the resulting offset
    offset_bits = read_bits_msb_first(2);
    
    // Go through table by length to see if there is a match.
    for (length = 2; length<9; length++) {
        
        diff = offset_bits - offset_bits_to_value_table[length].base_bits;
        
        if ( ( diff >=0 ) &&
            (diff < offset_bits_to_value_table[length].count) )
        {
            offset = offset_bits_to_value_table[length].base_value - diff;
            break;
        }
        
        offset_bits = (offset_bits << 1) | read_next_bit();
    }
    
    if (length == 9) printf("Error: Copy offset value not found.\n");

    // Now get low order bits and append. Length 2 is a special case.
    if (explode.length == 2)
        num_lsbs = 2;
    else
        num_lsbs = header.dictionary_size;
    
    offset = (offset << num_lsbs) | read_bits_lsb_first(num_lsbs);
    
    return offset;
}

void write_dict_data( void )
{
    int offset = explode.offset+1;   // +1 since zero should reference the
                                     // previous byte.
    
    // Do this length times. Offset does not change since one byte is
    // added each iteration and we are counting from the end.
    for (int i = 0; i < explode.length; i++) {
        write_byte( read_byte_from_write_buffer(offset) );
    }
}

/* Extract a file from an archive file and explode it.
   in_fp:           Pointer to imploded data start in archive file.
   out_filename:    Output filename [consider making this fp_out].
   expected_length: Expected length of file (0 if not provided).
   eof_reached():   Callback that indicates archive EOF is reached.
                    Callback should return new file pointer with 
                    the continued data for the imploded file.
 */
int extract_and_explode( FILE * in_fp,
                         FILE* out_fp,
                         int expected_length,
                         bool print_stats,
                         FILE* (*eof_reached)(void))
{
    // Set up read parameters. [Consider making this a function.]
    read_bitstream.file_pointer = in_fp;
    read_bitstream.eof_reached = eof_reached;
    read_bitstream.current_bit_position = 0;
    read_bitstream.error_flag = 0;
    read_bitstream.total_bits = 0;

    // Reset write parameters. [ Consider making this a function. ]
    write_buffer.bytes_written = 0;
    write_buffer.error_flag = 0;
    write_buffer.buffer_position = 0;
    write_buffer.file_pointer=out_fp;
    
    // Reset counters/markers.
    explode.end_marker = false;
    explode.length = 0;
    explode.offset = 0;
    
    // Initialize statistics.
    explode.literal_count = 0;
    explode.dictionary_count = 0;
    explode.max_offset = 0;
    explode.min_offset = 0x8000;
    explode.max_length = 0;
    explode.min_length = 0x8000;
    memset(explode.length_histogram, 0, sizeof(explode.length_histogram));
    
    // Read two header bytes.
    if ( fread( (uint8_t*) &header, sizeof (uint8_t), 2, in_fp ) != 2 ) {
        printf("Error: Unable to read header info.\n");
        return -1;
    }
    
    // Check literal mode value. Only 0 currently supported (1 is also defined)
    if (header.literal_mode != 00) {
        printf("Error: Literal mode %d not supported.\n", header.literal_mode);
        return -1;
    }

    // Check dictionary size value. Supports values of 4 through 6.
    // Dictionary size is 1 << (6 + val) (or 2^(6+val) ): 1024, 2048, or 4096
    if ((header.dictionary_size < 4) || (header.dictionary_size > 6)) {
        printf("Error: Bad dictionary size value (%d) in header.\n",
               header.dictionary_size);
        return -1;
    }
    
    // Read until EOF is detected.
    do
    {
        // Next bit indicates a literal or dictionary lookup.
        if (read_next_bit() == 0)
        {
            // -- Literal --
            unsigned char value;
            
            value = read_bits_lsb_first(8);
            write_byte( value );
            
            // Stats update
            explode.literal_count++;
        }
        else
        {
            // -- Dictionary Look Up --
            // Reset internal bitcount. Keeps track of number of bits
            // read for stats.
            int bitcount_a = read_bitcount();
            int bitcount_b;
            
            // Dictionary look up.  Find length and offset.
            explode.length = read_copy_length();
            
            // Stats. Gives number of bits used for length.
            bitcount_a = read_bitcount();
            
            // Length of 519 indicates end of file.
            if (explode.length == 519)
            {
                explode.end_marker = true;
            }
            else // otherwise,
            {
                
                // Find offset.
                explode.offset = read_copy_offset();
               
                // Use copy length and offset to copy data from dictionary.
                write_dict_data();
                
                // Statistics update
                explode.dictionary_count++;
                explode.length_histogram[explode.length]++;
                
                if (explode.length > explode.max_length)
                    explode.max_length = explode.length;
                if (explode.length < explode.min_length)
                    explode.min_length = explode.length;
                if (explode.offset > explode.max_offset)
                    explode.max_offset = explode.offset;
                if (explode.offset < explode.min_offset)
                    explode.min_offset = explode.offset;
                
                // More stats. Gives bits used for offset.
                bitcount_b = read_bitcount();
                
                //printf("  %d bits encoded %d bits (%d bytes)\n",
                //    bitcount_a + bitcount_b + 1,
                //    explode.length * 8, explode.length);
            }
        }
    } while ( !explode.end_marker &&
              !read_bitstream.error_flag && !write_buffer.error_flag );
    
    write_to_file();
    
    // If expected length was passed in, check it.
    if ((expected_length) &&
        (write_buffer.bytes_written != expected_length))
    {
        printf( "Warning: Number of bytes written (%d) "
                "doesn't match expected value (%d).\n",
                write_buffer.bytes_written, expected_length);
    }
    
    if (print_stats)
    {
        printf("    Literal Mode: %d  Literals: %d\n",
               header.literal_mode, explode.literal_count);
        printf("    Dictionary Size: %d  Dictionary Lookups: %d\n",
               1 << (header.dictionary_size + 6), explode.dictionary_count);
    
        printf("    Min/Max: Offset [%d, %d], Length [%d, %d]\n",
               explode.min_offset, explode.max_offset, explode.min_length,
               explode.max_length);
    }
    
// Length histogram. Interesting!
//  for (int i=0; i<520; i++)
//  {
//      if (explode.length_histogram[i])
//      {
//          printf("    %d: %d\n", i, explode.length_histogram[i]);
//      }
//  }
    
    return write_buffer.bytes_written;
}




