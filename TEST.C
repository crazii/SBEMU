#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <assert.h>
#include <PLATFORM.H>
#include <SBEMUCFG.H>
#include <MPXPLAY.H>
#include <AU_MIXER/MIX_FUNC.H>

typedef struct WAVHEADER {
    unsigned char riff[4];                          // RIFF string
    unsigned long overall_size;                      // overall size of file in bytes
    unsigned char wave[4];                          // WAVE string
    unsigned char fmt_chunk_marker[4];              // fmt string with trailing null char
    unsigned long length_of_fmt;                     // length of the format data
    unsigned short format_type;                       // format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
    unsigned short channels;                          // no.of channels
    unsigned long sample_rate;                       // sampling rate (blocks per second)
    unsigned long byterate;                          // SampleRate * NumChannels * BitsPerSample/8
    unsigned short block_align;                       // NumChannels * BitsPerSample/8
    unsigned short bits_per_sample;                   // bits per sample, 8- 8bits, 16- 16 bits etc
    unsigned long data_chunk_header;            // DATA string or FLLR string
    unsigned long data_size;                         // NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
}WAVHEADER;

extern mpxplay_audioout_info_s aui;

int16_t* TEST_Sample;
unsigned int TEST_SampleLen;

void TestSound(BOOL play)
{
    FILE* fp = fopen("test.wav", "rb");
    if(!fp)
    {
        printf("failed to open file.\n");
        return;
    }
    WAVHEADER header;
    if( fread(&header, sizeof(header), 1, fp) != 1)
    {
        printf("failed to read file header.\n");
        fclose(fp);
        return;
    }

    printf("Fmt: %x, Channels: %d, Samplerate: %d, Byterate: %d, Bitpersample: %d\n", header.format_type, header.channels, header.sample_rate, header.byterate, header.bits_per_sample);
    unsigned int samplecount = header.data_size * 8 / header.bits_per_sample;

    while(header.data_chunk_header != EndianSwap32(0x64617461)) //big endian 'data'
    {
        fseek(fp, header.data_size, SEEK_CUR);
        if(fread(&header.data_chunk_header, 8, 1, fp) != 1)
        {
            printf("failed to find data chunk.\n");
            fclose(fp);
            return;
        }
    }
    printf("sample count: %d, data size: %d\n", samplecount, header.data_size);
    int buffsize = (header.sample_rate < 44100) ? (((float)samplecount*44100+header.sample_rate-1)/header.sample_rate) : samplecount; //reserved space for frequency conversion
    assert(buffsize >= samplecount);

    short* samples = (short*)malloc(max(buffsize*2*2, header.data_size)); //16bit 2 channels
    if(fread(samples, header.data_size, 1, fp) != 1)
    {
        printf("failed to read file data.\n");
        fclose(fp);
        return;
    }

    if(header.bits_per_sample != 16)
        cv_bits_n_to_m(samples, samplecount, header.bits_per_sample/8, 2);
    header.bits_per_sample = 16;

    if(header.channels == 1)
    {
        cv_channels_1_to_n(samples, samplecount, 2, 2);
        samplecount *= 2;
    }
    header.channels = 2;

    mpxplay_audio_decoder_info_s adi = {NULL, 0, 1, SBEMU_SAMPLERATE, header.channels, header.channels, NULL, header.bits_per_sample, header.bits_per_sample/8, 0};
    AU_setrate(&aui, &adi);
    
    if(aui.freq_card != header.sample_rate) //soundcard not supported
    {
        printf("frequency: %d => %d\n", header.sample_rate, aui.freq_card);
        samplecount = mixer_speed_lq(samples, samplecount, header.channels, header.sample_rate, aui.freq_card);
    }
    TEST_Sample = samples;
    TEST_SampleLen = samplecount;

    if(play)
    {
        AU_prestart(&aui);
        AU_start(&aui);
    
        aui.samplenum = samplecount;
        aui.pcm_sample = samples;
        AU_writedata(&aui);
        AU_wait_and_stop(&aui);
    }
}