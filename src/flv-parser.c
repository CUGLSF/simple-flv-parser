#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include "flv-parser.h"

// File-scope ("global") variables
const char *flv_signature = "FLV";

const char *sound_formats[] = {
        "Linear PCM, platform endian",
        "ADPCM",
        "MP3",
        "Linear PCM, little endian",
        "Nellymoser 16 kHz mono",
        "Nellymoser 8 kHz mono",
        "Nellymoser",
        "G.711 A-law logarithmic PCM",
        "G.711 mu-law logarithmic PCM",
        "reserved",
        "AAC",                          // 10
        "Speex",
        "not defined by standard",
        "not defined by standard",
        "MP3 8-Khz",
        "Device-specific sound"
};

const char *sound_rates[] = {
        "5.5 Khz",
        "11 Khz",
        "22 Khz",
        "44 Khz"
};

const char *sound_sizes[] = {
        "8-bit samples",
        "16-bit samples"
};

const char *sound_types[] = {
        "Mono sound",
        "Stereo sound"
};

const char *frame_types[] = {
        "not defined by standard",
        "keyframe (for AVC, a seekable frame)",
        "inter frame (for AVC, a non-seekable frame)",
        "disposable inter frame (H.263 only)",
        "generated keyframe (reserved for server use only)",
        "video info/command frame"
};

const char *codec_ids[] = {
        "not defined by standard",
        "not defined by standard",
        "Sorenson H.263",
        "Screen video",
        "On2 VP6",
        "On2 VP6 with alpha channel",
        "Screen video version 2",
        "AVC"
};

const char *avc_packet_types[] = {
        "AVC sequence header",
        "AVC NALU",
        "AVC end of sequence (lower level NALU sequence ender is not required or supported)"
};
const char *metadata_properties[] = {
    "audiocodecid",         // Number Audio codec ID used in the file (see E.4.2.1 for available SoundFormat values)
    "audiodatarate",        // Number Audio bit rate in kilobits per second
    "audiodelay",           // Number Delay introduced by the audio codec in seconds
    "audiosamplerate",      // Number Frequency at which the audio stream is replayed
    "audiosamplesize",      // Number Resolution of a single audio sample
    "canSeekToEnd",         // Boolean Indicating the last video frame is a key frame
    "creationdate",         // String Creation date and time
    "duration",             // Number Total duration of the file in seconds
    "filesize",             // Number Total size of the file in bytes
    "framerate",            // Number Number of frames per second
    "height",               // Number Height of the video in pixels
    "stereo",               // Boolean Indicating stereo audio
    "videocodecid",         // Number Video codec ID used in the file (see E.4.3.1 for available CodecID values)
    "videodatarate",        // Number Video bit rate in kilobits per second
    "width"                 // Number Width of the video in pixels
};
const char *postfix[] = {
    "kbs",
    "seconds",
    "Hz",
    "fps",
    "pixels",
    "bytes"
};
union ieee_754_double {
    double data;
    uint8_t b[8];
};
static FILE *g_infile;
static uint32_t tag_count;
void die(void) {
    printf("Error!\n");
    exit(-1);
}

/*
 * @brief read bits from 1 byte
 * @param[in] value: 1 byte to analysize
 * @param[in] start_bit: start from the low bit side
 * @param[in] count: number of bits
 */
uint8_t flv_get_bits(uint8_t value, uint8_t start_bit, uint8_t count) {
    uint8_t mask = 0;

    mask = (uint8_t) (((1 << count) - 1) << start_bit);
    return (mask & value) >> start_bit;

}

