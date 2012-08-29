/******************************************************************************
 * gpucomp.h
 *
 * Public header file - defines the external interface of the gpu composition
 *    - type and constant definitions
 *    - config structures of video and gfx planes
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 *
 *   Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * mmurthy@ti.com
 ****************************************************************************/
#ifndef __GPUCOMP_H__
#define __GPUCOMP_H__


#define GFX_CONFIG_NAMED_PIPE    "/opt/gpu-compositing/named_pipes/gfx_cfg_plane_X"

#define VIDEOCONFIG_FIFO_NAME "/opt/gpu-compositing/named_pipes/video_cfg_plane_X"
#define VIDEODATA_FIFO_NAME "/opt/gpu-compositing/named_pipes/video_data_plane_X"

#define MAX_GFX_PLANES 4
#define MAX_VID_PLANES 4

#define DEBUGGPUVSINK
#define DEBUGGPUCOMP

/* Graphics Plane Config Structure */
#define BC_FOURCC(a,b,c,d) \
    ((unsigned long) ((a) | (b)<<8 | (c)<<16 | (d)<<24))

#define BC_PIX_FMT_RGB565   BC_FOURCC('R', 'G', 'B', 'P') /*RGB 5:6:5*/
#define BC_PIX_FMT_ARGB     BC_FOURCC('A', 'R', 'G', 'B') /*ARGB 8:8:8:8*/
typedef struct
{
    int enable;                      /* 0 - disable 1 - enable the gfx plane */
    int input_params_valid;          /* 1 - valid i/p parameters 0 - invalid */
    struct in_g { 
        unsigned long data_ph_addr;  /* physical address of gfx plane buffer  */
        int width;                   /* Input gfx plane width in pixels       */
        int height;                  /* output gfx plane width in pixels      */
        unsigned int pixel_format;   /* fourcc pixel format                   */
        int enable_blending;         /* 1 - blending enabled  0 - disabled    */
        int enable_global_alpha;     /* 1 - global alpha enabled 0 - disabled */
                                     /* pixel level alpha gets disabled with  */
                                     /* enable of global alpha                */
        float global_alpha;          /* global alpha value                    */
        float rotate;
    } in_g;
  
   /* output position and width device normalized values */
    int output_params_valid;         /* 1 - valid i/p parameters 0 - invalid  */
    struct out_g {
        float xpos;   /* x position-device normalized co-ordinate[-1.0 to 1.0]*/
        float ypos;   /* y position-device normalized co-ordinate[-1.0 to 1.0]*/
        float width;  /* o/p width  - device normalized co-ordinate[-1.0 to 1.0]*/
        float height; /* o/p height - device normalized co-ordinate[-1.0 to 1.0]*/
    } out_g;
} gfxCfg_s;

typedef struct
{
    int buf_id;
} videoData_s;

#define MAX_VIDEO_BUFFERS_PER_CHANNEL 16
typedef struct 
{
    int config_data;
    int buf_index;
    int enable;
    unsigned int channel_no;
    struct in {
        float rotate;
        int count;         /* Number of buffers */
        int width;         /* frame width in pixels */
        int height;        /* frame height in pixels */
        unsigned int fourcc;    /*buffer pixel format*/
        unsigned long phyaddr[MAX_VIDEO_BUFFERS_PER_CHANNEL];
    } in;
    struct out {
        float  xpos;
        float  ypos;
        float  width;
        float  height;
    } out;
} videoConfig_s;

#endif /* __GPUCOMP_H__ */
