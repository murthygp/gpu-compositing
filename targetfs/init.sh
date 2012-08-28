# Named pipes for video config and data 
# -----------------------------------------------------

ls /opt/gpu-compositing/named_pipes &> /dev/NULL
if [ $? -eq 1 ]
then
mkdir -p /opt/gpu-compositing/named_pipes
fi

# Video Plane #0
ls /opt/gpu-compositing/named_pipes/video_cfg_plane_0 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/video_cfg_plane_0
fi
ls /opt/gpu-compositing/named_pipes/video_data_plane_0 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/video_data_plane_0
fi

# Video Plane #1
ls /opt/gpu-compositing/named_pipes/video_cfg_plane_1 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/video_cfg_plane_1
fi
ls /opt/gpu-compositing/named_pipes/video_data_plane_1 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/video_data_plane_1
fi

# Video Plane #2
ls /opt/gpu-compositing/named_pipes/video_cfg_plane_2 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/video_cfg_plane_2
fi
ls /opt/gpu-compositing/named_pipes/video_data_plane_2 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/video_data_plane_2
fi

# Video Plane #3
ls /opt/gpu-compositing/named_pipes/video_cfg_plane_3 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/video_cfg_plane_3
fi
ls /opt/gpu-compositing/named_pipes/video_data_plane_3 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/video_data_plane_3
fi

#----------------------------------
# Named pipes for Graphics config
# --------------------------------
# Graphics Plane #0
ls /opt/gpu-compositing/named_pipes/gfx_cfg_plane_0 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/gfx_cfg_plane_0
fi

# Graphics Plane #1
ls /opt/gpu-compositing/named_pipes/gfx_cfg_plane_1 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/gfx_cfg_plane_1
fi

# Graphics Plane #2
ls /opt/gpu-compositing/named_pipes/gfx_cfg_plane_2 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/gfx_cfg_plane_2
fi

# Graphics Plane #3
ls /opt/gpu-compositing/named_pipes/gfx_cfg_plane_3 &> /dev/NULL
if [ $? -eq 1 ]
then
mkfifo -m 644 /opt/gpu-compositing/named_pipes/gfx_cfg_plane_3
fi