void flv_print_header(flv_header_t *flv_header) {
    if (!flv_header)
    {
        printf("line: %d, the parameter flv_header is NULL!", __LINE__);
        return;
    }
    printf("FLV file version %u\n", flv_header->version);
    printf("  Contains audio tags: ");
    // UB[1]:the sixth bit
    if (flv_header->type_flags & (1 << FLV_HEADER_AUDIO_BIT)) {
        printf("Yes\n");
    } else {
        printf("No\n");
    }
    printf("  Contains video tags: ");
    // UB[1]:the eighth bit
    if (flv_header->type_flags & (1 << FLV_HEADER_VIDEO_BIT)) {
        printf("Yes\n");
    } else {
        printf("No\n");
    }
    printf("  Data offset: %lu\n", (unsigned long) flv_header->data_offset);
}
size_t check_read_error(int line_num, const char * func_name, int count, int read_bytes)
{
    if (!feof(g_infile) && count != read_bytes)
    {
        printf("line: %d, read error in function %s", line_num, func_name);
        return 1; // Some Error
    }
    return 0;     // OK
}
size_t fread_1(uint8_t *ptr) {
    assert(NULL != ptr);
    size_t count = 0;
    count = fread(ptr, 1, 1, g_infile);
    if (!check_read_error(__LINE__, __FUNCTION__, count, 1))
       return count * 1;
    else
       exit(1);
}
void fread_2(uint16_t *ptr) {
    assert(NULL != ptr);
    size_t count = 0;
    uint8_t bytes[2] = {0};
    *ptr = 0;
    count = fread(bytes, 2, 1, g_infile);
    if(!check_read_error(__LINE__, __FUNCTION__, count, 1)) 
        *ptr = ((bytes[0] << 8) | bytes[1]);
    else
        exit(1);
}
void fread_3(uint32_t *ptr) {
    assert(NULL != ptr);
    size_t count = 0;
    uint8_t bytes[3] = {0};
    *ptr = 0;
    count = fread(bytes, 3, 1, g_infile);
    if(!check_read_error(__LINE__, __FUNCTION__, count, 1)) 
        *ptr = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
    else
        exit(1);
}

void fread_4(uint32_t *ptr) {
    assert(NULL != ptr);
    size_t count = 0;
    uint8_t bytes[4] = {0};
    *ptr = 0;
    count = fread(bytes, 4, 1, g_infile);
    if(!check_read_error(__LINE__, __FUNCTION__, count, 1)) 
    {
        // BIG-ENDIAN
        *ptr = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
    }
    else
       exit(1);
}

void fread_double(double *ptr) {
    assert(NULL != ptr);
    size_t count = 0;
    uint8_t bytes[8] = {0};
    *ptr = 0.0;
    count = fread(bytes, 8, 1, g_infile);
    // IEEE-754 DOUBLE 8-byte, BIG-ENDIAN,the sign bit is at the low memeory
    //         bit[63]  bit[53]-bit[62]         bit[0]-bit[52]
    // data =(sign bit) * (weishu)         *      2^(jiema) 
    if(!check_read_error(__LINE__, __FUNCTION__, count, 1))
    {
        union ieee_754_double read_data;
        for (int i = 0; i < 8; ++i)
    	    read_data.b[7 - i] = bytes[i];
        *ptr = read_data.data;
    }
    else
        exit(1);
}
void init_audio_tag(audio_tag_t * tag)
{
    assert(tag != NULL);
    tag->sound_format = 0;
    tag->sound_rate = 0;
    tag->sound_size = 0;
    tag->sound_type = 0;
    tag->data = NULL;
}
/*
 * @brief read audio tag
 */
audio_tag_t *read_audio_tag(flv_tag_t *flv_tag) {
    assert(NULL != flv_tag);
    uint8_t byte = 0;
    audio_tag_t *tag = NULL;

    tag = malloc(sizeof(audio_tag_t));
    if (!tag)
    {
        printf("line: %d, the parameter flv_tag is NULL!", __LINE__);
        return NULL;
    }
    init_audio_tag(tag);
    fread_1(&byte);

    tag->sound_format = flv_get_bits(byte, 4, 4);    // UB[4]
    tag->sound_rate = flv_get_bits(byte, 2, 2);      // UB[2]
    tag->sound_size = flv_get_bits(byte, 1, 1);      // UB[1]
    tag->sound_type = flv_get_bits(byte, 0, 1);      // UB[1], total 1 byte.

    printf("  Audio tag:\n");
    printf("    SoundFormat: %u - %s\n", tag->sound_format, sound_formats[tag->sound_format]);
    printf("    SoundRate: %u - %s\n", tag->sound_rate, sound_rates[tag->sound_rate]);

    printf("    SoundSize: %u - %s\n", tag->sound_size, sound_sizes[tag->sound_size]);
    printf("    SoundType: %u - %s\n", tag->sound_type, sound_types[tag->sound_type]);
    
    byte = 0;
    if (tag->sound_format != 10)
    {
        // -1 because the first byte in the AudioTagHeader
        tag->data = malloc((size_t) flv_tag->data_size - 1);
        size_t count = 0;
        count  = fread(tag->data, 1, (size_t) flv_tag->data_size - 1, g_infile); 
        if(!check_read_error(__LINE__, __FUNCTION__, count, flv_tag->data_size - 1))
        {
            return tag;
        }
        else
        {
            free(tag->data);
            free(tag);
            return NULL;
        }
     }
     // AAC Encoding
     else 
     {
        fread_1(&byte); 
        // 0 = AAC sequence header
        // 1 = AAC raw   
        printf("    AACPacketType: %u - %s\n", byte, (byte == 0?"AAC sequence header":"AAC raw"));
        tag->data = malloc((size_t) flv_tag->data_size - 2);
        size_t count = 0;
        count  = fread(tag->data, 1, (size_t) flv_tag->data_size - 2, g_infile); 
        if(!check_read_error(__LINE__, __FUNCTION__, count, flv_tag->data_size - 2))
        {
            return tag;
        }
        else
        {
            free(tag->data);
            free(tag);
            return NULL;
        } 
        
     }
}
void init_video_tag(video_tag_t * tag)
{
    assert(tag != NULL);
    tag->frame_type = 0;
    tag->codec_id = 0;
    tag->data = NULL;
}
/*
 * @brief read video tag
 */
