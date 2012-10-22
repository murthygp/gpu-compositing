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

//#define DEBUG

#ifdef  DEBUG
#define DEBUG_PRINTF(x) printf x
#else
#define DEBUG_PRINTF(x)
#endif


#define GFX_CONFIG_NAMED_PIPE    "/opt/gpu-compositing/named_pipes/gfx_cfg_plane_X"

#define VIDEO_CONFIG_AND_DATA_FIFO_NAME "/opt/gpu-compositing/named_pipes/video_cfg_and_data_plane_X"
#define VIDEODATA_FIFO_NAME "/opt/gpu-compositing/named_pipes/video_data_plane_X"

#define MAX_GFX_PLANES 4
#define MAX_VID_PLANES 4

/* GFX plane configuration delay in milliseconds to accomodate for the initial
   scene draw time by Qt */
#define GFX_CONFIG_DELAY_MS 1000 

/* Graphics Plane Config Structure */
#define BC_FOURCC(a,b,c,d) \
    ((unsigned long) ((a) | (b)<<8 | (c)<<16 | (d)<<24))

#define BC_PIX_FMT_RGB565   BC_FOURCC('R', 'G', 'B', 'P') /*RGB 5:6:5*/
#define BC_PIX_FMT_ARGB     BC_FOURCC('A', 'R', 'G', 'B') /*ARGB 8:8:8:8*/
typedef struct
{
    int enable;                      /* 1 - enable the gfx plane; 0 - disable */
    int input_params_valid;          /* 1 - valid i/p parameters; 0 - invalid */
    struct in_g { 
        unsigned long data_ph_addr;  /* physical address of the gfx  buffer   */
        int width;                   /* gfx plane width in pixels             */
        int height;                  /* gfx plane height in pixels            */
        unsigned int pixel_format;   /* fourcc pixel format                   */
        int enable_blending;         /* 1 - blending enabled;  0 - disabled   */
        int enable_global_alpha;     /* 1 - global alpha;  0 - pixel alpha    */
        float global_alpha;          /* global alpha value [0.0 to 1.0]       */
        float rotate;                /* rotate angle in decimal degrees [-180.0 to 180.0]*/
    } in_g;
  
   /* output window position and resolution in normalized device co-ordinates */
    int output_params_valid;         /* 1 - valid o/p parameters; 0 - invalid */
    struct out_g {
        float xpos;   /* x position [-1.0 to 1.0] */
        float ypos;   /* y position [-1.0 to 1.0] */
        float width;  /*  width  - [0.0 to 2.0], 2.0 correspond to fullscreen width */
        float height; /*  height - [0.0 to 2.0], 2.0 correspond to fullscreen height */
    } out_g;
} gfxCfg_s;

#define MAX_VIDEO_BUFFERS_PER_CHANNEL 16
typedef struct 
{
    int config_data;   /* 1 - config   0 - data */
    int buf_index;     /* if data, buffer index */
    int enable;        /* 1 - enable the video plane; 0 - disable */

    /* Video plane config structure */
    struct in {
        float rotate;  /* rotate angle in decimal degrees [-180.0 to 180.0]*/
        int count;     /* Number of video buffers */
        int width;     /* video frame width in pixels */
        int height;    /* video frame height in pixels */
        unsigned int fourcc;    /* pixel format */
        unsigned long phyaddr[MAX_VIDEO_BUFFERS_PER_CHANNEL]; /* Physical addresses of video buffers */
    } in;

    /* output video window position and resolution in normalized device co-ordinates */
    struct out {
        float  xpos;   /* x position [-1.0 to 1.0] */
        float  ypos;   /* y position [-1.0 to 1.0] */
        float  width;  /*  width  - [0.0 to 2.0], 2.0 correspond to fullscreen width */
        float  height; /*  height - [0.0 to 2.0], 2.0 correspond to fullscreen height */
    } out;
} videoConfig_s;

#endif /* __GPUCOMP_H__ */
