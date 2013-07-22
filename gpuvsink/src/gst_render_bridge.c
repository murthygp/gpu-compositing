/******************************************************************************
*****************************************************************************
 * gst_render_bridge.c
 * base implmentation adopted from 
 *   https://github.com/aditya-nellutla/TI-Graphics-Accelerated-Video-streaming- 
 *
 * Establishes communication with the renderer to display video onto 
 * 3d surface using the IMGBufferClass extension for texture streaming.
 *
 * Adopted base implementation from gst-plugin-bc project
 * http://gitorious.org/gst-plugin-bc/gst-plugin-bc 
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

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gst_render_bridge.h"
#include <stdio.h>
//#include "bc_cat.h"
#include <pthread.h>

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../gpucomp.h"


static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV("{I420, YV12, NV12, UYVY, YUYV, YUY2}"))
    );

GST_DEBUG_CATEGORY (gpuvsink_debug);

pthread_mutex_t ctrlmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t initmutex = PTHREAD_MUTEX_INITIALIZER;

/* Properties */
enum
{
  PROP_0,
  PROP_QUEUE_SIZE,
  PROP_CHANNEL_NO,
  PROP_XPOS,
  PROP_YPOS,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_ROTATE,
  PROP_OVERLAYONGFX
};

/* Signals */
enum
{
  SIG_INIT,
  SIG_RENDER,
  SIG_CLOSE,
  /* add more above */
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GST_BOILERPLATE (GstBufferClassSink, gst_render_bridge, GstVideoSink,
    GST_TYPE_VIDEO_SINK);

static void gst_render_bridge_dispose (GObject * object);
static void gst_render_bridge_finalize (GstBufferClassSink * gpuvsink);

/* GObject methods: */
static void gst_render_bridge_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_render_bridge_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);


/* GstElement methods: */
static GstStateChangeReturn gst_render_bridge_change_state (GstElement * element,
    GstStateChange transition);

/* GstBaseSink methods: */
#if 0
static GstCaps *gst_render_bridge_get_caps (GstBaseSink * bsink);
#endif
static gboolean gst_render_bridge_set_caps (GstBaseSink * bsink, GstCaps * caps);
static GstFlowReturn gst_render_bridge_buffer_alloc (GstBaseSink * bsink,
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf);
static GstFlowReturn gst_render_bridge_show_frame (GstBaseSink * bsink,
    GstBuffer * buf);


static void
gst_render_bridge_base_init (gpointer g_class)
{
  GstBufferClassSinkClass *gstgpuvsink_class = GST_BCSINK_CLASS (g_class);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (gstgpuvsink_class);

  GST_DEBUG_CATEGORY_INIT (gpuvsink_debug, "gpuvsink", 0, "video sink element");

  gst_element_class_set_details_simple (gstelement_class, 
      "GPU Rendering/Composition",
      "Sink/Video",
      "A video sink utilizing gpu for rendering/composition",
      " Mahesh Murthy <mmurthy@ti.com>,");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));
}


