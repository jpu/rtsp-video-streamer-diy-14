# esp32cam

A PlatformIO project to convert a ESP32-CAM from AI Thinker into a RTSP MJPEG server.


The stream is at `rtsp://<IP>:8554/mjpeg/1`
or this should work: `rtsp://<DEVICE_NAME>.localdomain:8554/mjpeg/1`


Control the led from topic `home/<DEVICE_NAME>/led`
Where the payload is 0-255 for intensity.

