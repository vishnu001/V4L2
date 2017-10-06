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

typedef enum {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
} io_method;

typedef enum {
        IO_TYPE_VIDEO_CAPTURE,
        IO_VIDEO_OVERLAY,
        VIDEO_OUTPUT_OVERLAY,
} io_type;

struct buffer {
        void *                  start;
        size_t                  length;
};

struct buffer *buffers = NULL;
int buffers_n = 0;
static io_type io_t = IO_TYPE_VIDEO_CAPTURE;
static io_method io_m = IO_METHOD_MMAP;
int fd; /* camera device file descriptor */
char *camera_device = "/dev/video0";
int frames_n = 5;

void f_flush() {
        char ch;
        do {
                ch = getchar();
        }while(ch !='\n'|| ch != EOF);
}

int xioctl(int file, int request, void* argp) {
	int er;
	do {
		er = ioctl(file, request, argp);
	}while(er == -1 && errno == EINTR);
	return er;
}

int save_image(void *start, unsigned int length) {
	printf("saving frame\n");
	char filename[10];
	sprintf(filename,"out%d.raw",(5-frames_n));
	/*
	int fp= open(filename, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);      
	if(fp == -1) {
		perror("save image failed");
		return -1;
	}
	int written = 0;
	//do {
	written = write(fp, start, length);
	//length = length - written;
	//} while(0 != length);
	close(fp);
	*/
	FILE *fp=fopen(filename,"wb");                            
        fwrite(start, length, 1, fp);        
        fclose(fp);
	frames_n--;
	return 0;
}

int frameout() {
        struct v4l2_buffer buf;
	switch(io_m) {
		case IO_METHOD_READ:
			if(read(fd, (*buffers).start, (*buffers).length)==-1) {
				switch(errno) {
					case EAGAIN: 
						return 0;
					case EBUSY:
						return 0;
					default:
						perror("frameout method read");
						return -1;
				}
			}
			if(save_image((*buffers).start, (*buffers).length) == -1) {
				return -1;
			} break;
		case IO_METHOD_MMAP:
			memset(&buf, 0, sizeof(buf));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;		                
		        printf("buffer ready\n");
	                if(xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
		                switch(errno) {
			                case EAGAIN:
                                                return 0; 
			                default:
				                perror("frameout mmap");
				                break;	
		                }
	                }
	                printf("done dque. bytes used = %d\n",buf.bytesused);
	                if(buf.bytesused != 0) {
	                        if(save_image(buf.m.offset,buf.bytesused) == -1) {
		                        return -1;
	                        } 
	                }
	                if(xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
			        perror("mmap VIDIOC QBUF");
			        return -1;
		        }
			        
			
			break;
		case IO_METHOD_USERPTR:
			
			memset(&buf, 0, sizeof(buf));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			if(xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
                                switch(errno) {
                                        case EAGAIN:
                                                return 0; 
                                        default:
                                                perror("frameout mmap");
                                                break;
                                }
                        }
			printf("done dque. bytes used = %d\n",buf.bytesused);
	                if(buf.bytesused != 0) {
	                        if(save_image((*buffers).start,buf.bytesused) == -1) {
		                        return -1;
	                        } 
	                }
	                if(xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
			        perror("mmap VIDIOC QBUF");
			        return -1;
		        } break;
	}
	return 0;
}

int readinit(unsigned int buffer_size) {
	buffers = calloc(1,sizeof(*buffers));
	if(buffers == NULL) {
		perror("out of memory");
		return -1;
	}
	
	(*buffers).length = buffer_size;
	(*buffers).start = malloc(buffer_size);
	
	if((*buffers).start == NULL) {
		perror("out of memory");
		free(buffers);
		return -1;
	}
	return 0;	
}

int mmapinit() {
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if(xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
		perror("mmap req buf failed");
		return -1;
	}
	buffers = calloc(req.count, sizeof(*buffers));
	if(buffers == NULL) {
		perror("mmap calloc");
		return -1;
	}
	for(buffers_n = 0;buffers_n < req.count;buffers_n++) {
		struct v4l2_buffer buf;
		
		memset(&buf, 0, sizeof(buf));
		buf.index = buffers_n;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if(xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
			perror("mmap querybuf");
			free(buffers);
			return -1;
		}
		printf("field is %d \n",buf.field);
		buffers[buffers_n].length = buf.length;
		buffers[buffers_n].start = mmap(NULL,buf.length,PROT_READ | PROT_WRITE,MAP_SHARED,fd, buf.m.offset);
		if (buffers[buffers_n].start == MAP_FAILED) {
			perror("mmap mmap()");
			for(int k = 0;k < buffers_n;k++)
                                munmap(buffers[k].start,buffers[k].length);
			free(buffers);
			return -1;
		}
	}

	for (int i = 0; i < buffers_n; ++i) {
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
			perror("mmap VIDIOC_QBUF");
			for(int k = 0;k < buffers_n;k++)
                                munmap(buffers[k].start,buffers[k].length);
			free(buffers);
			return -1;
		}
	}
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		perror("mmap VIDIOC_STREAMON");
		for(int k = 0;k < buffers_n;k++)
                        munmap(buffers[k].start,buffers[k].length);
		free(buffers);
		return -1;
	}
	return 0;
}