static void
gst_render_bridge_class_init (GstBufferClassSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseSinkClass *basesink_class;

  GST_DEBUG ("ENTER");

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  basesink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->dispose = gst_render_bridge_dispose;
  gobject_class->finalize = (GObjectFinalizeFunc) gst_render_bridge_finalize;
  gobject_class->set_property = gst_render_bridge_set_property;
  gobject_class->get_property = gst_render_bridge_get_property;

  element_class->change_state = gst_render_bridge_change_state;

  /**
   * GstBufferClassSink:device
   *
   * Provides the display configuration parameters.
   */
  g_object_class_install_property (gobject_class, PROP_XPOS,
      g_param_spec_float ("x-pos",
          "Display config parameters",
          "Specifies normalized device output x-cordinate for the video"
          "on the display", -1, 1, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_YPOS,
      g_param_spec_float ("y-pos",
          "Display config parameters",
          "Specifies normalized device output y-cordinate for the video"
          "on the display", -1, 1, 1, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_float ("width",
          "Display config parameters",
          "Specifies the normalized device output width for the video"
          "on the display", 0, 2, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_float ("height",
          "Display config parameters",
          "Specifies the normalized output height for the video"
          "on the display", 0, 2, 0, G_PARAM_WRITABLE));


  g_object_class_install_property (gobject_class, PROP_CHANNEL_NO,
      g_param_spec_uint ("channel-no",
          "Video channel number",
          "Specifies the video channel number"
          "on the display", 0, 4, 0, G_PARAM_WRITABLE));

 g_object_class_install_property (gobject_class, PROP_ROTATE,
      g_param_spec_float ("rotate",
          "Display config parameters",
          "Specifies the rotation in degrees"
          "on the display", -180, 180, 0, G_PARAM_WRITABLE));

g_object_class_install_property (gobject_class, PROP_OVERLAYONGFX,
      g_param_spec_uint ("overlayongfx",
          "Overlay video channel on top of gfx",
          "Specifies the video channel priority over gfx 0 - gfx over video  1 - video over gfx  "
          "on the display", 0, 1, 0, G_PARAM_WRITABLE));


  /**
   * GstBufferClassSink:queue-size
   *
   * Number of buffers to be enqueued in the driver in streaming mode
   */
  g_object_class_install_property (gobject_class, PROP_QUEUE_SIZE,
      g_param_spec_uint ("queue-size", "Queue size",
          "Number of buffers to be enqueued in the driver in streaming mode",
          GST_BC_MIN_BUFFERS, GST_BC_MAX_BUFFERS, PROP_DEF_QUEUE_SIZE,
          G_PARAM_READWRITE));
  
  /**
   * GstBufferClassSink::init:
   * @gpuvsink: the #GstBufferClassSink
   * @buffercount:  the number of buffers used
   *
   * Will be emitted after buffers are allocated, to give the application
   * an opportunity to bind the surfaces
   */
  signals[SIG_INIT] =
      g_signal_new ("init", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
      NULL, NULL, gst_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  /**
   * GstBufferClassSink::render:
   * @gpuvsink: the #GstBufferClassSink
   * @bufferindex:  the index of the buffer to render
   *
   * Will be emitted when a new buffer is ready to be rendered
   */
  signals[SIG_RENDER] =
      g_signal_new ("render", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
      NULL, NULL, gst_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  /**
   * GstBufferClassSink::close:
   * @gpuvsink: the #GstBufferClassSink
   *
   * Will be emitted when the device is closed
   */
  signals[SIG_CLOSE] =
      g_signal_new ("close", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
      NULL, NULL, gst_marshal_VOID__VOID, G_TYPE_NONE, 0);

#if 0
  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_render_bridge_get_caps);
#endif
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_render_bridge_set_caps);
  basesink_class->buffer_alloc = GST_DEBUG_FUNCPTR (gst_render_bridge_buffer_alloc);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_render_bridge_show_frame);
}

static void
gst_render_bridge_init (GstBufferClassSink * gpuvsink, GstBufferClassSinkClass * klass)
{

  GST_DEBUG_OBJECT (gpuvsink, "ENTER");

  /* default property values: */
  gpuvsink->num_buffers = PROP_DEF_QUEUE_SIZE;

  /*Initialize config structure with default values*/
  gpuvsink->videoConfig.out.xpos   = VID_GPUVSINK_XPOS;
  gpuvsink->videoConfig.out.ypos   = VID_GPUVSINK_YPOS;
  gpuvsink->videoConfig.out.width  = VID_GPUVSINK_WIDTH;
  gpuvsink->videoConfig.out.height = VID_GPUVSINK_HEIGHT;
  gpuvsink->channel_no = VID_GPUVSINK_CHANNEL_NO;
  gpuvsink->videoConfig.overlayongfx = VID_OVERLAYONGFX;
  gpuvsink->videoConfig.in.rotate = VID_GPUVSINK_ROTATE;
  strcpy(gpuvsink->video_config_fifo,VIDEO_CONFIG_AND_DATA_FIFO_NAME);
  gpuvsink->bcbuf_prev1 = NULL;
  gpuvsink->bcbuf_prev2 = NULL;
  gpuvsink->bcbuf_prev3 = NULL;
  gpuvsink->bcbuf_prev4 = NULL;
  gpuvsink->bcbuf_prev5 = NULL;  
}

static void
gst_render_bridge_dispose (GObject * object)
{
  GST_DEBUG_OBJECT (object, "ENTER");
  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
gst_render_bridge_finalize (GstBufferClassSink * gpuvsink)
{
  GST_DEBUG_OBJECT (gpuvsink, "ENTER");
  if (G_LIKELY (gpuvsink->pool)) {
    gst_mini_object_unref (GST_MINI_OBJECT (gpuvsink->pool));
    gpuvsink->pool = NULL;
  }
  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (gpuvsink));
}

static void
gst_render_bridge_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstBufferClassSink *gpuvsink = GST_BCSINK (object);
  switch (prop_id) {
    case PROP_QUEUE_SIZE:
      gpuvsink->num_buffers = g_value_get_uint (value);
      break;

    case PROP_CHANNEL_NO:
         gpuvsink->channel_no = g_value_get_uint (value);
         if (gpuvsink->channel_no < 0 || gpuvsink->channel_no > (MAX_GFX_PLANES-1)) {
             printf (" Invalid Channel Number: %d   <Valid Range: 0 to MAX_GFX_PLANES> \n", gpuvsink->channel_no);
             exit (0);
         }
         break;

    case PROP_XPOS:
	gpuvsink->videoConfig.out.xpos = g_value_get_float (value);
	break;

    case  PROP_OVERLAYONGFX:
        gpuvsink->videoConfig.overlayongfx = g_value_get_uint (value);
        break;

    case PROP_YPOS:
	gpuvsink->videoConfig.out.ypos = g_value_get_float (value);
	break;

    case PROP_WIDTH:
	gpuvsink->videoConfig.out.width = g_value_get_float (value);
	break;

    case PROP_HEIGHT:
	gpuvsink->videoConfig.out.height = g_value_get_float (value);
	break;

    case PROP_ROTATE:
    gpuvsink->videoConfig.in.rotate = g_value_get_float (value);
        break;


    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gst_render_bridge_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstBufferClassSink *gpuvsink = GST_BCSINK (object);
  switch (prop_id) {
    case PROP_QUEUE_SIZE:{
      g_value_set_uint (value, gpuvsink->num_buffers);
      break;
    }
    default:{
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  }
}


static GstStateChangeReturn
gst_render_bridge_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstBufferClassSink *gpuvsink = GST_BCSINK (element);

  GST_DEBUG_OBJECT (gpuvsink, "%d -> %d",
      GST_STATE_TRANSITION_CURRENT (transition),
      GST_STATE_TRANSITION_NEXT (transition));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      break;
    }
    default:{
      break;
    }
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      /* TODO stop streaming */
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:{
      g_signal_emit (gpuvsink, signals[SIG_CLOSE], 0);
      if (gpuvsink->pool) {
        gst_buffer_manager_dispose (gpuvsink->pool);
        gpuvsink->pool = NULL;

        if (gpuvsink->bcbuf_prev3 != NULL)
          gst_buffer_unref (gpuvsink->bcbuf_prev3);
        if (gpuvsink->bcbuf_prev2 != NULL)
          gst_buffer_unref (gpuvsink->bcbuf_prev2);
        if (gpuvsink->bcbuf_prev1 != NULL)
          gst_buffer_unref (gpuvsink->bcbuf_prev1); 
        gpuvsink->bcbuf_prev3 = NULL;
        gpuvsink->bcbuf_prev2 = NULL;
        gpuvsink->bcbuf_prev1 = NULL;
      }
      break;
    }
    default:{
      break;
    }
  }

  GST_DEBUG_OBJECT (gpuvsink, "end");

  return ret;
}

