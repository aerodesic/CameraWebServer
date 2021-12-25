This is an improved version of the ESP32-CAM software provided by the original vendor.

This version adds two things:
1) a web configurator for connecting WiFi to a user's Access Point.
2) An email client that can send periodic snapshots to an email address of your choice
(also configurable via web.)

The user must copy the file camera_email_config_proto.h to camera_email_config.h and
modify the entries according to their needs.
