// Include ESP Mail Client library (this library)
#include <ESP_Mail_Client.h>
#include "esp_camera.h"
#include "Preferences.h"
extern "C" {
#include "crypto/base64.h"
}

#undef CALLBACK_USED

// Define the SMTP Session object which used for SMTP transsport
static SMTPSession smtp;

// Define the session config data which used to store the TCP session configuration
static ESP_Mail_Session session;

static String email_recipient;
static String email_username;
static String email_password;
static String email_server;
static bool picture_init = false;

#ifdef CALLBACK_USED
/* Callback function to get the Email sending status */
static void smtpCallback(SMTP_Status status) {
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()){
    Serial.println("----------------");
    Serial.printf("Message sent success: %d\r\n", status.completedCount());
    Serial.printf("Message sent failled: %d\r\n", status.failedCount());
    Serial.println("----------------");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++){
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      Serial.printf("Message No: %d\r\n", i + 1);
      Serial.printf("Status: %s\r\n", result.completed ? "success" : "failed");
      Serial.printf("Date/Time: %d/%d/%d %d:%d:%d\r\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      Serial.printf("Recipient: %s\r\n", result.recipients);
      Serial.printf("Subject: %s\r\n", result.subject);
    }
    Serial.println("----------------");
  }
}
#endif /* CALLBACK_USED */

void send_picture(void) {
    if (! picture_init) {
        Preferences preferences;

        Serial.println("send_picture: initializing...");

        preferences.begin("camera-prefs", false);
        email_recipient = preferences.getString("email_recipient");  // Recipient email address
        email_username  = preferences.getString("email_username");   // Username on host to send email
        email_password  = preferences.getString("email_password");   // Password for user to send email
        email_server    = preferences.getString("email_server");     // Host to use to send email
        preferences.end();

//      Serial.printf("email_recipient: '%s'\r\n", email_recipient.c_str());
//      Serial.printf("email_username:  '%s'\r\n", email_username.c_str());
//      Serial.printf("email_password:  '%s'\r\n", email_password.c_str());
//      Serial.printf("email_server:    '%s'\r\n", email_server.c_str());
        
        session.server.host_name = email_server.c_str();
        session.server.port = 587;
        session.login.email = email_username.c_str();
        session.login.password = email_password.c_str();
        session.login.user_domain = email_server.c_str();

        // Set the NTP config time
        session.time.ntp_server = "pool.ntp.org,time.nist.gov";
        session.time.gmt_offset = 7;
        session.time.day_light_offset = 0;

//      smtp.debug(1);

#ifdef CALLBACK_USED
        // Set the callback function to get the sending results */
        smtp.callback(smtpCallback);
#endif

        picture_init = true;
    }
    
    if (email_recipient.length() != 0 && email_username.length() != 0 && email_password.length() != 0 && email_server.length() != 0) {
        camera_fb_t* image = esp_camera_fb_get();
    
        if (image == NULL) {
            Serial.println("send_picture: no image captured");
        } else {
            extern const char* getHostName(void);
            
            Serial.printf("send_picture: sending %d X %d image (%d bytes)\r\n", image->width, image->height, image->len);
            
            SMTP_Message message;
            message.sender.name = getHostName();
            message.sender.email = email_recipient.c_str();
            message.subject = "A Photo";
            message.addRecipient("", email_recipient.c_str());
            message.text.content = "Picture";

            // Encode base64
            unsigned int encoded_length;
            unsigned char * encoded = base64_encode((const unsigned char *) image->buf, image->len, &encoded_length);

            // Release the image
            esp_camera_fb_return(image);

            Serial.printf("Encoded picture is %d bytes\r\n", encoded_length);
            
            SMTP_Attachment attached;
            
            attached.descr.filename = "critter.png";
            attached.descr.mime = "image/png";
            attached.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;
            attached.descr.content_encoding = Content_Transfer_Encoding::enc_base64;
          
            attached.blob.data = encoded;
            attached.blob.size = encoded_length;

            message.addAttachment(attached);
          

            if (!smtp.connect(&session)) {
                Serial.printf("Unable to SMTP connect: %s\r\n", smtp.errorReason());
            } else if (!MailClient.sendMail(&smtp, &message, true)) {
                Serial.printf("Error sending Email: %s\r\n", smtp.errorReason());
            } else {
                Serial.println("send_picture: sent"); 
            }

            free(encoded);
        }
    }
}
