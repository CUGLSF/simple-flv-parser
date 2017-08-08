#ifndef FLV_PARSER_H_
#define FLV_PARSER_H_ 

#include <stdint.h>
#include <stdio.h>

#define FLV_HEADER_AUDIO_BIT (2)
#define FLV_HEADER_VIDEO_BIT (0)

#define FLV_CODEC_ID_H263          (2)
#define FLV_CODEC_ID_SCREEN        (3)
#define FLV_CODEC_ID_VP6           (4)
#define FLV_CODEC_ID_VP6_ALPHA     (5)
#define FLV_CODEC_ID_SCREEN_V2     (6)
#define FLV_CODEC_ID_AVC           (7)

/* AMF data types */
enum amf_data_types {
    AMF_TYPE_NUMBER = 0,            
    AMF_TYPE_BOOLEAN,	        
    AMF_TYPE_STRING	    
};

enum tag_types {
    TAGTYPE_AUDIODATA = 8,
    TAGTYPE_VIDEODATA = 9,
    TAGTYPE_SCRIPTDATAOBJECT = 18
};

/*
 * @brief flv file header 9 bytes
 */
struct flv_header {
    uint8_t signature[3]; // 0x46, 0x4C, 0x56(FLV), UI8
    uint8_t version;      // For FLV version 1, this vaule is 1. UI8
    uint8_t type_flags;   // TypeFlagsReserved: UB[5]  Shall be 0
                          // TypeFlagsAudio:    UB[1]  1 = Audio tags are present
                          // TypeFlagsReserved: UB[1]  Shall be 0
                          // TypeFlagsVideo:    UB[1]  1 = Video tags are present 
    uint32_t data_offset; // Length of FLV header in bytes, UI32. For FLV version 1,
                          // this field has a value of 9.
} __attribute__((__packed__));

typedef struct flv_header flv_header_t;

/*
 * @brief flv tag general header 11 bytes, the FLV tag
 * contains metadata for audio,video,or script,optional encryption metadata, and 
 * the payload.
 */
struct flv_tag {
    uint8_t filter;          // Filter, 0 = No pre-processing required;1 = Pre-processing required. UB[1]
    uint8_t tag_type;        // Reserved for FMS, should be 0. UB[2]
                             // TagType, 8 = audio; 9 = video; 18 = script data. UB[5]
    uint32_t data_size;      // DataSize, length of the message. Number of bytes after StreamID to end of tag. UI24
    uint32_t timestamp;      // Timestamp, in milliseconds. In the first tag, this value is 0. UI24
    uint8_t timestamp_ext;   // TimestampExtended, extend to SI32 value. Represent the upper 8 bits. UI8
    uint32_t stream_id;      // StreamID, always 0. UI24
    void *data;              // Up to the TagType, indicates it's an audio_tag or video_tag
};

typedef struct flv_tag flv_tag_t;

typedef struct audio_tag {
    uint8_t sound_format;    // SoundFormat, format of SoundData. UB[4] 
                             // 0 - Linear PCM, platform endian; 1 - ADPCM, 2 - MP3, 4 - Nellymoser 16 KHz mono, 5 - Nellymoser 8 KHz mono, 10 - AAC, 11 - Speex
    uint8_t sound_rate;      // Sampling rate. UB[2]
                             // 0 - 5.5 KHz, 1 - 11 KHz, 2 - 22 KHz, 3 - 44 KHz
    uint8_t sound_size;      // SoundSize, size of audio sample. UB[1] 
                             // 0 - 8 bit, 1 - 16 bit. 
    uint8_t sound_type;      // Mono or stereo sound. UB[1]
                             // 0 - mono, 1 - stereo
    void *data;
} audio_tag_t;

typedef struct video_tag {
    uint8_t frame_type;      // FrameType, UB[4]
                             // 1 = key frame(for AVC, a seekable frame)
                             // 2 = inter frame(for AVC, a non-seekable frame)
                             // 3 = disponsable inter frame(H.263 only)
                             // 4 = generated key frame(reserved for server use only)
                             // 5 = video info/command frame
    uint8_t codec_id;        // CodecID, UB[4]
                             // 2 = Sorenson H.263
                             // 3 = Screen video
                             // 4 = On2 VP6
                             // 5 = On2 VP6 with alpha channel
                             // 6 = Screen video version 2
                             // 7 = AVC
    void *data;
} video_tag_t;

typedef struct avc_video_tag {
    uint8_t avc_packet_type;   // AVCPacketType, up to CodecID, if CodecID == 7, UI8.
                               // 0 = AVC sequence header
                               // 1 = AVC NALU
                               // 2 = AVC end of sequence(lower level NALU sequence ender isn't required
                               // or supported)
    uint32_t composition_time; // Up to CodecID, if CodecID == 7, SI24.
                               // IF AVCPacketType == 1, Composition time offset 
                               // ELSE 0
    uint32_t nalu_len;
    void *data;
} avc_video_tag_t;

int flv_read_header(void);

flv_tag_t *flv_read_tag(void);

void flv_print_header(flv_header_t *flv_header);

audio_tag_t *read_audio_tag(flv_tag_t *flv_tag);

video_tag_t *read_video_tag(flv_tag_t *flv_tag);

avc_video_tag_t *read_avc_video_tag(video_tag_t *video_tag, flv_tag_t *flv_tag, uint32_t data_size);

void read_scriptdata_tag(void);

uint8_t flv_get_bits(uint8_t value, uint8_t start_bit, uint8_t count);

size_t check_read_error(int line_num, const char * func_name, int count, int read_bytes);

size_t fread_1(uint8_t *ptr);

void fread_3(uint32_t *ptr);

void fread_4(uint32_t *ptr);

void fread_double(double *ptr);

const char * check_property_name(const char *name);

void flv_free_tag(flv_tag_t *tag);

void flv_parser_init(FILE *in_file);

int flv_parser_run(void);

#endif // FLV_PARSER_H_
