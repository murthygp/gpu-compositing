/*****************************************************************************
 * main.c
 *   Multi-channel Video Streaming and blending with Graphics planes using 
 *   IMG proprietary Texture Streaming Extension 
 *   - Based on OpenGL ES 2.0
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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <cmem.h>
#include <getopt.h>
#include "../gpucomp.h"
#include <pthread.h>
#include <math.h>

#include "common.h"

#define GL_TEXTURE_STREAM_IMG  0x8C0D
#define MAX_TEX_BUFS 16

#define FILE_RAW_VIDEO

#ifdef FILE_RAW_VIDEO
static CMEM_AllocParams params = { CMEM_POOL, CMEM_NONCACHED, 4096 };


// Pre-calculated value of PI / 180.
#define kPI180   0.017453

// Pre-calculated value of 180 / PI.
#define k180PI  57.295780

// Converts degrees to radians.
#define degreesToRadians(x) (x * kPI180)

// Converts radians to degrees.
#define radiansToDegrees(x) (x * k180PI)

typedef float mat4[16]; 


    mat4 matgfx[MAX_GFX_PLANES]; 
    mat4 matvid[MAX_VID_PLANES];

    void matrixIdentity(mat4 m)
    {
         m[0] = m[5] = m[10] = m[15] = 1.0;
         m[1] = m[2] = m[3] = m[4] = 0.0;
         m[6] = m[7] = m[8] = m[9] = 0.0;
        m[11] = m[12] = m[13] = m[14] = 0.0;
     }


    void matrixRotateX(float degrees, mat4 matrix)
    {
        float radians = degreesToRadians(degrees);
        matrixIdentity(matrix);
       // Rotate X formula.
       matrix[5] = cosf(radians);
       matrix[6] = -sinf(radians);
       matrix[9] = -matrix[6];
       matrix[10] = matrix[5];
     }

void matrixRotateY(float degrees, mat4 matrix) 
{     
    float radians = degreesToRadians(degrees);
    matrixIdentity(matrix);
    // Rotate Y formula.
    matrix[0] = cosf(radians);
    matrix[2] = sinf(radians);
    matrix[8] = -matrix[2];
    matrix[10] = matrix[0]; 
}
void matrixRotateZ(float degrees, mat4 matrix)
 {
     float radians = degreesToRadians(degrees);
      matrixIdentity(matrix);
      // Rotate Z formula. 
    matrix[0] = cosf(radians);
    matrix[1] = sinf(radians);
     matrix[4] = -matrix[1];
     matrix[5] = matrix[0]; 
}

/* vertices for the file raw video */
GLfloat rect_vertices_file_vid[6][3] =
{   // x     y     z

   /* 1st Traingle */
    {-1.0,  1.0,  0.0}, // 0
    {-1.0, -1.0,  0.0}, // 1
    { 1.0,  1.0,  0.0}, // 2

   /* 2nd Traingle */
    { 1.0,  1.0,  0.0}, // 1
    {-1.0, -1.0,  0.0}, // 0
    { 1.0, -1.0,  0.0}, // 2
};
#endif

/* shader global objects */
static int ver_shader, frag_shader;
int program;
static int setup_shaders();
char buf[1024];

/* Graphics Planes Global variables */ 
pthread_t     gfxtid[MAX_GFX_PLANES];
gfxCfg_s  gfxCfg[MAX_GFX_PLANES];
int gfx_plane_mdfd[MAX_GFX_PLANES] = {-1, -1, -1, -1};

/* Video Planes Global varibles */
pthread_t     vidCfgtid[MAX_VID_PLANES];
videoConfig_s vidCfg[MAX_VID_PLANES];
int           vid_plane_mdfd[MAX_VID_PLANES] = {-1, -1, -1, -1};
int           vid_data_idx [MAX_VID_PLANES] = {0, 0, 0, 0};

