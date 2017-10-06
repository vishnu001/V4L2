#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <jpeglib.h>
#include <libv4l2.h>
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>
#include <linux/fb.h>

//#define v4l2_fourcc(a,b,c,d) (((__u32)(a)<<0)|((__u32)(b)<<8)|((__u32)(c)<<16)|((__u32)(d)<<24))

typedef enum {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
} io_method;

typedef enum {
        IO_TYPE_VIDEO_CAPTURE,
        //IO_TYPE_VIDEO_OUTPUT,
        IO_VIDEO_OVERLAY,
        VIDEO_OUTPUT_OVERLAY,
} io_type;


static io_type io_t = IO_TYPE_VIDEO_CAPTURE;
static io_method io_m = IO_METHOD_USERPTR;
int fd; /* camera device file descriptor */
int fd_fb; /* display device file descriptor */
char *camera_device = "/dev/video0";
char *display_device = "/dev/fb0";
int overlay = 1;

int fps = 30; /* camera frames per second */

void f_flush() {
        char ch;
        do {
                ch = getchar();
        }while(ch !='\n'|| ch != EOF);
}

static int xioctl(int fd, int request, void* argp)
{
	int r;

	do r = ioctl(fd, request, argp);
	while (r == -1 && errno == EINTR);

	return r;
}

int device_open() {     /* open the camera device file */
        struct stat sb;
        struct stat sb_fb;
        
        if(stat(camera_device, &sb) == -1) {
                perror("no such file");
                return -1;
        }
        if(!S_ISCHR(sb.st_mode)) {
                perror("not a char device");
                return -1;
        }
        
        fd = open(camera_device, O_RDWR | O_NONBLOCK, 0);
        
        if(fd == -1) {
                perror("failed to open device file\n");
                return -1;
        }
        
        if(stat(display_device, &sb_fb) == -1) {
                perror("no such device");
                return -1;
        }
        
        if(!S_ISCHR(sb_fb.st_mode)) {
                perror("not a char device");
                return -1;
        }
        
        fd_fb = open(display_device, O_RDWR);
        
        if (fd_fb == -1) {
                perror("cannot open framebuffer device");
                exit(1);
        }
        
        return 0;
}

