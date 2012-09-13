/*****************************************************************************
 * common.c
 *
 *    common functions - egl initialize
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
 ****************************************************************************/
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/fb.h>
#include "common.h"

int gQuit = 0;

EGLDisplay dpy;
EGLSurface surface = EGL_NO_SURFACE;
static EGLContext context = EGL_NO_CONTEXT;

void signalHandler(int signum) { (void)signum; gQuit=1; }

int get_disp_resolution(int *w, int *h)
{
    int fb_fd, ret = -1;
    struct fb_var_screeninfo vinfo;

    if ((fb_fd = open("/dev/fb0", O_RDONLY)) < 0) {
        printf("failed to open fb0 device\n");
        return ret;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("FBIOGET_VSCREENINFO");
        goto exit;
    }

    *w = vinfo.xres;
    *h = vinfo.yres;

    if (*w && *h)
        ret = 0;

exit:
    close(fb_fd);
    return ret;
}

static void print_err(char *name)
{
    char *err_str[] = {
          "EGL_SUCCESS",
          "EGL_NOT_INITIALIZED",
          "EGL_BAD_ACCESS",
          "EGL_BAD_ALLOC",
          "EGL_BAD_ATTRIBUTE",    
          "EGL_BAD_CONFIG",
          "EGL_BAD_CONTEXT",   
          "EGL_BAD_CURRENT_SURFACE",
          "EGL_BAD_DISPLAY",
          "EGL_BAD_MATCH",
          "EGL_BAD_NATIVE_PIXMAP",
          "EGL_BAD_NATIVE_WINDOW",
          "EGL_BAD_PARAMETER",
          "EGL_BAD_SURFACE" };

    EGLint ecode = eglGetError();

    printf("'%s': egl error '%s' (0x%x)\n",
           name, err_str[ecode-EGL_SUCCESS], ecode);
}

void deInitEGL()
{

    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (context != EGL_NO_CONTEXT)
        eglDestroyContext(dpy, context);
    if (surface != EGL_NO_SURFACE)
        eglDestroySurface(dpy, surface);
    eglTerminate(dpy);
}

int initEGL(int *surf_w, int *surf_h, int profile)
{

    EGLint  context_attr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

//    typedef NativeDisplayType EGLNativeDisplayType;
//    typedef NativeWindowType EGLNativeWindowType;

    EGLint            disp_w, disp_h;
    EGLNativeDisplayType disp_type;
    EGLNativeWindowType  window;
    EGLConfig         cfgs[2];
    EGLint            n_cfgs;
    EGLint            egl_attr[] = {
                         EGL_BUFFER_SIZE, EGL_DONT_CARE,
#if 0
                         EGL_RED_SIZE,    8,
                         EGL_GREEN_SIZE,  8,
                         EGL_BLUE_SIZE,   8,
                         EGL_DEPTH_SIZE,  8,
#endif
                         EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
  
                         EGL_NONE };

    if (get_disp_resolution(&disp_w, &disp_h)) {
        printf("ERROR: get display resolution failed\n");
        return -1;
    }

    disp_type = (EGLNativeDisplayType)EGL_DEFAULT_DISPLAY;
    window  = 0;

    dpy = eglGetDisplay(disp_type);

    if (eglInitialize(dpy, NULL, NULL) != EGL_TRUE) {
        print_err("eglInitialize");
        return -1;
    }

    if (eglGetConfigs(dpy, cfgs, 2, &n_cfgs) != EGL_TRUE) {
        print_err("eglGetConfigs");
        goto cleanup;
    }
    
    if (eglChooseConfig(dpy, egl_attr, cfgs, 2, &n_cfgs) != EGL_TRUE) {
        print_err("eglChooseConfig");
        goto cleanup;
    }

    surface = eglCreateWindowSurface(dpy, cfgs[0], window, NULL);
    if (surface == EGL_NO_SURFACE) {
        print_err("eglCreateWindowSurface");
        goto cleanup;
    }

    if (surf_w && surf_h) {
        *surf_w = disp_w;
        *surf_h = disp_h;
    }

    context = eglCreateContext(dpy, cfgs[0], EGL_NO_CONTEXT, context_attr);
    
    if (context == EGL_NO_CONTEXT) {
        print_err("eglCreateContext");
        goto cleanup;
    }

    if (eglMakeCurrent(dpy, surface, surface, context) != EGL_TRUE) {
        print_err("eglMakeCurrent");
        goto cleanup;
    }

    /* do not sync with video frame if profile enabled */
    if (profile == 1) {
        if (eglSwapInterval(dpy, 0) != EGL_TRUE) {
            print_err("eglSwapInterval");
            goto cleanup;
        }
    }
    return 0;

cleanup:
    deInitEGL();
    return -1;
}