video_tag_t *read_video_tag(flv_tag_t *flv_tag) {
    assert(flv_tag != NULL);
    uint8_t byte = 0;
    video_tag_t *tag = NULL;

    tag = malloc(sizeof(video_tag_t));
    if (tag == NULL)
    {
        printf("line: %d, malloc error in function %s\n", __LINE__, __FUNCTION__);
        return NULL;
    }

    init_video_tag(tag);

    fread_1(&byte);

    tag->frame_type = flv_get_bits(byte, 4, 4);
    tag->codec_id = flv_get_bits(byte, 0, 4);

    printf("  Video tag:\n");
    printf("    Frame type: %u - %s\n", tag->frame_type, frame_types[tag->frame_type]);
    printf("    Codec ID: %u - %s\n", tag->codec_id, codec_ids[tag->codec_id]);
    
    // Video frame payload 
    // IF CodecID == 2 
    //      H263VIDEOPACKET
    // IF CodecID == 3
    //      SCREENVIDEOPACKET
    // IF CodecID == 4
    //      VP6FLVVIDEOPACKET
    // IF CodecID == 5
    //      VP6FLVALPHAVIDEOPACKET
    // IF CodecID == 6
    //      SCREENV2VIDEOPACKET
    // IF CodecID == 7
    //      AVCVIDEOPACKET
    if (tag->frame_type != 5)
    {
        // AVCVIDEOPACKET
        if (tag->codec_id == FLV_CODEC_ID_AVC) {
            tag->data = read_avc_video_tag(tag, flv_tag, (uint32_t) (flv_tag->data_size - 1));
        }
        // Other Packets, TO_DO
        else
        {
            tag->data = malloc((size_t) flv_tag->data_size - 1);
            size_t count = 0;
            count = fread(tag->data, 1, (size_t) flv_tag->data_size - 1, g_infile);
            if(check_read_error(__LINE__, __FUNCTION__, count, flv_tag->data_size - 1))
            {
                free(tag->data);
                free(tag);
                return NULL; 
            }
            switch(tag->codec_id) 
            {
                case FLV_CODEC_ID_H263:
                    printf("    H263VIDEOPACKET\n");
                    break;
                case FLV_CODEC_ID_SCREEN:
                    printf("    SCREENVIDEOPACKET\n");
                    break;
                case FLV_CODEC_ID_VP6:
                    printf("    VP6VIDEOPACKET\n");
                    break;
                case FLV_CODEC_ID_VP6_ALPHA:
                    printf("    VP6ALPHAPACKET\n"); 
                    break;
                case FLV_CODEC_ID_SCREEN_V2:
                    printf("    SCREENV2PACKET\n");
                    break;
                default:
                    break;
            }
        }     
    }
    // frame info
    else
    {
        byte = 0;
        fread_1(&byte);
        if (byte == 0)
            printf("     Start of client-side seeking video frame sequence.\n");   
        else
            printf("     End of client-side seeking video frame sequence.\n"); 
        tag->data = NULL;
    } 
    
    return tag;
}

