#!/usr/bin/env bash
echo Starting download preparation...

rm -rf /cam/stuff.tar.gz
tar -cvzf /cam/stuff.tar.gz /cam/
sudo ln -s -f /cam/stuff.tar.gz /home/pi/cam-new/static/stuff.tar.gz

echo Finished download preparation...
echo OK