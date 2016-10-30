#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

#include <iostream>
#include <fstream>
#include <limits>
#include <cstdint>
#include <cctype>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "json.hpp"

#include <gps.h>
#include <math.h>

// for convenience
using json = nlohmann::json;

int nCount;

void error(char *msg) {
    perror(msg);
    exit(0);
}

void sendData( int sockfd, int x ) {
  int n;

  char buffer[1024];
  sprintf( buffer, "%d\n", x );
  if ( (n = write( sockfd, buffer, strlen(buffer) ) ) < 0 )
      error( const_cast<char *>( "ERROR writing to socket\n") );
  buffer[n] = '\0';
}

int getData( int sockfd ) {
  char buffer[1024];
  int n;

  if ( (n = read(sockfd,buffer,1023) ) < 0 )
       error( const_cast<char *>( "ERROR reading from socket\n") );
  buffer[n] = '\0';
  return atoi( buffer );
}

struct command_arg {
	pthread_t thread_id;        /* ID returned by pthread_create() */
	int thread_num;
	const char * deviceHostName;
};

struct download_images_arg {
	pthread_t thread_id;        /* ID returned by pthread_create() */
	int thread_num;
	const char * deviceHostName;
    long long ms;
};

struct cleanup_arg {
	pthread_t thread_id;        /* ID returned by pthread_create() */
	int thread_num;
	const char * deviceHostName;
};

void *cleanupTask(void *arguments)
{
	struct cleanup_arg *args = (struct cleanup_arg *)arguments;
	const char * serverIp;
    
	serverIp = (const char *)args -> deviceHostName;
    
    char exec_rm[180];
    sprintf(exec_rm,"ssh pi@%s 'sudo rm -rf /cam/*'", serverIp);
    if(system(exec_rm)==0) 
        printf("Dir %s cleaned!\n", serverIp);
    else
        printf("Dir %s not cleaned   :(\n", serverIp);   
    
	pthread_exit(NULL);
}

void *downloadImagesTask(void *arguments)
{
	struct download_images_arg *args = (struct download_images_arg *)arguments;
	const char * serverIp;
    long long ms;
	
	serverIp = (const char *)args -> deviceHostName;
    ms = (long long)args -> ms;
    
    char exec_cp[180];
    sprintf(exec_cp,"ssh pi@%s 'scp -o StrictHostKeyChecking=no /cam/* pi@192.168.88.10:/cam/%s'", serverIp, std::to_string(ms).c_str());
    if(system(exec_cp)==0) 
        printf("Files %s copied!\n", serverIp);
    else
        printf("Files %s not copied :(\n", serverIp);
    
	pthread_exit(NULL);
}