void init_avc_video_tag(avc_video_tag_t * tag)
{
    assert(tag != NULL);
    tag->avc_packet_type = 0;
    tag->composition_time = 0;
    tag->nalu_len = 0;
    tag->data = NULL; 
}
/*
 * @brief read AVC video tag
 */
avc_video_tag_t *read_avc_video_tag(video_tag_t *video_tag, flv_tag_t *flv_tag, uint32_t data_size) {
    avc_video_tag_t *tag = NULL;

    tag = malloc(sizeof(avc_video_tag_t));
    if (tag == NULL)
    {
        printf("line: %d, malloc error in function %s\n", __LINE__, __FUNCTION__);
        return NULL;
    } 

    init_avc_video_tag(tag);

    // AVCPacketType:UI8
    fread_1(&(tag->avc_packet_type));
    // CompositionTime:SI24
    fread_3(&(tag->composition_time));

    // if AVCPacketType == 1, one or more NALUS
    // 0x17|01|00 00 00|xx xx xx xx|
    if (tag->avc_packet_type == 1) 
    {
        // 4-byte, the actual length of the raw data(NALU)
        fread_4(&(tag->nalu_len));
    }

    printf("    AVC video tag:\n");
    printf("      AVC packet type: %u - %s\n", tag->avc_packet_type, avc_packet_types[tag->avc_packet_type]);
    printf("      AVC composition time: %i\n", tag->composition_time);
    // 0 = AVC sequence header
    // 1 = AVC NALU
    // 2 = AVC end of sequence (lower level NALU sequence ender is not required or supported)
    if (tag->avc_packet_type == 1)
    {
        printf("      AVC nalu length: %i\n", tag->nalu_len);
        tag->data = malloc((size_t) data_size - 1 - 3 - 4);
        if (tag->data == NULL)
        {
           printf("line: %d, malloc error in function %s\n", __LINE__, __FUNCTION__);
           free(tag);
           return NULL; 
        }
        size_t count = 0;
        count = fread(tag->data, 1, (size_t) data_size - 1 - 3 - 4, g_infile);
        if(check_read_error(__LINE__, __FUNCTION__, count, data_size - 8))
        {
            free(tag->data);
            free(tag);
            return NULL;
        }
    }
    else
    {
        tag->data = malloc((size_t) data_size - 1 - 3);
        size_t count = 0;
        count = fread(tag->data, 1, (size_t) data_size - 1 - 3, g_infile);
        if(check_read_error(__LINE__, __FUNCTION__, count, data_size - 4))
        {
            free(tag->data);
            free(tag);
            return NULL;
        }
    }
    return tag;
}
const char * check_property_name(const char *name)
{
    assert( NULL != name);
    if(strncmp(metadata_properties[1], name, strlen(name)) == 0 || 
       strncmp(metadata_properties[13], name, strlen(name)) ==0 ) 
       return postfix[0];
    if(strncmp(metadata_properties[2], name, strlen(name)) == 0 || 
       strncmp(metadata_properties[7], name, strlen(name)) ==0 ) 
       return postfix[1];
    if(strncmp(metadata_properties[3], name, strlen(name)) == 0) 
       return postfix[2];
    if(strncmp(metadata_properties[9], name, strlen(name)) == 0) 
       return postfix[3];
    if(strncmp(metadata_properties[10], name, strlen(name)) == 0 || 
       strncmp(metadata_properties[14], name, strlen(name)) ==0 ) 
       return postfix[4];
     if(strncmp(metadata_properties[8], name, strlen(name)) == 0) 
       return postfix[5];
     return NULL; 
}
void read_scriptdata_tag(void)
{
    // type: 0x02, length: 0x000A, "onMetaData"
    uint8_t type = 0;
    fread_1(&type);
    // length, because the byte order is BIG-ENDIAN
    // We should read the byte one by one
    uint16_t length;
    fread_2(&length);
    // "onMetaData"
    uint8_t name[10];
    size_t count = 0;
    count = fread(name, 1, 10, g_infile); 
    if (!feof(g_infile) && count != 10)
    {
        printf("line: %d, read error in function %s", __LINE__, __FUNCTION__);
        return; 
    }
    // Type:ScriptDataECMAArray, type: 0x08 
    fread_1(&type);
    // ECMAArrayLength(UI32):0x0000000C
    uint32_t ecma_array_length = 0;
    fread_4(&ecma_array_length);
    
    // Read the property one by one
    // ScriptDataObjectProperty: PropertyName(ScriptDataString), PropertyData(ScriptDataValue)
    for (int i = 0; i < ecma_array_length; ++i)
    {
        length = 0;
        fread_2(&length); // Length of PropertyName
        
        // For Debug
        // printf("the length of propertyname is: %d\n", length);
        char *property_name = NULL;
        // Consider the end identifier '\0'
        property_name = malloc(length + 1);
        
        if(property_name == NULL)
        {
            printf("line: %d, malloc error in function %s\n", __LINE__, __FUNCTION__);
            return;
        }
        // MUST Initialize the memory
        memset(property_name, 0, length + 1);

        fread(property_name, 1, length, g_infile);
        fread_1(&type);  // Type of PropertyData
        switch (type) {
            // Number: DOUBLE 8-byte
            case AMF_TYPE_NUMBER:
                {
                    double data1 = 0.0;
                    fread_double(&data1);
                    if (check_property_name(property_name) != NULL)
                         printf("    Property: %s - value: %.12g %s\n", property_name, data1, check_property_name(property_name));
                    else
                         printf("    Property: %s - value: %.12g\n", property_name, data1);
                }
               break;
            // Boolean: UI8
            case AMF_TYPE_BOOLEAN:
                {
                    uint8_t value = 0;
                    fread_1(&value);
                    printf("    Property: %s - value: %u\n", property_name, value);
                }
               break;
            // ScriptDataString 
            case AMF_TYPE_STRING:
                {
                    // get the length of ScriptDataString
                    fread_2(&length);
                    char *property_data_str = NULL;
                    property_data_str = malloc(length + 1);
                    if(property_data_str == NULL)
                    {
                        printf("line: %d, malloc error in function %s\n", __LINE__, __FUNCTION__);
                        free(property_name);
                        return;
                    }
                    // MUST Initialize the memory
                    memset(property_data_str, 0, length + 1);

                    count = fread(property_data_str, 1, length, g_infile);
                    if (!feof(g_infile) && count != length)
                    {
                        printf("line: %d, read error in function %s", __LINE__, __FUNCTION__);
                        return; 
                    }
                    printf("    Property: %s - value: %s", property_name, property_data_str);
                    free(property_data_str);
                    property_data_str = NULL;
                }
               break;
            default:
                break;
        }
        free(property_name);
        property_name = NULL; 
    }
    // ScirptDataObjectEND
    uint8_t end[3] = {0};
    count = fread(end, sizeof(uint8_t), 3, g_infile);
    if (!feof(g_infile) && count != 3)
    {
        printf("line: %d, read error in function %s", __LINE__, __FUNCTION__);
        return; 
    }
}
void flv_parser_init(FILE *in_file) {
    g_infile = in_file;
    tag_count = 0;
}
// main processing func
int flv_parser_run() {

    flv_read_header();

    for (; ;) {
        flv_tag_t *tag = NULL;
        tag = flv_read_tag(); // read the tag
        if (!tag) {
            return 0;
        }
        flv_free_tag(tag);    // and free it
        tag = NULL;
    }
}

