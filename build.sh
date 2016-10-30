#!/usr/bin/env bash
echo Starting build...

sudo mkdir /cam

while [[ $# > 0 ]]
do
key="$1"
echo $key
case $key in
    -b|--base)
    g++ base_unit.cpp -o base_unit -Wall -ljpeg -lv4l2 -lpthread -lm -lgps -DIO_MMAP -DIO_USERPTR -D__STDC_LIMIT_MACROS -std=c++0x -Wwrite-strings -lstdc++
    shift # past argument
    ;;
    -c|--cam)
    g++ cam.cpp capturer_mmap.cpp yuv.cpp -o cam -Wall -ljpeg -lv4l2 -lpthread -std=c++11
    shift # past argument
    ;;
    -v|--vid)
    echo Requires OpenCV!!!
    g++ stitch_video.cpp -o timelapse -lopencv_core -lopencv_videoio -lopencv_highgui
    shift # past argument
    ;;
esac
shift # past argument or value
done

echo Build completed successfully!