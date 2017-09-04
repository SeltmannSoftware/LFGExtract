//
//  implode.c
//  LFGMake
//
//  Created by Kevin Seltmann on 6/17/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//
//  Designed to "implode" files in a similar fashion to the PKWARE Data
//  Compression Library from ~1990.  Implementation based on
//  specification found on the internet

#include "implode.h"
#include <stdbool.h>

#define MIN(x,y)  ((x)<(y))?(x):(y)    
    // Minimum of 2 values

#define ENCODE_BUFF_SIZE         0x2000
    // Buffer for the file to encode.  Must be larger than dictionary and have
    // at least 518 bytes of future data (1K works).
    // Maximum dictionary size is 4K (0x1000); here, 8K is used.

#define ENCODE_BUFF_MASK         0x1800
    // Masked MSBs of the encoder buffer are used to determine when
    // new data should be loaded from the file into the buffer.
    // Here, new data is loaded every 0x800 or 2K.

#define ENCODE_BUFF_LOAD_SIZE     0x800
    // Amount of data to be loaded at a time into buffer.
    // Effectively for 8k buffer we have 4-6K history
    // and 2-4K of future data depending where encode index is.

#define ENCODE_BUFF_LOAD_DONE    0xFFFF
    // Indicates that EOF has been reached on input file.
    // Just needs to be > BUFF SIZE

#define ENCODE_MIN_OFFSET 0
    // Minimum offset to use for encoding.
    // LFG appears to have used 1.

#define ENCODE_MAX_OFFSET ( dictionary_size )
    // Maximum offset to use for encoding.  Maximum possible is the
    // dictionary size.
    // LFG appears to have used the dictionary size - 1.
    // (i.e., 0 through 4094 rather than 0 through 4095.)

#define ENCODE_MAX_LENGTH 518
    // Maximum length to use for encoding.  Max possible is 518.
    // LFG appears to have used 516.

unsigned char encoding_buffer[ENCODE_BUFF_SIZE];
unsigned int encode_index;
unsigned int dictionary_size;  // 0x1000, 0x800, 0x400 for 4k, 2k, 1k
unsigned int dictionary_bits;  // 4,5, or 6
unsigned int bytes_encoded;
unsigned int bytes_written;
unsigned int bytes_length;


// -- BIT WRITE ROUTINES --
typedef struct {
    
    // Output file pointer.
    FILE* file_pointer;
    
    // The current byte that is being built.
    unsigned char byte_value;
    
    // Bit position in current byte for the next bit to be written.
    int bit_position;
    
    // Signals a read error
    int error_flag;
    
} write_bitstream_type;

write_bitstream_type write_bitstream;

// Write a single bit.
void write_next_bit( unsigned int bit )
{
    
    write_bitstream.byte_value |=
        (bit & 0x1) << write_bitstream.bit_position;

    if (write_bitstream.bit_position == 7)
    {
        fputc(write_bitstream.byte_value,
              write_bitstream.file_pointer );
        
        //printf("\n byte %d (%2x) ", bytes_written, write_bitstream.byte_value);
        write_bitstream.byte_value = 0;
        bytes_written++;
        
        // Check if error is reported.
        if (ferror(write_bitstream.file_pointer)) {
            printf("Error: file error.\n");
            write_bitstream.error_flag = true;
        }

    }
    
    write_bitstream.bit_position++;
    write_bitstream.bit_position%=8;
   
}

// Flush remaining bits to next byte.
void write_flush( void )
{
    int j = (8-write_bitstream.bit_position) % 8;
    
    for (int i=0; i<j; i++){
        write_next_bit(0);
    }
}

// Write some bits, msb first.
void write_bits_msb_first( unsigned int bit_count,
                           unsigned int bits)
{
   
    if (bit_count <= 8)
    {
        for (int i=bit_count-1; i>=0;i--)
        {
            write_next_bit((bits >> i) & 1);
        }
    }

}

// Write some bits, lsb first.
void write_bits_lsb_first( unsigned int bit_count,
                           unsigned int bits)
{
    
    if (bit_count <= 8)
    {
        for (int i=0; i<bit_count;i++)
        {
            write_next_bit((bits >> i) & 1);
        }
    }
    
}