/* Vertex shader source */
const char * vshader_src = " \
    attribute vec4 vPosition; \
    attribute mediump vec2  inTexCoord; \
    varying mediump vec2    TexCoord; \
    uniform mediump mat4        matrix;\
    void main() \
    {\
        gl_Position = matrix*vPosition;\
        TexCoord = inTexCoord; \
    }";

/* Fragment shader source */
static const char * fshader_src_palpha_single_texture =
    "#ifdef GL_IMG_texture_stream2\n"
    "#extension GL_IMG_texture_stream2 : enable\n"
    "#endif\n"
    "varying mediump vec2 TexCoord;\n"
    "uniform samplerStreamIMG sTexture;\n"
    "void main(void)\n"
    "{\n" 
        " gl_FragColor = textureStreamIMG(sTexture, TexCoord); \n"
    "}";

/* Vertices for the video planes */
GLfloat rect_vertices_vid[MAX_VID_PLANES][6][3] =

{   // x     y     z
   /* Video Plane 0 */
   /* ---------------- */
   /* 1st Traingle */
   {{-1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0,  1.0,  0.0},

   /* 2nd Traingle */
    { 1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0, -1.0,  0.0}},


   /* Video Plane 1 */
   /* -----------------*/
   /* 1st Traingle */
   {{-1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0,  1.0,  0.0},

   /* 2nd Traingle */
    { 1.0,  1.0,  0.0}, // 1
    {-1.0, -1.0,  0.0}, // 0
    { 1.0, -1.0,  0.0}}, // 2


   /* Video Plane 2 */
   /*------------------*/
   /* 1st Traingle */
   {{-1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0,  1.0,  0.0},

   /* 2nd Traingle */
    { 1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0, -1.0,  0.0}},

   /* Video Plane 3 */
   /*------------------*/
   /* 1st Traingle */
   {{-1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0,  1.0,  0.0},

   /* 2nd Traingle */
    { 1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0, -1.0,  0.0}}
};

/* Vertices for the Graphics planes */
GLfloat rect_vertices_gfx[MAX_GFX_PLANES][6][3] = 

{   // x     y     z
   /* Graphics Plane 0 */
   /* ---------------- */
   /* 1st Traingle */
   {{-1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0,  1.0,  0.0},

   /* 2nd Traingle */
    { 1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0, -1.0,  0.0}},


   /* Graphics Plane 1 */
   /* -----------------*/
   /* 1st Traingle */
   {{-1.0,  1.0,  0.0}, 
    {-1.0, -1.0,  0.0}, 
    { 1.0,  1.0,  0.0},

   /* 2nd Traingle */
    { 1.0,  1.0,  0.0}, // 1
    {-1.0, -1.0,  0.0}, // 0
    { 1.0, -1.0,  0.0}}, // 2


   /* Graphics Plane 2 */
   /*------------------*/
   /* 1st Traingle */
   {{-1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0},
    { 1.0,  1.0,  0.0},

   /* 2nd Traingle */
    { 1.0,  1.0,  0.0},
    {-1.0, -1.0,  0.0}, 
    { 1.0, -1.0,  0.0}},


   /* Graphics Plane 3 */
   /*------------------*/
   /* 1st Traingle */
   {{-1.0,  1.0,  0.0}, 
    {-1.0, -1.0,  0.0},
    { 1.0,  1.0,  0.0},

   /* 2nd Traingle */
    { 1.0,  1.0,  0.0}, 
    {-1.0, -1.0,  0.0}, 
    { 1.0, -1.0,  0.0}} 

};

/* Texture Co-ordinates  common for all the Graphics and Video planes */
GLfloat rect_texcoord[6][2] =
{   // x     y     z  alpha

   /* 1st Traingle */
    { 0.0, 0.0},
    { 0.0, 1.0},
    { 1.0, 0.0},

   /* 2nd Traingle */
    { 1.0,  0.0}, 
    { 0.0,  1.0},
    { 1.0,  1.0},

};