int usrptrinit(unsigned int buffer_size) {
	struct v4l2_requestbuffers req;
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;
        if(xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
                perror("userptr req buf failed");
                return -1;
        }
        buffers = calloc(req.count, sizeof(*buffers));
        if(buffers == NULL) {
                perror("userptr calloc");
                return -1;
        }
        for(buffers_n = 0;buffers_n < req.count;buffers_n++) {
                buffers[buffers_n].length = buffer_size;
                buffers[buffers_n].start = malloc(buffer_size);
                if (buffers[buffers_n].start == NULL) {
                        perror("userptr v4l2_mmap()");
                        for(int k = 0;k < buffers_n;k++)
				free(buffers[buffers_n].start);
			free(buffers);
                        return -1;
                }
        }
	for (int i = 0; i < buffers_n; ++i) {
                struct v4l2_buffer buf;

                memset(&buf, 0, sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;
                buf.index = i;
		buf.m.userptr = (unsigned long)buffers[i].start;

                if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
                        perror("userptr VIDIOC_QBUF");
			for(int k = 0;k < buffers_n;k++)
                                free(buffers[buffers_n].start);
                        free(buffers);
                        return -1;
                }
        }
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
                perror("userptr VIDIOC_STREAMON");
		for(int k = 0;k < buffers_n;k++)
                        free(buffers[buffers_n].start);
                free(buffers);
                return -1;
        }
        return 0;
}
int device_init() {
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_streamparm strmparm;
	struct v4l2_fmtdesc enumfmt;
	struct v4l2_frmsizeenum enumframe;
	struct v4l2_frmivalenum frameintval;

	int fps = 60;
	
	char *frmsizetypes[4]={"none","V4L2_FRMSIZE_TYPE_DISCRETE","V4L2_FRMSIZE_TYPE_CONTINUOUS","V4L2_FRMSIZE_TYPE_STEPWISE"};
	if(ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
		switch(errno) {
			case EINVAL:
				perror("VIDIOC QUERYCAP invalid");
				break;
			default:
				perror("querycap");
				break;
		}
		return -1;
	}
	
	printf("1. VIDEO CAPTURE");
	if(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("2. VIDEO OUTPUT\n"); 
	if(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
	       printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("3. VIDEO OVERLAY\n");
	if(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY) {
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
	if(cap.capabilities & V4L2_CAP_VBI_OUTPUT) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("6. SLICED_VBI_CAPTURE\n");
	if(cap.capabilities & V4L2_CAP_SLICED_VBI_CAPTURE) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("7. SLICED_VBI_OUTPUT\n");
	if(cap.capabilities & V4L2_CAP_SLICED_VBI_OUTPUT) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("8. VIDEO_OUTPUT_OVERLAY\n");
	if(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_OVERLAY) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		perror("video capture device\n");
                close(fd);
		return -1;
	}
	printf("V4L2_BUF_TYPE_VIDEO_CAPTURE is taken\n");
	
	
	printf("1. READWRITE METHOD ");
	if(cap.capabilities & V4L2_CAP_READWRITE) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	
	printf("2. MMAP METHOD: y\n"); 
	
	printf("3. USER POINTER METHOD\n");
	if(cap.capabilities & V4L2_CAP_STREAMING) {
	        printf(": y\n");
	} else {
	        printf(": n\n");
	}
	int iom;
	printf("choose one method : ");
	scanf("%d",&iom);
	//f_flush();

	switch (iom) {
		case 1:
			io_m = IO_METHOD_READ;
			if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
				printf("does not support read i/o\n");
				close(fd);
				return -1;
			}
			break;
		case 2: printf("support mmap\n");
		        break;
		case 3:
                        io_m = IO_METHOD_USERPTR;
      			if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
				printf("does not support streaming i/o\n");
				close(fd);
				return -1;
			}
			break;
	}
	
	memset(&cropcap, 0, sizeof(cropcap));
	
	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
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
	}
	int ret_enum;
	int i=0;
	do {
		memset(&enumfmt, 0, sizeof(enumfmt));
		enumfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		enumfmt.index = i;
		ret_enum = xioctl(fd, VIDIOC_ENUM_FMT, &enumfmt);
		if(errno == EINVAL) {
			perror("enum fmt");
			//break;
	        }
		printf("%d, V4L2_BUF_TYPE_VIDEO_CAPTURE, flags=0x%X, description= %s, pixelformat=0x%X\n",	\
		enumfmt.index, enumfmt.flags, enumfmt.description, enumfmt.pixelformat);
		i++;
	}while(ret_enum == 0);
	
	i=1;
	ret_enum=0;
	do {
		memset(&enumframe, 0, sizeof(enumframe));
		enumframe.index = i;
		ret_enum = xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &enumframe);
		if(errno == EINVAL) {
			perror("enum frame");
			//break;
		}
		printf("%d, pixelformat=0x%X, framesize type=%s, res=%dx%d\n", \
                i, enumframe.pixel_format, frmsizetypes[enumframe.type],enumframe.discrete.width,enumframe.discrete.height);
		i++;
	}while(ret_enum == 0);

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
	if(xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		perror("set video format");
		//close(fd);
		//return -1;
	}
	if(xioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		perror("get video format");
		//close(fd);
		//return -1;
	}
	printf("width=%d, height=%d\n",fmt.fmt.pix.width,fmt.fmt.pix.height);
	if(fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_UYVY) {
		printf("error: UYVY format not supported\n");
		//close(fd);
		//return -1;
	}
	
	i = 1;
	do {
		memset(&frameintval, 0, sizeof(frameintval));
		frameintval.index = i;
		frameintval.pixel_format = V4L2_PIX_FMT_UYVY;
		frameintval.width = fmt.fmt.pix.width;
		frameintval.height = fmt.fmt.pix.height;
		ret_enum = xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frameintval);
		//if(errno == EINVAL)
			//break;
		printf("%d,V4L2_FRMIVAL_TYPE_DISCRETE, fps=%d \n",i,frameintval.discrete.denominator);
		i++;
	}while(ret_enum == 0);
	
        memset(&strmparm, 0, sizeof(strmparm));
	strmparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	strmparm.parm.capture.timeperframe.numerator = 1;
    	strmparm.parm.capture.timeperframe.denominator = fps;
    	if (-1 == xioctl(fd, VIDIOC_S_PARM, &strmparm))
     		 fprintf(stderr,"Unable to set frame interval.\n");

	switch (io_m) {
                case IO_METHOD_READ:
                        if(readinit(fmt.fmt.pix.sizeimage)== -1) {
				close(fd);
				return -1;
			}
			
                        break;

                case IO_METHOD_MMAP:
                        if(mmapinit() == -1) {
				close(fd);
				return -1;
			}
                        break;

                case IO_METHOD_USERPTR:
                        if(usrptrinit(fmt.fmt.pix.sizeimage) == -1) {
				close(fd);
				return -1;
			}
                        break;
        }
	return 0;
}