void *sendCommandTask(void *arguments)
{
	struct command_arg *args = (struct command_arg *)arguments;
	const char * serverIp;
	
	serverIp = (const char *)args -> deviceHostName;
    
    int sockfd, portno = 51717, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int data;

    printf( "contacting %s on port %d\n", serverIp, portno );
    if ( ( sockfd = socket(AF_INET, SOCK_STREAM, 0) ) < 0 )
        error( const_cast<char *>( "ERROR opening socket\n") );

    if ( ( server = gethostbyname( serverIp ) ) == NULL ) 
        error( const_cast<char *>("ERROR, no such host\n") );
    
    bzero( (char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy( (char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if ( connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
    {
        error( const_cast<char *>( "ERROR connecting") );
    }

    for ( n = 0; n < 1; n++ ) {
      sendData( sockfd, n );
      data = getData( sockfd );
      printf("%d ->  %d\n",n, data );
      //usleep( 5000 * 1000 );
    }
    sendData( sockfd, -2 );

    close( sockfd );
    
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int opt;
    int imageCount;
    
    while ((opt = getopt(argc, argv, "hc:")) != -1) {
        switch (opt) {
        case 'h':
            //show help
            break;
        case 'c':
            imageCount = atoi(optarg);
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-h] [-c] image count\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    
    if (imageCount <= 0)
    {
        printf("please define image count greater than 0 with a '-c' option \n");
        exit(EXIT_FAILURE);
    }

    printf("image count = %d\n", imageCount);
   
    std::ifstream t("/home/pi/cam-new/config.json");
    std::stringstream buffer;
    buffer << t.rdbuf();
   
    auto j = json::parse(buffer.str());
    
    json jcameras = j["cameras"];
    
    nCount = jcameras.size();
    const int camCount = nCount;
    printf("camera count: %d\n", nCount);
    
    const char* cameraDeviceHostNames[camCount] = {};
    
    for (int camnum = 0; camnum < nCount; camnum++) {
        cameraDeviceHostNames[camnum] = (jcameras[camnum]["ipaddr"].get<std::string>()).c_str();
    }
    
    /* Go to working directory */
	if ((chdir("/cam")) < 0) {
		exit(EXIT_FAILURE);
	}
    
    setbuf(stdout, NULL);    
    struct command_arg *args;
    struct download_images_arg *download_args;
    struct cleanup_arg *cleanup_args;
    
    int rc;
    struct timeval tv;
    struct gps_data_t gps_data;
    
    if ((rc = gps_open("localhost", "2947", &gps_data)) == -1) {
        printf("code: %d, reason: %s\n", rc, gps_errstr(rc));
        return EXIT_FAILURE;
    }
    gps_stream(&gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
    
    for(const char* cameraDeviceHostName : cameraDeviceHostNames)
    {
        char exec[180];
        sprintf(exec,"ssh pi@%s '/home/pi/cam-new/cam'", cameraDeviceHostName);
        if(system(exec)==0) 
            printf("Camera daemon %s started!\n", cameraDeviceHostName);
        else
            printf("Camera daemon %s failed to start :(\n", cameraDeviceHostName);   
    }
    
    for ( int j = 0; j < imageCount; j++ ) {
        
        if (gps_waiting (&gps_data, 2000000)) {
            /* read data */
            if ((rc = gps_read(&gps_data)) == -1) {
                printf("error occured reading gps data. code: %d, reason: %s\n", rc, gps_errstr(rc));
            } else {
                /* Display data from the GPS receiver. */
                if ((gps_data.status == STATUS_FIX) && 
                    (gps_data.fix.mode == MODE_2D || gps_data.fix.mode == MODE_3D) &&
                    !isnan(gps_data.fix.latitude) && 
                    !isnan(gps_data.fix.longitude)) {
                        gettimeofday(&tv, NULL);
                        printf("latitude: %f, longitude: %f, speed: %f, timestamp: %ld\n", gps_data.fix.latitude, gps_data.fix.longitude, gps_data.fix.speed, tv.tv_sec);
                } else {
                    printf("no GPS data available\n");
                }
            }
        }       
        
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
        args = (command_arg*)calloc(nCount, sizeof(struct command_arg));
        download_args = (download_images_arg*)calloc(nCount, sizeof(struct download_images_arg));
        cleanup_args = (cleanup_arg*)calloc(nCount, sizeof(struct cleanup_arg));
        int threadId = 0;
        void *res;
        int s;

        struct timeval tp;
        gettimeofday(&tp, NULL);
        long long ms = (long long) tp.tv_sec * 1000L + tp.tv_usec / 1000; //get current timestamp in milliseconds
        
        char exec_mkdir[180];
        sprintf(exec_mkdir,"sudo mkdir /cam/%s", std::to_string(ms).c_str());
        system(exec_mkdir);
        
        char exec_chmod[180];
        sprintf(exec_chmod,"sudo chmod 777 /cam/*");
        system(exec_chmod);
        
        for(const char* cameraDeviceHostName : cameraDeviceHostNames)
        {
            args[threadId].thread_num = threadId + 1;
            args[threadId].deviceHostName = cameraDeviceHostName;
            
            pthread_create(&args[threadId].thread_id, NULL, sendCommandTask, &args[threadId]);
            threadId++;
        }
        
        s = pthread_attr_destroy(&attr);
        
        // free attribute and wait for the other threads
        for (int tnum = 0; tnum < nCount; tnum++) {
            s = pthread_join(args[tnum].thread_id, &res);
            //free(res);      /* Free memory allocated by thread */
        }
        
        threadId = 0;
        
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        
        for(const char* cameraDeviceHostName : cameraDeviceHostNames)
        {
            download_args[threadId].thread_num = threadId + 1;
            download_args[threadId].deviceHostName = cameraDeviceHostName;
            download_args[threadId].ms = ms;
            
            pthread_create(&download_args[threadId].thread_id, NULL, downloadImagesTask, &download_args[threadId]);
            threadId++;
        }
        
        s = pthread_attr_destroy(&attr);
        
        // free attribute and wait for the other threads
        for (int tnum = 0; tnum < nCount; tnum++) {
            s = pthread_join(download_args[tnum].thread_id, &res);
            //free(res);      /* Free memory allocated by thread */
        }
        
        char exec_rm_ln[180];
        sprintf(exec_rm_ln,"sudo rm /home/pi/cam-new/static/cam_images");
        system(exec_rm_ln);
        
        char exec_ln[180];
        sprintf(exec_ln,"sudo ln -s -f /cam/%s /home/pi/cam-new/static/cam_images", std::to_string(ms).c_str());
        system(exec_ln);
        
        //For the dashboard to know when to refresh images
        printf("OK\n");
        
        threadId = 0;
        
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        
        for(const char* cameraDeviceHostName : cameraDeviceHostNames)
        {
            cleanup_args[threadId].thread_num = threadId + 1;
            cleanup_args[threadId].deviceHostName = cameraDeviceHostName;
            
            pthread_create(&cleanup_args[threadId].thread_id, NULL, cleanupTask, &cleanup_args[threadId]);
            threadId++;
        }

        s = pthread_attr_destroy(&attr);
        
        // free attribute and wait for the other threads
        for (int tnum = 0; tnum < nCount; tnum++) {
            s = pthread_join(cleanup_args[tnum].thread_id, &res);
            //free(res);      /* Free memory allocated by thread */
        }

        free(args);
        free(download_args);
        free(cleanup_args);
    }
    		
    
    gps_stream(&gps_data, WATCH_DISABLE, NULL);
    gps_close (&gps_data);
    return 0;
}