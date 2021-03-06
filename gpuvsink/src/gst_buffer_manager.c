/******************************************************************************
*****************************************************************************
 * gstbuffer_manager.c 
 * Responsible for gst buffer pool management -  Adopted base implementation from
 * gst-plugin-bc project -  http://gitorious.org/gst-plugin-bc/gst-plugin-bc 
 * &
 * https://github.com/aditya-nellutla/TI-Graphics-Accelerated-Video-streaming-
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
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
 * Contact: mmurthy@ti.com
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/* despite what config.h thinks, don't use 64bit mmap...
 */
#ifdef _FILE_OFFSET_BITS
#  undef _FILE_OFFSET_BITS
#endif

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <fcntl.h>
#include <unistd.h>

#include "gst_render_bridge.h"

#include <stdio.h>
#include <string.h>
#include <cmem.h>
#include "../../gpucomp.h"


GST_DEBUG_CATEGORY_EXTERN (gpuvsink_debug);
#define GST_CAT_DEFAULT gpuvsink_debug

static CMEM_AllocParams cmem_params = { CMEM_POOL, CMEM_CACHED, 4096 };

/* round X up to a multiple of Y:
 */
#define CEIL(X,Y)  ((Y) * ( ((X)/(Y)) + (((X)%(Y)==0)?0:1) ))

/*
 * GstBufferClassBuffer:
 */
static GstBufferClass *buffer_parent_class = NULL;



static void
gst_bcbuffer_finalize (GstBufferClassBuffer * buffer)
{
  GstBufferClassBufferPool *pool = buffer->pool;
  gboolean resuscitated;

  GST_LOG_OBJECT (pool->elem, "finalizing buffer %p %d", buffer, buffer->index);


  GST_BCBUFFERPOOL_LOCK (pool);
  if (pool->running) {
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_READONLY);
    g_async_queue_push (pool->avail_buffers, buffer);
    resuscitated = TRUE;
  } else {
    GST_LOG_OBJECT (pool->elem, "the pool is shutting down");
    resuscitated = FALSE;
  }

  if (resuscitated) {
    GST_LOG_OBJECT (pool->elem, "reviving buffer %p, %d", buffer,
        buffer->index);
    gst_buffer_ref (GST_BUFFER (buffer));
  }

  GST_BCBUFFERPOOL_UNLOCK (pool);

  if (!resuscitated) {
    GST_LOG_OBJECT (pool->elem, "buffer %p not recovered, unmapping", buffer);
    gst_mini_object_unref (GST_MINI_OBJECT (pool));
//    munmap ((void *) GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

    GST_MINI_OBJECT_CLASS (buffer_parent_class)->
        finalize (GST_MINI_OBJECT (buffer));
  }
}


static void
gst_bcbuffer_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  buffer_parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      gst_bcbuffer_finalize;
}


GType
gst_bcbuffer_get_type (void)
{
  static GType _gst_bcbuffer_type;

  if (G_UNLIKELY (_gst_bcbuffer_type == 0)) {
    static const GTypeInfo bcbuffer_info = {
      sizeof (GstBufferClassBufferClass),
      NULL,
      NULL,
      gst_bcbuffer_class_init,
      NULL,
      NULL,
      sizeof (GstBufferClassBuffer),
      0,
      NULL,
      NULL
    };
    _gst_bcbuffer_type = g_type_register_static (GST_TYPE_BUFFER,
        "GstBufferClassBuffer", &bcbuffer_info, 0);
  }
  return _gst_bcbuffer_type;
}


static GstBufferClassBuffer *
gst_bcbuffer_new (GstBufferClassBufferPool * pool, int idx, int sz, unsigned long buf_paddr)
{
  GstBufferClassBuffer *ret = NULL;
  ret = (GstBufferClassBuffer *) gst_mini_object_new (GST_TYPE_BCBUFFER);

  if(ret == NULL)
	goto fail;

  ret->pool = GST_BCBUFFERPOOL (gst_mini_object_ref (GST_MINI_OBJECT (pool)));
  ret->index = idx;

  GST_LOG_OBJECT (pool->elem, "creating buffer %u (sz=%d), %p in pool %p", idx,
      sz, ret, pool);
  GST_BUFFER_SIZE (ret) = sz;
  return ret;

fail:
  gst_mini_object_unref (GST_MINI_OBJECT (ret));
  return NULL;
}


/*
 * GstBufferClassBufferPool:
 */
static GstMiniObjectClass *buffer_pool_parent_class = NULL;


static void
gst_buffer_manager_finalize (GstBufferClassBufferPool * pool)
{
  g_mutex_free (pool->lock);
  pool->lock = NULL;

  if (pool->avail_buffers) {
    g_async_queue_unref (pool->avail_buffers);
    pool->avail_buffers = NULL;
  }

  if (pool->buffers) {
    g_free (pool->buffers);
    pool->buffers = NULL;
  }

  gst_caps_unref (pool->caps);
  pool->caps = NULL;

  GST_MINI_OBJECT_CLASS (buffer_pool_parent_class)->finalize (GST_MINI_OBJECT
      (pool));
}