void usage(char *arg)
{
    printf("Usage:\n"
           "  %s \n"
           "\t-f  File video 1 - enable 0 - disbale <default> \n" 
           "\t-i  Raw video input file - YUV422ILE format <default: akiyo_d1_422.yuv>\n"
           "\t-a  File video frame width in pixels <default: 720> \n"
           "\t-b  File video frame height in pixels <default: 480>\n"
           "\t-p  profiling     1 - Enable \n"
           "\t                  0 - Disable <default> \n"
           "\t-l  x_pos of output video window  - normalized Device co-ordinates (-1.0 to +1.0) \n"
           "\t-m  y_pos of output video window  - Normalized Device Co-ordinates (-1.0 to +1.0) \n"
           "\t-n  Output video window width  - Normalized Device Co-ordinates (-1.0 to +1.0) \n"
           "\t-o  Output video window height - Normalized Device Co-ordinates (-1.0 to +1.0) \n"
           "\t-h - print this message\n\n", arg);
}

/* Config thread to receive configuration for GFX planes  */
void * gfxThread ( void *threadarg)
{
    int   n, fd_gfxplane;
    float xpos, ypos, width, height;
    int   gfx_plane_no;
    gfxCfg_s gfxCfgRecvd;
    char  gfx_config_fifo[] = GFX_CONFIG_NAMED_PIPE;

    gfx_plane_no = *(int *)threadarg;

    /* Seperate named pipe for each Graphics Plane */
    gfx_config_fifo[strlen(gfx_config_fifo)-1] = '0' + gfx_plane_no;

    while (1) {
#ifdef DEBUGGPUCOMP
        printf (" Opening the Named Pipe For GFX plane Config:%d %s\n", gfx_plane_no, gfx_config_fifo);
#endif

        fd_gfxplane = open(gfx_config_fifo, O_RDONLY);
        if (fd_gfxplane < 0)
        {
            printf (" Failed to open named pipe %s\n", gfx_config_fifo);
            exit(0);
        }

#ifdef DEBUGGPUCOMP
        printf (" Opened the Named Pipe For GFX plane Config:%d %s\n", gfx_plane_no, gfx_config_fifo);
#endif

        while(1)
        {
            /* receive config command from named pipe */
            n = read(fd_gfxplane, &gfxCfgRecvd, sizeof(gfxCfgRecvd));
           
            /* Check for the closure of pipe at the writer side */
            /* disable the plnae and break the look             */
            if (n == 0) 
            {
                gfxCfg[gfx_plane_no].enable = 0;
                break; 
            }

            gfxCfg[gfx_plane_no] = gfxCfgRecvd;

            /* Set up to process the input parameters if they are valid only */
            if (gfxCfgRecvd.input_params_valid)
            {
                gfx_plane_mdfd[gfx_plane_no] = 1;
            }
            
            /* process the output parameters if they are valid only  */
            /* Calculate the vertices based on the output parameters */
            if (gfxCfgRecvd.output_params_valid) 
            {  
                xpos   = gfxCfgRecvd.out_g.xpos;
                ypos   = gfxCfgRecvd.out_g.ypos;
                width  = gfxCfgRecvd.out_g.width;
                height = gfxCfgRecvd.out_g.height;

 
                rect_vertices_gfx [gfx_plane_no][0][0] = xpos;
                rect_vertices_gfx [gfx_plane_no][0][1] = ypos;

                rect_vertices_gfx [gfx_plane_no][1][0] = xpos;
                rect_vertices_gfx [gfx_plane_no][1][1] = ypos - height;

                rect_vertices_gfx [gfx_plane_no][2][0] = xpos + width;
                rect_vertices_gfx [gfx_plane_no][2][1] = ypos;

                rect_vertices_gfx [gfx_plane_no][3][0] = xpos + width;
                rect_vertices_gfx [gfx_plane_no][3][1] = ypos;

                rect_vertices_gfx [gfx_plane_no][4][0] = xpos;
                rect_vertices_gfx [gfx_plane_no][4][1] = ypos - height;

                rect_vertices_gfx [gfx_plane_no][5][0] = xpos + width;
                rect_vertices_gfx [gfx_plane_no][5][1] = ypos - height;
            }   
        }
    }
}

