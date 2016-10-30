#include <fstream>
#include <cstdlib>
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "opencv2/videoio.hpp"
using namespace std;


void makeVideos()
{
	cv::VideoCapture in_captureA("./%04d_panorama.png");
	cv::Mat imgA;
	//cv::VideoWriter out_captureA("./cam_cv_video_mp4.mp4", CV_FOURCC('X', '2', '6', '4'), 10, cv::Size(7168, 753));
	cv::VideoWriter out_captureA("./cam_cv_video.mp4", 0x21, 10, cv::Size(7168, 753));

	while (true)
	{
		in_captureA >> imgA;
		if (imgA.empty())
			break;

		out_captureA.write(imgA);
	}
}


int main(int argc, char *argv[]) {
	//make videos 
	makeVideos();
	return 0;
}