static void
gst_buffer_manager_init (GstBufferClassBufferPool * pool, gpointer g_class)
{
  pool->lock = g_mutex_new ();
  pool->running = FALSE;
}


static void
gst_buffer_manager_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  buffer_pool_parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      gst_buffer_manager_finalize;
}


GType
gst_buffer_manager_get_type (void)
{
  static GType _gst_buffer_manager_type;

  if (G_UNLIKELY (_gst_buffer_manager_type == 0)) {
    static const GTypeInfo buffer_manager_info = {
      sizeof (GstBufferClassBufferPoolClass),
      NULL,
      NULL,
      gst_buffer_manager_class_init,
      NULL,
      NULL,
      sizeof (GstBufferClassBufferPool),
      0,
      (GInstanceInitFunc) gst_buffer_manager_init,
      NULL
    };
    _gst_buffer_manager_type = g_type_register_static (GST_TYPE_MINI_OBJECT,
        "GstBufferClassBufferPool", &buffer_manager_info, 0);
  }
  return _gst_buffer_manager_type;
}

/*
 * Construct new bufferpool and allocate buffers from driver
 *
 * @elem      the parent element that owns this buffer
 * @fd        the file descriptor of the device file
 * @count     the requested number of buffers in the pool
 * @caps      the requested buffer caps
 * @return the bufferpool or <code>NULL</code> if error
 */
GstBufferClassBufferPool *
gst_buffer_manager_new (GstElement * elem, videoConfig_s videoConfig, int count, GstCaps * caps)
{
  GstBufferClassBufferPool *pool = NULL;
  GstVideoFormat format;
  gint width, height;
  unsigned long vidStreamBufPa;
  void *vidStreamBufVa;
  int n, i;

  GstBufferClassSink *gpuvsink = GST_BCSINK (elem);

  if (gst_video_format_parse_caps(caps, &format, &width, &height)) {

  /* The buffers are allocated from CMEM as the gpu composition requires contiguous memory */
  CMEM_init();

  /* Allocate single block of contiguous memory for all the buffers */
  vidStreamBufVa = CMEM_alloc((width*height*BPP*count), &cmem_params);
  if (!vidStreamBufVa)
  {
    printf ("CMEM_alloc for Video Stream buffer returned NULL \n");
    return NULL; 
  }

  vidStreamBufPa = CMEM_getPhys(vidStreamBufVa);
 
  DEBUG_PRINTF((" Number of texture buffers: %d\n", count)); 
  DEBUG_PRINTF((" Video Frame Width: %d\n", width));
  DEBUG_PRINTF((" Video Frame Height: %d\n", height));

  /* Divide the single block of contiguous memory into the requested number of buffers */
  for (i = 0; i < count; i++)
  {
    videoConfig.in.phyaddr[i] = vidStreamBufPa + (width*height*BPP*i); 
    DEBUG_PRINTF ((" TextureBufAddr %d: %lx\n", i, videoConfig.in.phyaddr[i]));
  }
  videoConfig.enable = 1;
  videoConfig.config_data = 1;
  videoConfig.in.count   = count;
  videoConfig.in.height  = height;
  videoConfig.in.width   = width;
   
  if (videoConfig.in.crop_width  == 0) videoConfig.in.crop_width = width;
  if (videoConfig.in.crop_height == 0) videoConfig.in.crop_height = height;
  if ((videoConfig.in.crop_x+videoConfig.in.crop_width) > width) {
    printf (" Invalid crop_x+crop_w value: %d \n",(videoConfig.in.crop_x+videoConfig.in.crop_width));
    exit (0);
  }
  if ((videoConfig.in.crop_y+videoConfig.in.crop_height) > height) {
    printf (" Invalid crop_y+crop_h value: %d \n",(videoConfig.in.crop_y+videoConfig.in.crop_height));
    exit (0);
  }

  if(format == GST_VIDEO_FORMAT_YUY2) {
    videoConfig.in.fourcc  = GST_MAKE_FOURCC ('Y', 'U', 'Y', 'V');
  }
  else
  videoConfig.in.fourcc  = gst_video_format_to_fourcc (format);

  /* Send the video configuration via named pipe to the composition module */
  gpuvsink->video_config_fifo[strlen(gpuvsink->video_config_fifo)-1] = '0' +  gpuvsink->channel_no;

  DEBUG_PRINTF ((" gst_buffer_manager_new: video buffers allocation successfull for channel_no: %d \n",  gpuvsink->channel_no));
  DEBUG_PRINTF ((" Opening the video config fifo - %s\n", gpuvsink->video_config_fifo));

  gpuvsink->fd_video_cfg = open(gpuvsink->video_config_fifo, O_WRONLY);
  if(gpuvsink->fd_video_cfg < 0)
  {
    printf (" Failed to open fd_video_cfg FIFO - fd: %d\n", gpuvsink->fd_video_cfg);
    exit(0);
  }

  DEBUG_PRINTF ((" writing to the config fifo - %s \n", gpuvsink->video_config_fifo));

  n = write(gpuvsink->fd_video_cfg, &videoConfig, sizeof(videoConfig));

  if(n != sizeof(videoConfig))
  {
    printf("Error in writing to named pipe: %s \n", VIDEO_CONFIG_AND_DATA_FIFO_NAME);
  }

  DEBUG_PRINTF ((" writing to the config fifo - %s is successful\n", gpuvsink->video_config_fifo));

  /* construct bufferpool */
  pool = (GstBufferClassBufferPool *)
        gst_mini_object_new (GST_TYPE_BCBUFFERPOOL);

  //TODO: Remove fd from pool -not required any more.
   pool->fd = -1;
   pool->elem = elem;
   pool->num_buffers = count;
   pool->vidStreamBufVa = vidStreamBufVa;


   GST_DEBUG_OBJECT (pool->elem, "orig caps: %" GST_PTR_FORMAT, caps);
   GST_DEBUG_OBJECT (pool->elem, "requested %d buffers, got %d buffers", count,
        count);
    
    pool->caps = gst_caps_ref(caps);

    /* and allocate buffers:
     */
    pool->buffers = g_new0 (GstBufferClassBuffer *, count);
    pool->avail_buffers = g_async_queue_new_full (
        (GDestroyNotify) gst_mini_object_unref);

    for (i = 0; i < count; i++) {
     // TODO: Find correct size here
	GstBufferClassBuffer *buf = gst_bcbuffer_new (pool, i, width*height*BPP, videoConfig.in.phyaddr[i]);
	GST_BUFFER_DATA (buf) = (vidStreamBufVa +  width*height*BPP*i);
	GST_BUFFER_SIZE (buf) = width*height*BPP;

      if (G_UNLIKELY (!buf)) {
        GST_WARNING_OBJECT (pool->elem, "Buffer %d allocation failed", i);
        goto fail;
      }
      gst_buffer_set_caps (GST_BUFFER (buf), caps);
      pool->buffers[i] = buf;
      g_async_queue_push (pool->avail_buffers, buf);
    }

    return pool;
  } else {
    GST_WARNING_OBJECT (elem, "failed to parse caps: %" GST_PTR_FORMAT, caps);
  }

fail:
  if (pool) {
    gst_mini_object_unref (GST_MINI_OBJECT (pool));
  }
  return NULL;
}