/* Config thread to receive configuration for Video planes  */
void * vidConfigDataThread ( void *threadarg)
{
    int   n, fd_vidplane;
    float xpos, ypos, width, height;
    int   vid_plane_no;
    videoConfig_s vidCfgRecvd;
    char  vid_config_fifo[] = VIDEO_CONFIG_AND_DATA_FIFO_NAME;

    vid_plane_no = *(int *)threadarg;

    /* Seperate named pipe for each video plane */
    vid_config_fifo[strlen(vid_config_fifo)-1] = '0' + vid_plane_no;

  while (1) {
#ifdef DEBUGGPUCOMP
    printf (" Opening the Named Pipe For Video plane Config:%d %s\n", vid_plane_no, vid_config_fifo);
#endif

    fd_vidplane = open(vid_config_fifo, O_RDONLY);
    if (fd_vidplane < 0)
    {
        printf (" Failed to open named pipe %s\n", vid_config_fifo);
        exit(0);
    }

#ifdef DEBUGGPUCOMP
    printf (" Opened the Named Pipe For Video plane Config: %d %s\n", vid_plane_no, vid_config_fifo);
#endif

    while(1)
    {
        n = read(fd_vidplane, &vidCfgRecvd, sizeof(vidCfgRecvd));

        if (n == 0)
        {
            vidCfg[vid_plane_no].enable = 0;
            break;
        }

      if (vidCfgRecvd.config_data) {
        xpos   = vidCfgRecvd.out.xpos;
        ypos   = vidCfgRecvd.out.ypos;
        width  = vidCfgRecvd.out.width;
        height = vidCfgRecvd.out.height;

        vidCfg[vid_plane_no] = vidCfgRecvd;

        rect_vertices_vid [vid_plane_no][0][0] = xpos;
        rect_vertices_vid [vid_plane_no][0][1] = ypos;

        rect_vertices_vid [vid_plane_no][1][0] = xpos;
        rect_vertices_vid [vid_plane_no][1][1] = ypos - height;

        rect_vertices_vid [vid_plane_no][2][0] = xpos + width;
        rect_vertices_vid [vid_plane_no][2][1] = ypos;

        rect_vertices_vid [vid_plane_no][3][0] = xpos + width;
        rect_vertices_vid [vid_plane_no][3][1] = ypos;

        rect_vertices_vid [vid_plane_no][4][0] = xpos;
        rect_vertices_vid [vid_plane_no][4][1] = ypos - height;

        rect_vertices_vid [vid_plane_no][5][0] = xpos + width;
        rect_vertices_vid [vid_plane_no][5][1] = ypos - height;

        vid_plane_mdfd[vid_plane_no] = 1;
     } else {
        vid_data_idx[vid_plane_no] = vidCfgRecvd.buf_index;

     }
    }
  }
}

static int setup_shaders( )
{
    int status;

    /* Initialize shaders */
    ver_shader     = glCreateShader(GL_VERTEX_SHADER);
    frag_shader    = glCreateShader(GL_FRAGMENT_SHADER);

    /* Attach and compile shaders */
    /* Vertex Shader */
    glShaderSource(ver_shader, 1, (const char **) &vshader_src, NULL);
    glCompileShader(ver_shader);
    glGetShaderiv(ver_shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(ver_shader, sizeof (buf), NULL, buf);
        printf("ERROR: Vertex shader compilation failed, info log:\n%s", buf);
        return -1;
    }

    glShaderSource(frag_shader, 1, (const char **) &fshader_src_palpha_single_texture, NULL);

    glCompileShader(frag_shader);
    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(frag_shader, sizeof (buf), NULL, buf);
        printf("ERROR: Fragment shader compilation failed, info log:\n%s", buf);
        return -1;
    }

    program = glCreateProgram();

    /* Attach shader to the program */
    glAttachShader(program, ver_shader);
    glAttachShader(program, frag_shader);

    // Bind vPosition to attribute 0
    glBindAttribLocation(program, 0, "vPosition");
    glBindAttribLocation(program, 1, "inTexCoord");

    /* link the program */
    glLinkProgram(program);
    return 0;
}