int device_open() {
	struct stat st;
	if(stat(camera_device, &st) == -1) {
		printf("no such device\n");
		return -1;
	}
	
	if(!S_ISCHR(st.st_mode)) {
		printf("not a char device\n");
		return -1;
	}
	
	fd = open(camera_device,O_RDWR | O_NONBLOCK,0);
	
	if(fd == -1) {
		perror("camera device open\n");
	}
	
	return 0;
}

void safe_exit() {
        int type;
        switch(io_m) {
        case IO_METHOD_READ: break;
        case IO_METHOD_MMAP:
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if(xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
                        perror("userptr VIDIOC_STREAMOFF");
		        for (int i = 0; i < buffers_n; ++i)
			        if (-1 == munmap(buffers[i].start, buffers[i].length))
		                        exit(EXIT_FAILURE);        
                        free(buffers);
                        perror("STREAMOFF");
                        exit(EXIT_FAILURE);
                }
                for (int i = 0; i < buffers_n; ++i)
		        if (-1 == munmap(buffers[i].start, buffers[i].length))
	                        exit(EXIT_FAILURE);
                free(buffers);
                break;
                
        case IO_METHOD_USERPTR:
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if(xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
                        perror("userptr VIDIOC_STREAMOFF");
		        for(int k = 0;k < buffers_n;k++)
                                free(buffers[buffers_n].start);
                        free(buffers);
                        perror("STREAMOFF");
                        exit(EXIT_FAILURE);
                }
                for(int k = 0;k < buffers_n;k++)
                        free(buffers[buffers_n].start);
                free(buffers);
                break;
        }
}
int main() {
	printf("starting application\n");
	if(device_open() == -1) {
		printf("terminating application\n");
		exit(EXIT_FAILURE);
	}

	if(device_init() == -1) {
		printf("terminating appication\n");
		exit(EXIT_FAILURE);
	}
	while(frames_n > 0) {
	
	        fd_set rfds;
                struct timeval tv;
                int retval;

                /* Watch stdin (fd 0) to see when it has input. */
                FD_ZERO(&rfds);
                FD_SET(fd, &rfds);

                /* Wait up to five seconds. */
                tv.tv_sec = 3;
                tv.tv_usec = 0;

                retval = select(fd+1, &rfds, NULL, NULL, &tv);
                /* Don't rely on the value of tv now! */

                if (retval == -1)
                        perror("select()");
                else if (retval) {
                        //printf("Data is available now.\n");
                        if(frameout() == -1) {
		                printf("terminating application");
		                safe_exit();
		                exit(EXIT_FAILURE);
		        }
                }
                /* FD_ISSET(fd, &rfds) will be true. */
                else
                        printf("No data within 3 seconds.\n");
	        
	}
        safe_exit();
        printf("exiting application \n");
	return 0;
}