int device_init() {

/*============camera initial setup============*/
        struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_streamparm frameinit;
	struct v4l2_framebuffer fb_v4l2;
	
	if(xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
	        perror("QUERYCAP ioctl");
	        return -1;
	}
	printf("1. VIDEO CAPTURE");
	if(cap.capabilities && V4L2_CAP_VIDEO_CAPTURE) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("2. VIDEO OUTPUT\n"); 
	if(cap.capabilities && V4L2_CAP_VIDEO_OUTPUT) {
	       printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("3. VIDEO OVERLAY\n");
	if(cap.capabilities && V4L2_CAP_VIDEO_OVERLAY) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("4. VBI CAPTURE\n");
	if(cap.capabilities && V4L2_CAP_VBI_CAPTURE) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("5. VBI OUTPUT\n");
	if(cap.capabilities && V4L2_CAP_VBI_OUTPUT) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("6. SLICED_VBI_CAPTURE\n");
	if(cap.capabilities && V4L2_CAP_SLICED_VBI_CAPTURE) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("7. SLICED_VBI_OUTPUT\n");
	if(cap.capabilities && V4L2_CAP_SLICED_VBI_OUTPUT) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("8. VIDEO_OUTPUT_OVERLAY\n");
	if(cap.capabilities && V4L2_CAP_VIDEO_OUTPUT_OVERLAY) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
        
        printf("video capture overlay is choosen for video buffering\n");
        /*
        printf("choose one from 1, 3, 8\n");
        int iot = 0;
        scanf("%d",&iot);
        f_flush();
        
        switch(iot){
        case 1:
                io_t = IO_TYPE_VIDEO_CAPTURE;
                if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		        perror("video capture device\n");
		        close(fd_fb);
                        close(fd);
		        return -1;
	        }
	        break;
	case 3:
	        io_t = IO_TYPE_VIDEO_OVERLAY;
	        if (!(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY)) {
		        close(fd_fb);
                        close(fd);
		        perror("no video capture device\n");
		        return -1;
	        }
	        break;
	case 8:
                io_t = IO_TYPE_VIDEO_OUTPUT_OVERLAY;
                if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)) {
		        close(fd_fb);
                        close(fd);
		        perror("no video capture device\n");
		        return -1;
	        }
	        break;
	default:
	        printf("this support is not currently available\nVIDEO CAPTURE selected\n");
	        
        }
        
        */
        
        //printf("choose from one of the read write method\n");
        printf("1. READWRITE METHOD ");
	if(cap.capabilities && V4L2_CAP_READWRITE) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("2. MMAP METHOD: y\n"); 
	
	printf("3. USER POINTER METHOD\n");
	if(cap.capabilities && V4L2_CAP_STREAMING) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	/* depricated, since streaming is to be done. user pointer is set as default */
	/*
	int iom;
	printf("method : ");
	scanf("%d",&iom);
	f_flush();
	
	
	switch (iom) {
		case 1:
			io_m = IO_METHOD_READ;
			if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
				fprintf(stderr, "%s does not support read i/o\n",deviceName);
				exit(EXIT_FAILURE);
			}
			break;
		case 2:
		case 3:
                        io_m = IO_METHOD_USERPTR;
      			if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
				fprintf(stderr, "%s does not support streaming i/o\n",deviceName);
				exit(EXIT_FAILURE);
			}
			break;
	}
	*/
	
	printf("userptr method is set as default for display overlay\n");
	
	memset(&cropcap, 0, sizeof(cropcap));
	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					printf("crop not supported\n");
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	} else {
		/* Errors ignored. */
	}
	
	memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
	fmt.fmt.pix.pixelformat = v4l2_fourcc('B','G','R','3');
	fmt.fmt.win.w.top =  0 ;
        fmt.fmt.win.w.left = 0;
        fmt.fmt.win.w.width = 640;
        fmt.fmt.win.w.height = 480;
        if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
        {
                close(fd_fb);
                close(fd);
                printf("set format failed\n");
                return -1;
        }
	
	memset(&frameinit, 0, sizeof(frameinit));
	frameinit.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        frameinit.parm.capture.timeperframe.numerator = 1;
        frameinit.parm.capture.timeperframe.denominator = fps;
        
        
        if (-1 == xioctl(fd, VIDIOC_S_PARM, &frameinit))
                fprintf(stderr,"Unable to set frame interval.\n");
        
        
	
/*============display initial setup============*/
        
        struct fb_fix_screeninfo fb_fixed;
        struct fb_var_screeninfo fb_variable;
        
        if (ioctl(fd_fb, FBIOGET_FSCREENINFO, &fb_fixed) == -1) {
                close(fd_fb);
                close(fd);
                perror("FBIOGET_FSCREENINFO");
                return -1;
        } else {
                printf("display device ID : %s", fb_fixed.id);
        }
        
                close(fd_fb);
                close(fd);
                perror("FBIOGET_VSCREENINFO");
        if (ioctl(fd_fb, FBIOGET_VSCREENINFO, &fb_variable) == -1) {
                return -1;
        }
        
        memset(&fb_v4l2, 0, sizeof(fb_v4l2));
        
        fb_v4l2.fmt.width = fb_variable.xres;
        fb_v4l2.fmt.height = fb_variable.yres;
        fb_v4l2.fmt.pixelformat = v4l2_fourcc('B','G','R','3');
        fb_v4l2.fmt.bytesperline = 3 * fb_v4l2.fmt.width;
       
        
        fb_v4l2.flags = V4L2_FBUF_FLAG_OVERLAY;
        
        fb_v4l2.base = (unsigned long *)fb_fixed.smem_start;
        
/*===========initiate video buffering===========*/
        
        close(fd_fb);           
        
        if (ioctl(fd, VIDIOC_S_FBUF, &fb_v4l2) == -1) {
                close(fd);
                perror("set frame buffer");
                return -1;
        }
        
        if (ioctl(fd, VIDIOC_OVERLAY, &overlay) == -1) {
                close(fd);
                perror("start video overlay");
                return -1;
        }
        char ch;
        do {
                ch = getchar();        
        }while(ch == 'q');
        overlay = 0;
        
        if (ioctl(fd, VIDIOC_OVERLAY, &overlay) == -1) {
                perror("error closing video overlay");
                close(fd);
                return -1;
        }
        
        close(fd);
        
        return 0;    

}

/*
 * V4L2_BUF_TYPE_VIDEO_CAPTURE	1	Buffer of a video capture stream, see Section 4.1, “Video Capture Interface”.
 * V4L2_BUF_TYPE_VIDEO_OUTPUT	2	Buffer of a video output stream, see Section 4.3, “Video Output Interface”.
 * V4L2_BUF_TYPE_VIDEO_OVERLAY	3	Buffer for video overlay, see Section 4.2, “Video Overlay Interface”.
 * V4L2_BUF_TYPE_VBI_CAPTURE	4	Buffer of a raw VBI capture stream, see Section 4.7, “Raw VBI Data Interface”.
 * V4L2_BUF_TYPE_VBI_OUTPUT	5	Buffer of a raw VBI output stream, see Section 4.7, “Raw VBI Data Interface”.
 * V4L2_BUF_TYPE_SLICED_VBI_CAPTURE	6	Buffer of a sliced VBI capture stream, see Section 4.8, “Sliced VBI Data Interface”.
 * V4L2_BUF_TYPE_SLICED_VBI_OUTPUT	7	Buffer of a sliced VBI output stream, see Section 4.8, “Sliced VBI Data Interface”.
 * V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY	8	Buffer for video output overlay (OSD), see Section 4.4, “Video Output Overlay Interface”. Status: Experimental.
 * V4L2_BUF_TYPE_PRIVATE	0x80	This and higher values are reserved for custom (driver defined) buffer types.
 */
 
int main() {
        printf("Video steaming application has started!\n");
        
        if(device_open() == -1) {
                printf("failed to open device!\nEXITING APPLICATION\n");
                exit(EXIT_FAILURE);    
        }

        if(device_init() == -1) {
                printf("EXITING APPLICATION!\n");
                exit(EXIT_FAILURE);
        }
        
         
        
        return 0;
}