/**
 * Stop and dispose of this pool object.
 */
void
gst_buffer_manager_dispose (GstBufferClassBufferPool * pool)
{
  int n;
  GstBufferClassBuffer *buf;
  GstBufferClassSink *gpuvsink = GST_BCSINK (pool->elem);
  g_return_if_fail (pool);

  pool->running = FALSE;

  while ((buf = g_async_queue_try_pop (pool->avail_buffers)) != NULL) {
    gst_buffer_unref (GST_BUFFER (buf));
  }

  gst_mini_object_unref (GST_MINI_OBJECT (pool));
 
  if (pool->vidStreamBufVa) {
      CMEM_free (pool->vidStreamBufVa, &cmem_params); 
      DEBUG_PRINTF ((" Freeing Video Memory - CMEM allocated \n"));
      pool->vidStreamBufVa = NULL;

      gpuvsink->videoConfig.config_data = 2;
      n = write(gpuvsink->fd_video_cfg, &gpuvsink->videoConfig, sizeof(gpuvsink->videoConfig));

      if(n != sizeof(gpuvsink->videoConfig))
      {
          printf("Error in writing to named pipe: %s \n", VIDEO_CONFIG_AND_DATA_FIFO_NAME);
      }
      DEBUG_PRINTF ((" sending close command to the config fifo - %s is successful\n", gpuvsink->video_config_fifo));

      usleep (50000);
      close(gpuvsink->fd_video_cfg);
      usleep (100000); 
  }

  GST_DEBUG ("end");
}

/**
 * Get the current caps of the pool, they should be unref'd when done
 *
 * @pool   the "this" object
 */
GstCaps *
gst_buffer_manager_get_caps (GstBufferClassBufferPool * pool)
{
  return gst_caps_ref (pool->caps);
}

/**
 * Get an available buffer in the pool
 *
 * @pool   the "this" object
 */
GstBufferClassBuffer *
gst_buffer_manager_get (GstBufferClassBufferPool * pool)
{
  GstBufferClassBuffer *buf = g_async_queue_pop (pool->avail_buffers);

  if (buf) {
    GST_BUFFER_FLAG_UNSET (buf, 0xffffffff);
  }

  pool->running = TRUE;

  return buf;
}

/**
 * cause buffer to be flushed before rendering
 */
void
gst_bcbuffer_flush (GstBufferClassBuffer * buffer)
{
//  GstBufferClassBufferPool *pool = buffer->pool;

// DEBUG_PRINTF ((" gst_bcbuffer_flush not implemented \n"));
  
#if 0
  BCIO_package param;

  param.input = buffer->index;

  if (ioctl (pool->fd, BCIO_FLUSH, &param) < 0) {
    GST_WARNING_OBJECT (pool->elem, "Failed BCIO_FLUSH: %s",
        g_strerror (errno));
  }
#endif
}