void write_literal( unsigned int literal_val )
{
    write_next_bit(0);
    write_bits_lsb_first(8, literal_val);
    
    //printf("\nWriting literal %2x\n", literal_val);
};


// --- Routines for finding length and offset ---

// Lookup table for coverting dictionary offset to bit codes.
struct {
    unsigned int lookup_min;
    unsigned int bit_count;
    unsigned int bit_value;
} offset_to_bits_table[] =
{
    { 0x30, 8, 0x0F },
    { 0x16, 7, 0x21 },
    { 0x07, 6, 0x1F },
    { 0x03, 5, 0x13 },
    { 0x01, 4, 0x0B },
    { 0x00, 2, 0x03 }
};

// Lookup table for converting dictionary length to bit codes.
struct {
    unsigned int lookup_min;
    unsigned int bit_count;
    unsigned int bit_value;
    unsigned int lsb_count;
} length_table[] =
{
    { 264, 7, 0, 8},
    { 136, 7, 1, 7},
    {  72, 6, 1, 6},
    {  40, 6, 2, 5},
    {  24, 6, 3, 4},
    {  16, 5, 2, 3},
    {  12, 5, 3, 2},
    {  10, 5, 4, 1},
    {   9, 5, 5, 0},
    {   8, 4, 3, 0},
    {   7, 4, 4, 0},
    {   6, 4, 5, 0},
    {   5, 3, 3, 0},
    {   4, 3, 4, 0},
    {   3, 2, 3, 0},
    {   2, 3, 5, 0}
};

// Search through table to return number of bits and value of bits
// for encoding given offset.
void find_offset_codes(unsigned int offset,
                       unsigned int * offset_bits,
                       unsigned int * offset_code)
{
    int i, delta;
    
    // Look through table for entry in range of the offset we want.
    for (i = 0; offset < offset_to_bits_table[i].lookup_min; i++ );
    
    delta = offset - offset_to_bits_table[i].lookup_min;
    *offset_bits = offset_to_bits_table[i].bit_count;
    *offset_code = offset_to_bits_table[i].bit_value - delta;
}

// Search through table to return number of bits, value of bits,
// length of lsb bits, and value of lsb bits for given length.
void find_length_codes(unsigned int length,
                       unsigned int * length_bits,
                       unsigned int * length_code,
                       unsigned int * length_lsb_count,
                       unsigned int * length_lsb_value )
{
    int i;
    
    // Look through table for entry in range of the length we want.
    for (i = 0;length < length_table[i].lookup_min; i++ );
    
    *length_bits = length_table[i].bit_count;
    *length_code = length_table[i].bit_value;
    *length_lsb_count = length_table[i].lsb_count;
    *length_lsb_value = length - length_table[i].lookup_min;
}

// Find offset bits, length bits and write to file.
void write_dictionary_entry( int offset, int length )
{
    unsigned int low_offset_bits;
    unsigned int high_offset_bits;
    unsigned int offset_msb_code;
    unsigned int length_bits;
    unsigned int length_code;
    unsigned int length_lsb_bits;
    unsigned int length_lsb_value;
    
    //printf("\n writing length %d offset %d\n", length, offset);
    
    if (length!= 2)
    {
        low_offset_bits = dictionary_bits;
    }
    else
    {
        low_offset_bits = 2;
    }
    
    write_next_bit(1);
    
    find_length_codes(length,
                      &length_bits,
                      &length_code,
                      &length_lsb_bits,
                      &length_lsb_value);
    
    write_bits_msb_first( length_bits, length_code);
    write_bits_lsb_first( length_lsb_bits, length_lsb_value);
    
    find_offset_codes(offset>>low_offset_bits,
                      &high_offset_bits,
                      &offset_msb_code);
    
    write_bits_msb_first( high_offset_bits, offset_msb_code);
    write_bits_lsb_first( low_offset_bits, offset);
}

// Find the bit length of a offset, length pair without writing it.
int length_dictionary_entry( int offset, int length)
{
    unsigned int low_offset_bits;
    unsigned int high_offset_bits;
    unsigned int offset_msb_code;
    unsigned int length_bits;
    unsigned int length_code;
    unsigned int length_lsb_bits;
    unsigned int length_lsb_value;
    
    int bit_length = 1;
    
    if (length!= 2)
    {
        low_offset_bits = dictionary_bits;
    }
    else
    {
        low_offset_bits = 2;
    }
    
    bit_length+=low_offset_bits;
    
    find_length_codes(length,
                      &length_bits,
                      &length_code,
                      &length_lsb_bits,
                      &length_lsb_value);
    
    bit_length+=length_bits + length_lsb_bits;
    
    
    find_offset_codes(offset>>low_offset_bits,
                      &high_offset_bits,
                      &offset_msb_code);
    
    bit_length+=high_offset_bits;
    
    return bit_length;
}