static gboolean
gst_render_bridge_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstBufferClassSink *gpuvsink = GST_BCSINK (bsink);

  g_return_val_if_fail (caps, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  if (G_UNLIKELY (gpuvsink->pool)) {
    GstCaps *current_caps = gst_buffer_manager_get_caps (gpuvsink->pool);

    GST_DEBUG_OBJECT (gpuvsink, "already have caps: %" GST_PTR_FORMAT,
        current_caps);
    if (gst_caps_is_equal (current_caps, caps)) {
      GST_DEBUG_OBJECT (gpuvsink, "they are equal!");
      gst_caps_unref (current_caps);
      return TRUE;
    }

    gst_caps_unref (current_caps);
    GST_DEBUG_OBJECT (gpuvsink, "new caps are different: %" GST_PTR_FORMAT, caps);

    // TODO
    GST_DEBUG_OBJECT (bsink, "reallocating buffers not implemented yet");
    g_return_val_if_fail (0, FALSE);
  }

  GST_DEBUG_OBJECT (gpuvsink,
      "constructing bufferpool with caps: %" GST_PTR_FORMAT, caps);

  gpuvsink->pool =
      gst_buffer_manager_new (GST_ELEMENT (gpuvsink), gpuvsink->videoConfig,
      gpuvsink->num_buffers, caps);

  if (!gpuvsink->pool) {
	return FALSE;
}
  if (gpuvsink->num_buffers != gpuvsink->pool->num_buffers) {
    GST_DEBUG_OBJECT (gpuvsink, "asked for %d buffers, got %d instead",
        gpuvsink->num_buffers, gpuvsink->pool->num_buffers);
    gpuvsink->num_buffers = gpuvsink->pool->num_buffers;
    g_object_notify (G_OBJECT (gpuvsink), "queue-size");
  }

  g_signal_emit (gpuvsink, signals[SIG_INIT], 0, gpuvsink->num_buffers);

  return TRUE;
}