void flv_free_tag(flv_tag_t *tag) {
    assert(tag != NULL); 
    if (tag->tag_type == TAGTYPE_VIDEODATA) {
        video_tag_t *video_tag;
        video_tag = (video_tag_t *) tag->data;
        if (video_tag->codec_id == FLV_CODEC_ID_AVC) {
            avc_video_tag_t *avc_video_tag;
            avc_video_tag = (avc_video_tag_t *) video_tag->data;
            free(avc_video_tag->data);
            free(video_tag->data);
            free(tag->data);
            free(tag);
        } else {
            free(video_tag->data);
            free(tag->data);
            free(tag);
        }
    } else if (tag->tag_type == TAGTYPE_AUDIODATA) {
        audio_tag_t *audio_tag;
        audio_tag = (audio_tag_t *) tag->data;
        free(audio_tag->data);
        free(tag->data);
        free(tag);
    } else {
        // free(tag->data);
        free(tag);
    }
}
void init_flv_header_t(flv_header_t * flv_header)
{
    if (!flv_header)
    {
        printf("line: %d, the parameter flv_header is NULL!", __LINE__);
        return;
    }
    int i = 0;
    for (; i < 3; i++)
        flv_header->signature[i] = 0;
    flv_header->version = 1;           // For FLV version 1, this value is 1.
    flv_header->type_flags = 0;
    flv_header->data_offset = 0;
}
int flv_read_header(void) {
    size_t count = 0;
    int i = 0;
    flv_header_t *flv_header = NULL;

    flv_header = malloc(sizeof(flv_header_t));
    if (!flv_header)
    {
        printf("line: %d, malloc error in function: %s", __LINE__, __FUNCTION__);
        exit(2);
    }
    init_flv_header_t(flv_header);

    count = fread(flv_header, sizeof(flv_header_t), 1, g_infile);
    
    if (!feof(g_infile) && count != 1)
    {
        printf("line: %d,reading file error in function: %s", __LINE__, __FUNCTION__);
        exit(3);
    }

    for (i = 0; i < strlen(flv_signature); i++) {
        assert(flv_header->signature[i] == flv_signature[i]);
    }
    // FLV files shall store multi-byte numbers in big-endian byte order
    flv_header->data_offset = ntohl(flv_header->data_offset);

    flv_print_header(flv_header);

    free(flv_header);
    return 0;

}

