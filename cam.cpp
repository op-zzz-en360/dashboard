#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
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
#include <libv4l1.h>
#include <signal.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <iostream>
#include <limits>
#include <cstdint>
#include <cctype>
 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include <net/if.h>
#include <arpa/inet.h>

#include "capturer_mmap.h"
#include "yuv.h"
#include <jpeglib.h>

const int nCount = 2; //number of cameras 

//struct buffer *         buffersHolder[VIDIOC_REQBUFS_COUNT*2];
//static unsigned int     nBuffersHolder[VIDIOC_REQBUFS_COUNT*2];

// global settings
static unsigned int width = 1280; 
static unsigned int height = 720;
static unsigned char jpegQuality = 99;

VideoStream	rgb_stream[nCount];

unsigned char *rgb_frame_buffer[nCount];

static const char* camera1DeviceName = "video0";
static const char* camera2DeviceName = "video1";

static const char* const cameras[] = {camera1DeviceName, camera2DeviceName};

struct save_arg {
	pthread_t thread_id;        /* ID returned by pthread_create() */
	int thread_num;
	char *camera_id;
	int cameraId;
	const char * jpegFilename;
};

void error( char *msg )
{
	perror(  msg );
	exit(1);
}

void sendData( int sockfd, int x ) {
  int n;

  char buffer[1024];
  sprintf( buffer, "%d\n", x );
  if ( (n = write( sockfd, buffer, strlen(buffer) ) ) < 0 )
    error( const_cast<char *>( "ERROR writing to socket") );
  buffer[n] = '\0';
}

int getData( int sockfd ) {
  char buffer[1024];
  int n;

  if ( (n = read(sockfd,buffer,1023) ) < 0 )
    error( const_cast<char *>( "ERROR reading from socket") );
  buffer[n] = '\0';
  return atoi( buffer );
}

void initVideoStream(int cameraId, char * deviceName)
{
    memset(&rgb_stream[cameraId], 0, sizeof(VideoStream));
    strncpy(rgb_stream[cameraId].videoName, deviceName, sizeof(rgb_stream[cameraId].videoName));
    rgb_stream[cameraId].width = width;//1280;
    rgb_stream[cameraId].height = height;//720;
    rgb_stream[cameraId].pixelFormat = V4L2_PIX_FMT_YUV420;
    rgb_stream[cameraId].fd = -1;
}

int processRGB(int cameraId)
{
    int stream_state = 0;
    stream_state = capturer_mmap_get_frame(&rgb_stream[cameraId]);
    return stream_state;
}

void configureAsDaemon()
{
    /* Our process ID and Session ID */
	pid_t pid, sid;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	/* If we got a good PID, then
	we can exit the parent process. */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}
	
	// /* Change the file mode mask */
	umask(0);

	/* Open any logs here */

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		exit(EXIT_FAILURE);
	}
	
	/* Go to working directory */
	if ((chdir("/cam")) < 0) {
		exit(EXIT_FAILURE);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	int policy;
	struct sched_param param;

	pthread_getschedparam(pthread_self(), &policy, &param);
	param.sched_priority = sched_get_priority_max(policy);
	pthread_setschedparam(pthread_self(), policy, &param);
}

void *saveFrameTask(void *arguments)
{
	struct save_arg *args = (struct save_arg *)arguments;
	const char * jpegFilename;
	char * c_id;
	int cameraId;
	
	jpegFilename = (const char *)args -> jpegFilename;
	c_id = (char *)args -> camera_id;
	cameraId = (int) args -> cameraId;
	fprintf(stderr, "process camera: %s \n", c_id);
	
	char* deviceName = NULL;
	deviceName = const_cast<char*>((std::string("/dev/") + c_id).c_str());

	// initVideoStream(cameraId, deviceName);
	// if(capturer_mmap_init(&rgb_stream[cameraId]))
    // {
    //     printf("open %s error!!!!!!!!\n", rgb_stream[cameraId].videoName);
    //     return 0;
    // }
	
    rgb_frame_buffer[cameraId] = new unsigned char[rgb_stream[cameraId].width * rgb_stream[cameraId].height * 2];
    
    int stream_rgb_state = 0;

    stream_rgb_state = processRGB(cameraId);

    memcpy(rgb_frame_buffer[cameraId], rgb_stream[cameraId].fillbuf, rgb_stream[cameraId].buflen);

	unsigned char* src = (unsigned char*)rgb_frame_buffer[cameraId];
	unsigned char* dst = (unsigned char*)malloc(rgb_stream[cameraId].width * rgb_stream[cameraId].height * 3 * sizeof(char));

	YUV420toYUV444(rgb_stream[cameraId].width, rgb_stream[cameraId].height, src, dst);

    struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	JSAMPROW row_pointer[1];
	FILE *outfile = fopen( jpegFilename, "wb" );

	// try to open file for saving
	if (!outfile) {
		printf("jpeg failed");
	}

	// create jpeg data
	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, outfile);

	// set image parameters
	cinfo.image_width = rgb_stream[cameraId].width;
	cinfo.image_height = rgb_stream[cameraId].height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_YCbCr;

	// set jpeg compression parameters to default
	jpeg_set_defaults(&cinfo);
	// and then adjust quality setting
	jpeg_set_quality(&cinfo, jpegQuality, TRUE);

	// start compress
	jpeg_start_compress(&cinfo, TRUE);

	// feed data
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &dst[cinfo.next_scanline * cinfo.image_width *  cinfo.input_components];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	// finish compression
	jpeg_finish_compress(&cinfo);

	// destroy jpeg data
	jpeg_destroy_compress(&cinfo);

	// close output file
	fclose(outfile);
	
    //capturer_mmap_exit(&rgb_stream[cameraId]);
	
    delete[] rgb_frame_buffer[cameraId];
    
	free(dst);