GLuint tex_obj_gfx[MAX_GFX_PLANES];
GLuint tex_obj_gfx_created[MAX_GFX_PLANES] = {0, 0, 0, 0};

/* GFX plane update - recreates the texture based on the change in input parameters */
void recreate_gfx_texture (int * bc_id_p, int gfx_plane_no) 
{
    int bc_id;

    bc_id = *bc_id_p;

    /* check whether the texture device is opened earlier or not */
    if (bc_id < 0)
    {
        /* open a device and initialize with texture parameters */
        bc_id = init_bcdev (gfxCfg[gfx_plane_no].in_g.pixel_format,gfxCfg[gfx_plane_no].in_g.width, gfxCfg[gfx_plane_no].in_g.height, 1);
        if ( bc_id < 0) exit (0);

    } else 
    {
        /* close and re-open the device with the new texture parameters */
        bc_id = reinit_bcdev (gfxCfg[gfx_plane_no].in_g.pixel_format,gfxCfg[gfx_plane_no].in_g.width, gfxCfg[gfx_plane_no].in_g.height, 1, bc_id);

    }
    *bc_id_p = bc_id;   /* store the device id */

    
#ifdef DEBUGGPUCOMP
    printf (" bc_id: %d  gfx_plane_no: %d  data_ph_addr: %lx \n", bc_id, gfx_plane_no, gfxCfg[gfx_plane_no].in_g.data_ph_addr);

#endif
    /* set the gfx plane buffer address as the texture address */
    if ( modify_bufAddr (bc_id, 0, gfxCfg[gfx_plane_no].in_g.data_ph_addr) < 0)
    {
        exit(0);
    }
    if (tex_obj_gfx_created[gfx_plane_no]) 
    { 
       glDeleteTextures(1, &tex_obj_gfx[gfx_plane_no]);
    }
    tex_obj_gfx_created[gfx_plane_no] = 1;

    /* Set the texture attributes */
    glGenTextures(1, &tex_obj_gfx[gfx_plane_no]);
    glBindTexture(GL_TEXTURE_STREAM_IMG, tex_obj_gfx[gfx_plane_no]);
    glTexParameterf(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//    glTexBindStreamIMG (bc_id, 0);
}


/* Video texture object creation */
GLuint tex_obj_vid_created[MAX_VID_PLANES] = {0,0,0,0}; 

GLuint tex_obj_vid[MAX_VID_PLANES];

void recreate_vid_texture (int * bc_id_p, int vid_plane_no)
{
    int bc_id, i;

    bc_id = *bc_id_p;

#ifdef DEBUGGPUCOMP
    printf (" bc_id: %d  vid_plane_no: %d  recreating the video textures \n", bc_id, vid_plane_no);
#endif

    if (bc_id < 0)
    {
        bc_id = init_bcdev (vidCfg[vid_plane_no].in.fourcc, vidCfg[vid_plane_no].in.width, vidCfg[vid_plane_no].in.height, vidCfg[vid_plane_no].in.count);
        if ( bc_id < 0) exit (0);

    } else
    {
        bc_id = reinit_bcdev (vidCfg[vid_plane_no].in.fourcc, vidCfg[vid_plane_no].in.width, vidCfg[vid_plane_no].in.height, vidCfg[vid_plane_no].in.count, bc_id);

    }
    *bc_id_p = bc_id;

    for (i = 0; i < vidCfg[vid_plane_no].in.count; i++) 
    {
        if ( modify_bufAddr (bc_id, i, vidCfg[vid_plane_no].in.phyaddr[i]) < 0)
        {
            exit(0);
        }
     
    }
    if (tex_obj_vid_created[vid_plane_no]) 
    {
        glDeleteTextures(1, &tex_obj_vid[vid_plane_no]);

    }
    glGenTextures (1, &tex_obj_vid[vid_plane_no]);
    tex_obj_vid_created[vid_plane_no] = 1;

    glBindTexture(GL_TEXTURE_STREAM_IMG, tex_obj_vid[vid_plane_no]);
    glTexParameterf(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

int main(int argc, char *argv[])
{
    int   bcdevid_vid[MAX_VID_PLANES] = { -1, -1, -1, -1 };
    int   bcdevid_gfx[MAX_GFX_PLANES] = { -1, -1, -1, -1 };
    int   gfx_planes[MAX_GFX_PLANES], vidCfgPlanes[MAX_VID_PLANES];
    
    /* Variables for profiling */
    int i, c, idx;
    struct timeval tvp, tv;
    unsigned long tdiff = 0;
    int fcount = 0;
    int   profiling   = 0;

#ifdef FILE_RAW_VIDEO
    int file_video = 0;
    int file_buf_idx = 0;
    int bcdevid_file_vid;
    GLuint tex_obj_file_vid; 
    int   iwidth      = 720;
    int   iheight     = 480;
    char  infile[200] = "akiyo_d1_422.yuv";
    void          *vidStreamBufVa;
    unsigned long vidStreamBufPa;
    char *yuv_ptr;
    FILE *fin;
    unsigned long TextureBufsPa[256];
    float video_x = -0.5, video_y = 0.5, video_width = 1.0, video_height = 1.0;
#endif
int matrixLocation;

    char opts[] = "f:i:a:b:p:l:m:n:o:h";

    signal(SIGINT, signalHandler);

    for (;;) {
        c = getopt_long(argc, argv, opts, (void *)NULL, &idx);
        if (-1 == c)
            break;
        switch (c) {

#ifdef FILE_RAW_VIDEO
            case 'f':
                file_video = atoi(optarg);
                break;
            case 'i':
                strcpy(infile,optarg);
                break;
            case 'a':
                iwidth = atoi(optarg);
                break;
            case 'b':
                iheight = atoi(optarg);
                break;
           case 'l':
                video_x = atof(optarg);
                break;
           case 'm':
                video_y = atof(optarg);
                break;
           case 'n':
                video_width = atof(optarg);
                break;
           case 'o':
                video_height = atof(optarg);
                break;
#endif
           case 'p':
                profiling = atoi(optarg);
                break;
     
           default:
                usage(argv[0]);
                return 0;
        }
    }
    printf (" LED Wall Application: Blending Graphics w/ video \n");

    for (i = 0; i < MAX_GFX_PLANES; i++) 
    {
        matrixRotateZ(0, matgfx[i]);

    }

    for (i = 0; i < MAX_VID_PLANES; i++)
    {
        matrixRotateZ(0, matvid[i]);

    }



#ifdef FILE_RAW_VIDEO
    if (file_video) {
        rect_vertices_file_vid[0][0] = video_x;
        rect_vertices_file_vid[0][1] = video_y;

        rect_vertices_file_vid[1][0] = video_x;
        rect_vertices_file_vid[1][1] = video_y - video_height;

        rect_vertices_file_vid[2][0] = video_x + video_width;
        rect_vertices_file_vid[2][1] = video_y;

        rect_vertices_file_vid[3][0] = video_x + video_width;
        rect_vertices_file_vid[3][1] = video_y;

        rect_vertices_file_vid[4][0] = video_x;
        rect_vertices_file_vid[4][1] = video_y - video_height;

        rect_vertices_file_vid[5][0] = video_x + video_width;
        rect_vertices_file_vid[5][1] = video_y - video_height;

        printf (" input file video frame width: %d\n", iwidth);
        printf (" input file video frame height: %d\n", iheight);
        printf (" number of input frames/texture buffers : %d\n", MAX_TEX_BUFS);
        printf (" profiling Enable/Disbale %d\n", profiling);
        printf (" raw YUV422 input file : %s\n", infile);

        printf (" video_x: %f\n", video_x);
        printf (" video_y: %f\n", video_y);
        printf (" video_width: %f\n", video_width);
        printf (" video_height: %f\n", video_height);

        CMEM_init();

        vidStreamBufVa = CMEM_alloc((iwidth*iheight*2*MAX_TEX_BUFS), &params);
        if (!vidStreamBufVa)
        {
            printf ("CMEM_alloc for Video Stream buffer returned NULL \n");
            exit (1);
        }

        vidStreamBufPa = CMEM_getPhys(vidStreamBufVa);
        for (i = 0; i < MAX_TEX_BUFS; i++)
        {
            TextureBufsPa[i] = vidStreamBufPa + (iwidth*iheight*2*i);
        }  
        if ((fin = fopen(infile,"rb")) == NULL)
        {
            printf ("Unable to open input file:  %s \n", infile);
            exit(1);
        }
        yuv_ptr = (char *)vidStreamBufVa;
        printf (" Loading YUV422 data from file ......");fflush(stdout);
        fread( yuv_ptr ,1, (iwidth*iheight*2*MAX_TEX_BUFS), fin);
        printf (" Finished ......\n");
        fclose (fin);
    }
#endif

    memset(gfxCfg, 0, sizeof(gfxCfg_s)*MAX_GFX_PLANES);
    memset(vidCfg, 0, sizeof(videoConfig_s)*MAX_VID_PLANES);

    /* Threads for video config Planes */
    for (i=0; i < MAX_VID_PLANES; i++)
    {
        vidCfgPlanes[i] = i;
        pthread_create(&vidCfgtid[i], NULL, vidConfigDataThread, (void *) &vidCfgPlanes[i]);
#ifdef DEBUGGPUCOMP
        printf (" Created Thread for Video plane %d\n", i);
#endif
    }

    /* Threads for Graphics Planes */
    for (i=0; i < MAX_GFX_PLANES; i++)
    {
        gfx_planes[i] = i;
        pthread_create(&gfxtid[i], NULL, gfxThread, (void *) &gfx_planes[i]);
#ifdef DEBUGGPUCOMP
        printf (" Created Thread for GFX plane %d\n", i);
#endif
    }

    if (initEGL(NULL, NULL, profiling)) {
        printf("ERROR: init EGL failed\n");
        exit (0);
    }
    if (setup_shaders () < 0)
    {
      printf (" ERROR: setup shader faileed \n");
      exit (0);
    };

    matrixLocation = glGetUniformLocation(program, "matrix");

    glActiveTexture(GL_TEXTURE0);
   
#ifdef FILE_RAW_VIDEO
    if (file_video) 
    {
        glGenTextures (1, &tex_obj_file_vid);
        bcdevid_file_vid = init_bcdev (BC_PIX_FMT_UYVY, iwidth, iheight, MAX_TEX_BUFS);
        for (i = 0; i < MAX_TEX_BUFS; i++)
        {
            if ( modify_bufAddr (bcdevid_file_vid, i, TextureBufsPa[i]) < 0)
            {
                exit(0);
            }
        }

        glBindTexture(GL_TEXTURE_STREAM_IMG, tex_obj_file_vid);
        glTexParameterf(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
#endif

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glUniform1i(glGetUniformLocation(program, "sTexture"), 0);
    glUseProgram(program);

    gettimeofday(&tvp, NULL);
    while (!gQuit) {
        glClear(GL_COLOR_BUFFER_BIT);

#ifdef FILE_RAW_VIDEO
        /* ------------------------------------------------------------------*/
        /* File Video Texturing                                              */
        /* ------------------------------------------------------------------*/
        if (file_video ) 
        {
            glBindTexture(GL_TEXTURE_STREAM_IMG, tex_obj_file_vid);
            glTexBindStreamIMG (bcdevid_file_vid, file_buf_idx);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, rect_vertices_file_vid);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, rect_texcoord);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glDisableVertexAttribArray (0);
            glDisableVertexAttribArray (1);

            file_buf_idx++;
            if (file_buf_idx >= (MAX_TEX_BUFS-3)) file_buf_idx = 0;
        }
#endif

        /* ------------------------------------------------------------------*/
        /* Video Texturing                                                */
        /* ------------------------------------------------------------------*/
        for (i=0; i < MAX_GFX_PLANES; i++)
        {
            if (vidCfg[i].enable)
            {
                if (vid_plane_mdfd[i] > 0)
                {
#ifdef DEBUGGPUCOMP
                    printf (" Vid plane %d Updated \n", i);
#endif
                    recreate_vid_texture (&bcdevid_vid[i], i);
                    matrixRotateZ(vidCfg[i].in.rotate, matvid[i]);
                    printf (" vidCfg[i].in.rotate: %f\n", vidCfg[i].in.rotate);
                    vid_plane_mdfd[i] = 0;

                }
                glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, matvid[i]);

                glBindTexture(GL_TEXTURE_STREAM_IMG, tex_obj_vid[i]);
                glTexBindStreamIMG(bcdevid_vid[i], vid_data_idx[i]);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,
                                      rect_vertices_vid[i]);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0,
                                      rect_texcoord);
                glEnableVertexAttribArray(0);
                glEnableVertexAttribArray(1);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glDisableVertexAttribArray (0);
                glDisableVertexAttribArray (1);
            }
        }
        /*-------------------------------------------------------------------*/


        /* ------------------------------------------------------------------*/
        /* Graphics Texturing                                                */
        /* ------------------------------------------------------------------*/
        for (i=0; i < MAX_GFX_PLANES; i++)
        {
            if (gfxCfg[i].enable)
            {
                /* Update the gfx plane if modified */
                if (gfx_plane_mdfd[i] > 0) 
                {
#ifdef DEBUGGPUCOMP
                    printf (" GFX plane %d Updated \n", i);
#endif
                    recreate_gfx_texture (&bcdevid_gfx[i], i);
                    matrixRotateZ(gfxCfg[i].in_g.rotate, matgfx[i]);
                    printf (" rotate = %f\n", gfxCfg[i].in_g.rotate);
                    gfx_plane_mdfd[i] = 0;
                }
                glUniformMatrix4fv( matrixLocation, 1, GL_FALSE, matgfx[i]);

                glBindTexture(GL_TEXTURE_STREAM_IMG, tex_obj_gfx[i]);
                glTexBindStreamIMG (bcdevid_gfx[i], 0);

                /* Configure pixel/global blending if enabled */
                if (gfxCfg[i].in_g.enable_blending)
                {
                    glEnable (GL_BLEND);
                    if (gfxCfg[i].in_g.enable_global_alpha)
                    {
                        glBlendColor (0.0, 0.0, 0.0, gfxCfg[i].in_g.global_alpha);
                        glBlendFunc (GL_CONSTANT_ALPHA, 
                                     GL_ONE_MINUS_CONSTANT_ALPHA);
                    } else 
                    {
                        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    }
                }

                /* Draw the plane */
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 
                                      rect_vertices_gfx[i]);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 
                                      rect_texcoord);
                glEnableVertexAttribArray(0);
                glEnableVertexAttribArray(1);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glDisableVertexAttribArray (0);
                glDisableVertexAttribArray (1);
                if (gfxCfg[i].in_g.enable_blending)
                {
                    glDisable (GL_BLEND);
                }
            } 
        }
        /*-------------------------------------------------------------------*/

        eglSwapBuffers(dpy, surface);

        if (profiling == 0)
            continue;
        gettimeofday(&tv, NULL);
        fcount++;
        if (fcount == 1000) {
            tdiff = (unsigned long)(tv.tv_sec*1000 + tv.tv_usec/1000 -
                                tvp.tv_sec*1000 - tvp.tv_usec/1000);
            printf("Frame Rate: %ld \n", (1000*1000)/tdiff);
            fcount = 0;
            gettimeofday(&tvp, NULL);
        }
    }
    printf ("\n");

    deInitEGL();
    /* clean up shaders */
    glDeleteProgram(program);
    glDeleteShader(ver_shader);
    glDeleteShader(frag_shader);
//    deinit_bcdev (bcdevid);
    printf(" DONE \n");
    return 0;
}
