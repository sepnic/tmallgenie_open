// Copyright (c) 2022 Qinglong<sysu.zqlong@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "speex/speex.h"
#include "speex/speex_header.h"
#include "speex/speex_stereo.h"
#include "ogg/ogg.h"

#define MAX_FRAME_SIZE 960
#define MAX_FRAME_BYTES 960

#define SAMPLE_RATE 16000
#define SAMPLE_BITS 16
#define CHANNEL_COUNT 1

/*Write an Ogg page to a file pointer*/
int oe_write_page(ogg_page *page, FILE *fp)
{
   int written;
   written = fwrite(page->header,1,page->header_len, fp);
   written += fwrite(page->body,1,page->body_len, fp);
   return written;
}

static int read_samples(FILE *fin,int frame_size, short * input)
{
   size_t to_read = SAMPLE_BITS/8*CHANNEL_COUNT*frame_size;
   int nb_read = fread(input,1,to_read,fin);
   nb_read /= SAMPLE_BITS/8*CHANNEL_COUNT;
   return nb_read;
}

int main(int argc, char **argv)
{
   char *inFile = "test.wav";
   char *outFile = "test.ogg";
   FILE *fin = fopen(inFile, "rb");
   if (!fin) {
      perror(inFile);
      exit(1);
   }
   FILE *fout = fopen(outFile, "wb");
   if (!fout) {
      perror(outFile);
      exit(1);
   }

   /*Initialize Ogg stream struct*/
   ogg_stream_state os;
   ogg_page og;
   ogg_packet op;
   srand(time(NULL));
   if (ogg_stream_init(&os, rand()) < 0)
   {
      fprintf(stderr,"Error: stream init failed\n");
      exit(1);
   }

   SpeexHeader header;
   const SpeexMode *mode = speex_lib_get_mode (SPEEX_MODEID_NB);
   speex_init_header(&header, SAMPLE_RATE, CHANNEL_COUNT, mode);
   header.frames_per_packet=1;
   header.vbr=0;
   header.nb_channels = CHANNEL_COUNT;

   /*Initialize Speex encoder*/
   void *st = speex_encoder_init(mode);
   spx_int32_t frame_size;
   speex_encoder_ctl(st, SPEEX_GET_FRAME_SIZE, &frame_size);
   spx_int32_t complexity=2;
   speex_encoder_ctl(st, SPEEX_SET_COMPLEXITY, &complexity);
   spx_int32_t quality=8;
   speex_encoder_ctl(st, SPEEX_SET_QUALITY, &quality);
   spx_int32_t rate = SAMPLE_RATE;
   speex_encoder_ctl(st, SPEEX_SET_SAMPLING_RATE, &rate);
   spx_int32_t lookahead = 0;
   speex_encoder_ctl(st, SPEEX_GET_LOOKAHEAD, &lookahead);

   /*Write header*/
   {
      int packet_size;
      op.packet = (unsigned char *)speex_header_to_packet(&header, &packet_size);
      op.bytes = packet_size;
      op.b_o_s = 1;
      op.e_o_s = 0;
      op.granulepos = 0;
      op.packetno = 0;
      ogg_stream_packetin(&os, &op);
      speex_header_free(op.packet);

      while (ogg_stream_flush(&os, &og) != 0)
      {
         if(oe_write_page(&og, fout) != og.header_len + og.body_len)
         {
            fprintf (stderr,"Error: failed writing header to output stream\n");
            exit(1);
         }
      }
   }

   short input[MAX_FRAME_SIZE];
   char cbits[MAX_FRAME_BYTES];
   int id=0;
   int eos=0;

   SpeexBits bits;
   speex_bits_init(&bits);
   /*Main encoding loop (one frame per iteration)*/
   while (eos == 0)
   {
      id++;
      if (read_samples(fin,frame_size,input)==0)
         eos=1;

      /*Encode current frame*/
      if (CHANNEL_COUNT==2)
         speex_encode_stereo_int(input, frame_size, &bits);
      speex_encode_int(st, input, &bits);
      speex_bits_insert_terminator(&bits);
      int nbBytes = speex_bits_write(&bits, cbits, sizeof(cbits));
      speex_bits_reset(&bits);
      op.packet = (unsigned char *)cbits;
      op.bytes = nbBytes;
      op.b_o_s = 0;
      op.e_o_s = eos;
      op.granulepos = id*frame_size-lookahead;
      op.packetno = id;
      ogg_stream_packetin(&os, &op);

      /*Write all new pages (most likely 0 or 1)*/
      while (ogg_stream_pageout(&os,&og) != 0)
      {
         if (oe_write_page(&og, fout) != og.header_len + og.body_len)
         {
            fprintf (stderr,"Error: failed writing header to output stream\n");
            exit(1);
         }
      }
   }

   speex_encoder_destroy(st);
   speex_bits_destroy(&bits);
   ogg_stream_clear(&os);

   fclose(fin);
   fclose(fout);
   return 0;
}