static int bcfd[4] = {-1, -1, -1, -1};
static int bcdev_id = -1;
PFNGLTEXBINDSTREAMIMGPROC glTexBindStreamIMG = NULL;
#define MAX_BCCAT_DEVICES 8 

int init_bcdev (unsigned int pix_frmt, int width, int height, int num_bufs)
{
    char bcdev_name[] = "/dev/bccatX";
    BCIO_package ioctl_var;
    bc_buf_params_t buf_param;

    buf_param.width  = width;
    buf_param.height = height;
    buf_param.count  = num_bufs;
    buf_param.fourcc = pix_frmt;
    buf_param.type   = BC_MEMORY_USERPTR;

    bcdev_id += 1;
    if (bcdev_id > (MAX_BCCAT_DEVICES -1)) 
    {
        printf (" Exceeded the bumber of bccat devices\n");
        return -1;
    }
    bcdev_name[strlen(bcdev_name)-1] = '0' + bcdev_id;
    if ((bcfd[bcdev_id] = open(bcdev_name, O_RDWR|O_NDELAY)) == -1) {
        printf("ERROR: open %s failed\n", bcdev_name);
        return -1;
    }
    if (ioctl(bcfd[bcdev_id], BCIOREQ_BUFFERS, &buf_param) != 0) {
        printf("ERROR: BCIOREQ_BUFFERS failed\n");
        return -1;
    }
    if (ioctl(bcfd[bcdev_id], BCIOGET_BUFFERCOUNT, &ioctl_var) != 0) {
        return -1;
    }
    if (ioctl_var.output == 0) {
        printf("ERROR: no texture buffer available\n");
        return -1;
    }
    glTexBindStreamIMG =
        (PFNGLTEXBINDSTREAMIMGPROC)eglGetProcAddress("glTexBindStreamIMG");
    return (bcdev_id);
}

int modify_bufAddr (int bcdevId, int idx, unsigned long buf_paddr)
{
    bc_buf_ptr_t buf_pa;
    buf_pa.pa    = buf_paddr;
    buf_pa.index = idx;
    if (ioctl(bcfd[bcdevId], BCIOSET_BUFFERPHYADDR, &buf_pa) != 0) {
        printf("ERROR: BCIOSET_BUFFERADDR[%d]: failed (0x%lx)\n",
                buf_pa.index, buf_pa.pa);
        return -1;
    }
    return 0;
}
void deinit_bcdev (int bcdevId )
{
    close (bcfd[bcdevId]);
}

int reinit_bcdev (unsigned int pix_frmt, int width, int height, int num_bufs, int bcdevId)
{
    char bcdev_name[] = "/dev/bccatX";
    BCIO_package ioctl_var;
    bc_buf_params_t buf_param;

    deinit_bcdev (bcdevId);

    buf_param.width  = width;
    buf_param.height = height;
    buf_param.count  = num_bufs;
    buf_param.fourcc = pix_frmt;
    buf_param.type   = BC_MEMORY_USERPTR;

    bcdev_name[strlen(bcdev_name)-1] = '0' + bcdevId;
    if ((bcfd[bcdevId] = open(bcdev_name, O_RDWR|O_NDELAY)) == -1) {
        printf("ERROR: reinit_bcdev: %s failed\n", bcdev_name);
        return -1;
    }
    if (ioctl(bcfd[bcdevId], BCIOREQ_BUFFERS, &buf_param) != 0) {
        printf("ERROR: reinit_bcdev: BCIOREQ_BUFFERS failed\n");
        return -1;
    }
    if (ioctl(bcfd[bcdevId], BCIOGET_BUFFERCOUNT, &ioctl_var) != 0) {
        return -1;
    }
    if (ioctl_var.output == 0) {
        printf("ERROR: reinit_bcdev: no texture buffer available\n");
        return -1;
    }
    glTexBindStreamIMG =
        (PFNGLTEXBINDSTREAMIMGPROC)eglGetProcAddress("glTexBindStreamIMG");

    return (bcdevId);
}