/** buffer alloc function to implement pad_alloc for upstream element */
static GstFlowReturn
gst_render_bridge_buffer_alloc (GstBaseSink * bsink, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buf)
{
  GstBufferClassSink *gpuvsink = GST_BCSINK (bsink);

  if (G_UNLIKELY (!gpuvsink->pool)) {
    /* it's possible caps haven't been set yet: */
    gst_render_bridge_set_caps (bsink, caps);
    if (!gpuvsink->pool)
      return GST_FLOW_ERROR;
  }

  *buf = GST_BUFFER (gst_buffer_manager_get (gpuvsink->pool));

  if (G_LIKELY (buf)) {
    GST_DEBUG_OBJECT (gpuvsink, "allocated buffer: %p", *buf);
    return GST_FLOW_OK;
  }

  GST_DEBUG_OBJECT (gpuvsink, "failed to allocate buffer");
  return GST_FLOW_ERROR;
}

/** called after A/V sync to render frame */
static GstFlowReturn
gst_render_bridge_show_frame (GstBaseSink * bsink, GstBuffer * buf)
{
  GstBufferClassSink *gpuvsink = GST_BCSINK (bsink);
  GstBufferClassBuffer *bcbuf;
  GstBufferClassBuffer *bcbuf_rec;
  GstBuffer *newbuf = NULL;
  int n;
  static int queue_counter=0;

  GST_DEBUG_OBJECT (gpuvsink, "render buffer: %p", buf);

  if (G_UNLIKELY (!GST_IS_BCBUFFER (buf))) {
    GstFlowReturn ret;

    GST_DEBUG_OBJECT (gpuvsink, "slow-path.. I got a %s so I need to memcpy",
        g_type_name (G_OBJECT_TYPE (buf)));
    ret = gst_render_bridge_buffer_alloc (bsink,
        GST_BUFFER_OFFSET (buf), GST_BUFFER_SIZE (buf), GST_BUFFER_CAPS (buf),
        &newbuf);

    if (GST_FLOW_OK != ret) {
      GST_DEBUG_OBJECT (gpuvsink,
          "dropping frame!  Consider increasing 'queue-size' property!");
      return GST_FLOW_OK;
    }

    memcpy (GST_BUFFER_DATA (newbuf),
        GST_BUFFER_DATA (buf),
        MIN (GST_BUFFER_SIZE (newbuf), GST_BUFFER_SIZE (buf)));

    GST_DEBUG_OBJECT (gpuvsink, "render copied buffer: %p", newbuf);

    buf = newbuf;
  }

  bcbuf = GST_BCBUFFER (buf);

  /* cause buffer to be flushed before rendering */
  gst_bcbuffer_flush (bcbuf);

  //g_signal_emit (gpuvsink, signals[SIG_RENDER], 0, bcbuf->index);
  gst_buffer_ref(bcbuf);

  gpuvsink->videoConfig.config_data = 0;
  gpuvsink->videoConfig.buf_index = bcbuf->index;

   n = write(gpuvsink->fd_video_cfg, &gpuvsink->videoConfig, sizeof(gpuvsink->videoConfig));

  if(n != sizeof(gpuvsink->videoConfig))
  {
      printf("Error in writing to named pipe: %s \n", VIDEO_CONFIG_AND_DATA_FIFO_NAME);
  }
 
  /* delay the buffer free up by two frames to account for the SGX deferred rendering archtecture */
  if (gpuvsink->bcbuf_prev3 != NULL)
    gst_buffer_unref (gpuvsink->bcbuf_prev3);
  gpuvsink->bcbuf_prev3 = gpuvsink->bcbuf_prev2;
  gpuvsink->bcbuf_prev2 = gpuvsink->bcbuf_prev1;
  gpuvsink->bcbuf_prev1 = bcbuf; 
//  gst_buffer_unref(bcbuf);

  /* note: it would be nice to know when the driver is done with the buffer..
   * but for now we don't keep an extra ref
   */

  if (newbuf) {
    gst_buffer_unref (newbuf);
  }

  return GST_FLOW_OK;
}