//	if(jpegFilenamePart != 0){ 
//		free(jpegFilename);
//	}
	
	fprintf(stderr, "exiting %s thread\n", c_id);
	pthread_exit(NULL);
}

void grabImages( void ) {
   struct save_arg *args;
   pthread_attr_t attr;
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
   args = (save_arg*)calloc(nCount, sizeof(struct save_arg));
   int threadId = 0;
   void *res;
   int s;
   int k = 0;
   for(const char* camera : cameras)
   {
	   char* jpegFilename = NULL;
	   char* jpegFilenamePart = NULL;
	   
	   struct timeval tp;
	   
	   gettimeofday(&tp, NULL);
	   long long ms = (long long) tp.tv_sec * 1000L + tp.tv_usec / 1000; //get current timestamp in milliseconds
	   
	   int ip_fd;
	   struct ifreq ifr;
	   ip_fd = socket(AF_INET, SOCK_DGRAM, 0);
	   ifr.ifr_addr.sa_family = AF_INET;
	   strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
	   ioctl(ip_fd, SIOCGIFADDR, &ifr);
	   close(ip_fd);
	   
	   //int ip_length = sizeof(inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr))/sizeof(char);
	   
	//    if (ip_lenght < 10)
	//    {
	// 		ip_fd = socket(AF_INET, SOCK_DGRAM, 0);
	//    		ifr.ifr_addr.sa_family = AF_INET;
	//    		strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
	//    		ioctl(ip_fd, SIOCGIFADDR, &ifr);
	//    		close(ip_fd);
	//    }
	   
	   jpegFilename = const_cast<char*>(((char*) camera
	   		+ std::string("_") + inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr)
	   		+ std::string("_") + std::to_string(ms)
	   		+ std::string(".jpeg")).c_str());
	   
	   jpegFilenamePart = jpegFilename;
	   jpegFilename = (char *)calloc((unsigned)strlen(jpegFilenamePart)+1,sizeof(char));
	   strcpy(jpegFilename,jpegFilenamePart);
	   
	   args[threadId].thread_num = threadId + 1;
	   args[threadId].camera_id = (char*) camera;
	   args[threadId].jpegFilename = jpegFilename;
	   args[threadId].cameraId = k;
	   
	   pthread_create(&args[threadId].thread_id, NULL, saveFrameTask, &args[threadId]);
	   k++;
	   threadId++;
	}
	
	s = pthread_attr_destroy(&attr);
		
	// free attribute and wait for the other threads
	for (int tnum = 0; tnum < nCount; tnum++) {
		s = pthread_join(args[tnum].thread_id, &res);
		//free(res);      /* Free memory allocated by thread */
	}
			
	free(args);
}

int main(int argc, char **argv)
{
    configureAsDaemon();
	///////////////////////////////////////////////////////////////////////////
	///NETWORK
	///////////////////////////////////////////////////////////////////////////
	
	int sockfd, newsockfd, portno = 51717, clilen;
    //char buffer[1024];
    struct sockaddr_in serv_addr, cli_addr;
    //int n;
    int data;

    printf( "using port #%d\n", portno );
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
	{
		error( const_cast<char *>("ERROR opening socket") );	
	} 
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons( portno );
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		error( const_cast<char *>( "ERROR on binding" ) );	
	} 
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
  
    int v = 0;
  	for(const char* camera : cameras)
   	{
		   
		char* deviceName = NULL;
		deviceName = const_cast<char*>((std::string("/dev/") + (char*) camera).c_str());
		initVideoStream(v, deviceName);
		if(capturer_mmap_init(&rgb_stream[v]))
		{
			printf("open %s error!!!!!!!!\n", rgb_stream[v].videoName);
			return 0;
		}
	   v++;
	}
  
    //--- infinite wait on a connection ---
    while ( 1 ) {
		printf( "waiting for new client...\n" );
		if ( ( newsockfd = accept( sockfd, (struct sockaddr *) &cli_addr, (socklen_t*) &clilen) ) < 0 )
		{
			error( const_cast<char *>("ERROR on accept") );	
		}
		printf( "opened new communication with client\n" );
        while ( 1 ) {
			//---- wait for a number from client ---
			data = getData( newsockfd );
			printf( "got %d\n", data );
			if ( data < 0 )
			{
				break;	
			}
				
			grabImages();
			 
            //--- send new data back --- 
            printf( "sending back %d\n", data );
            sendData( newsockfd, data );
        }
        close( newsockfd );
		
		//--- if -2 sent by client, we can quit ---
		if ( data == -2 )
		{
			printf( "~~~~ we are on the right track ~~~~\n");	
		}
    }
	
	int w = 0;
  	for(const char* camera : cameras)
   	{
    	capturer_mmap_exit(&rgb_stream[w]); 	   
		w++;
	}
	
    exit(EXIT_SUCCESS);
}