// Check dictionary for a byte sequence match at a particular position.
// Target sequence is already in buffer (as those are the bytes being encoded).
// Takes the buffer (bytes), two positions within the buffer,
// the max length of the byte sequence to compare, and
// the circular buffer size.
int compare_in_circular( unsigned char * buffer,
                         unsigned int position1,
                         unsigned int position2,
                         int max_length,
                         int circular_buffer_size)
{
    int i = 0;
    
    do
    {
        position1 %= circular_buffer_size;
        position2 %= circular_buffer_size;
        
        if (buffer[position1] != buffer[position2])
        {
            break;
        }
        
        position1++;
        position2++;
        i++;
    } while (i<max_length);
    
    return i;
}


// Look in dictionary for sequence
// TRUE if sequence is found
bool check_dictionary( unsigned int* length,           // length found
                       unsigned int* offset,           // offset found
                       unsigned char *encoding_buffer, // dictionary
                       unsigned int encoding_index)    // start index
{
    bool match_found = false;
    int search_size = MIN(ENCODE_MAX_OFFSET, bytes_encoded);
    unsigned int offset_val = 0;
    int final_length = 1;
    int length_now;
    int max_length = MIN(bytes_length - bytes_encoded, ENCODE_MAX_LENGTH);
    
    for (int i=(ENCODE_MIN_OFFSET+1); i<=search_size; i++)
    {
        length_now = compare_in_circular( encoding_buffer,
                                          encoding_index,
                                          encoding_index-i,
                                          max_length,
                                          ENCODE_BUFF_SIZE );
        
        // If the found length is greater than the length found so far,
        // update length, remember the offset, continue looking.
        // 
        if (length_now > final_length)
        {
            final_length=length_now;
            offset_val = i-1;
            match_found = true;
        }
    }
    
    // Fill in the offset and length of the match
    if (match_found)
    {
        *offset = offset_val;
        *length = final_length;
    }
    
    return match_found;
}

unsigned int next_load_point = 0;

