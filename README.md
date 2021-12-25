This is an improved version of the ESP32-CAM software provided by the original vendor.

This version adds two things:
1) a web configurator for connecting WiFi to a user's Access Point.
2) An email client that can send periodic snapshots to an email address of your choice
(also configurable via web.)

The user must copy the file camera_email_config_proto.h to camera_email_config.h and
modify the entries according to their needs.

Configuration of the web page is by inclusion of a pre-gzipped image into the camera_index.h file.
To make things slightly easier, I've included the 'source' for the index.h file for the
index_ov2640.html (the camera I'm using) into the 'data' folder.  You may edit this file
and then use the dumphex.py python command-line tool to dump out the gzipped version
plus converting it to the format used in the camera_index.h file.  You will need to
manually include this .txt file into the camera_image.h file at the appropriate
location (an automatic means will happen later :-)