void print_general_tag_info(flv_tag_t *tag) {
    assert(NULL != tag);
    printf("  Data size: %lu\n", (unsigned long) tag->data_size);
    printf("  Timestamp: %lu\n", (unsigned long) tag->timestamp);
    printf("  Timestamp extended: %u\n", tag->timestamp_ext);
    printf("  StreamID: %lu\n", (unsigned long) tag->stream_id);

    return;
}
void init_flv_tag(flv_tag_t *tag)
{
   assert(tag != NULL);
   tag->filter = 0;
   tag->tag_type = 0;
   tag->data_size = 0;
   tag->timestamp = 0;
   tag->timestamp_ext = 0;
   tag->stream_id = 0;
   tag->data = NULL;
}
// FLV File Body
// PreviousTagSize0   UI32    (Always 0)
// Tag1               FLVTAG
// PreviousTagSize1   UI32
// Tag2               FLVTAG
// ...
// PreviousTagSizeN-1 UI32
// TagN               FLVTAG
// PreviousTagSizeN   UI32
flv_tag_t *flv_read_tag(void) {
    uint32_t prev_tag_size = 0;
    flv_tag_t *tag = NULL;
    uint8_t first_byte = 0;

    tag = malloc(sizeof(flv_tag_t));
    if (!tag)
    {
        printf("line: %d, malloc error in function: %s", __LINE__, __FUNCTION__);
        return NULL;
    }

    init_flv_tag(tag);

    fread_4(&prev_tag_size);

    printf("\n");
    printf("PreviousTagSize%u: %lu\n", tag_count, (unsigned long) prev_tag_size);

    size_t count = 0;
    // Start reading next tag
    count = fread_1(&first_byte); 
    if (count == 0) 
    {
        free(tag);
        return NULL;
    }
    
    tag->filter = (first_byte & (1 << 4)); // Filter == 1, other process need to add 
    tag->tag_type = (first_byte & 0x1F);   
    // count = fread_1(&(tag->tag_type));
    fread_3(&(tag->data_size));
    fread_3(&(tag->timestamp));
    fread_1(&(tag->timestamp_ext));
    fread_3(&(tag->stream_id));

    tag_count++;
    
    printf("Tag%u\n",tag_count);
    printf("Tag type: %u - ", tag->tag_type);
    switch (tag->tag_type) {
        case TAGTYPE_AUDIODATA:
            printf("Audio data\n");
            print_general_tag_info(tag);
            tag->data = (void *) read_audio_tag(tag);
            break;
        case TAGTYPE_VIDEODATA:
            printf("Video data\n");
            print_general_tag_info(tag);
            tag->data = (void *) read_video_tag(tag);
            break;
        case TAGTYPE_SCRIPTDATAOBJECT:
            printf("Script data object\n");
            print_general_tag_info(tag);                     // Parse the metadata info  
            read_scriptdata_tag();
            break;
        default:
            printf("Unknown tag type!\n");
            die();
    }
    return tag;
}