unsigned int implode( FILE * in_file,
                      FILE * out_file,
                      unsigned int length,
                      implode_dictionary_type dictionary,
                      unsigned int initial_max_length,
                      unsigned int next_max_length,
                      FILE* (*max_reached)( FILE*) )
{
    unsigned int encode_length = 0;
    long bytes_loaded;
    
    bool initial = true;
    
    encode_index = 0;
    bytes_encoded = 0;
    bytes_written = 2;  // account for header
    bytes_length = length;
    
    // range check dictionary
    dictionary_size = 1 << ((int)dictionary + 6);
    dictionary_bits = (int)dictionary;
    
    write_bitstream.file_pointer = out_file;
    
    bytes_loaded = fread( encoding_buffer,
                          sizeof encoding_buffer[0],
                          ENCODE_BUFF_LOAD_SIZE,
                          in_file);
    
    // File is shorter than our buffer. Mark no more loads.
    if ( bytes_loaded != ENCODE_BUFF_LOAD_SIZE )
    {
        next_load_point = ENCODE_BUFF_LOAD_DONE;
    }
    else
    {
        next_load_point = 0;
    }
    
    fputc(0, out_file);
    fputc(dictionary_bits, out_file);
    
    // While there are bytes to encode...
    while (bytes_encoded < length)
    {
        unsigned int offset, offset_b;
        bool use_literal = true;
        
        if ((initial && (bytes_written>initial_max_length)) ||
             bytes_written>next_max_length)
        {
            initial = false;
            
            printf(" Switch at %d for %d, %d\n", bytes_written, initial_max_length, next_max_length);
            
            if (max_reached)
            {
                write_bitstream.file_pointer = max_reached( write_bitstream.file_pointer );
            }
        }
        
        // Check if data should be loaded into buffer.
        if ((encode_index & ENCODE_BUFF_MASK) ==
            (next_load_point & ENCODE_BUFF_MASK))
        {
            next_load_point+=ENCODE_BUFF_LOAD_SIZE;
            next_load_point%=ENCODE_BUFF_SIZE;
            
            if (fread(&encoding_buffer[next_load_point],
                      sizeof encoding_buffer[0],
                      ENCODE_BUFF_LOAD_SIZE,
                      in_file)
                != ENCODE_BUFF_LOAD_SIZE)
            {
                // Hit end of file. Signal no more data reads.
                next_load_point = ENCODE_BUFF_LOAD_DONE;
            }
        }
        
        encode_index %= ENCODE_BUFF_SIZE;
        
        // Encoding buffer and dictionary are one and the same.
        // Dictionary is simple bytes that have already been encoded.
        // Check for the longer run of next bytes in the dictionary.
        if ( check_dictionary(&encode_length, &offset,
                               encoding_buffer, encode_index))
        {
            int temp, temp1;
            int possible_bitcount, bitcount_with_literal;
            float bits_per_byte,bits_per_byte_lit;
            unsigned int new_length;
            
            if (encode_length<2)
            printf("ENCODE LENGTH! %d\n", encode_length);
            
            // Byt the time we are here, encode length must be >= 2
            // and encode_length + bytes already encoded should not
            // exceed the file length
 
            use_literal = false;
            
            // A sequence was found, but now see if a better match can be
            // found if we use a literal for next byte, then sequence.
            if ( check_dictionary( &new_length,
                                   &offset_b, encoding_buffer,
                                   (encode_index+1) % ENCODE_BUFF_SIZE ))
            {
                
                // Compare the bit ratio for each case.
                
                // Make sure the new lengths are valid.
                if ((new_length > 2) ||
                    ((new_length ==2 ) && (offset_b <= 255)))
                {
                    possible_bitcount =
                        length_dictionary_entry(offset, encode_length);
                    bitcount_with_literal =
                        length_dictionary_entry(offset_b, new_length);
                    
                    bits_per_byte = (float)possible_bitcount/encode_length;
                    bits_per_byte_lit = (float)(bitcount_with_literal + 9)
                                / (new_length + 1);
                       
               //         printf ("Bits/byte %f   ...  %f\n", bits_per_byte, bits_per_byte_lit);
                        
              //          printf("(L,O): %d, %d (%d) -> Literal + %d, %d (%d)\n", encode_length, offset, possible_bitcount, new_length, offset_b, bitcount_with_literal);
                        
                    if (bits_per_byte_lit <= bits_per_byte)
                    {
                        use_literal = true;
                    }

                    // Now we can check if same sequence can do better if used
                    // with original sequence.
                    temp1 = new_length + 1 - encode_length;
                    if ( temp1 > 0 )
                    {
                        if ( temp1  == 1 )
                        {
                            temp = 9;
                        }
                        else
                        {
                            if ((temp1 == 2) && (offset_b > 255))
                                temp = 18;
                            else
                                temp = length_dictionary_entry(offset_b, new_length+1-encode_length);
                        }
                        
                        if (( possible_bitcount + temp) <= ( bitcount_with_literal + 9))
                        {
                            use_literal = false;
                        }
                    }
                   // }
                //printf("Actual bits:  %d vs %d\n\n", possible_bitcount + length_dictionary_entry(offset_b, new_length+1-encode_length), bitcount_with_literal + 9);
                }
                
            }
        }
        
        // If flag for literal is set, or encode length is less than 2,
        // or encode length is 2 but offset is > 255, use literal.
        // Else use dictionary.   (Encode length shouldn't be < 2. Ever)
        if (use_literal || //(encode_length == 0) || (encode_length == 1) ||
            ((encode_length == 2) && (offset > 255)))
        {
            write_literal(encoding_buffer[encode_index]);
            encode_index++;
            bytes_encoded++;
            encode_length = 0;
        }
        else
        {
            write_dictionary_entry(offset, encode_length);
            encode_index += encode_length;
            bytes_encoded += encode_length;
            encode_length = 0;
        }

    }
    
    // Write end-of-data marker (Length 519) and zero bits for final byte.
    write_next_bit(1);
    write_bits_msb_first( 7, 0);
    write_bits_lsb_first( 8, 0xFF);
    write_flush();
    
    return bytes_written;
}

