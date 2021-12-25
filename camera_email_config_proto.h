/*
 * Define camera email config prototype
 */
#ifndef __camera_email_config_h_defined
#define __camera_email_config_h_defined

#define DEFAULT_EMAIL_SERVER    "smtp.example.conf"
#define DEFAULT_EMAIL_USER      "user-allowed-to-send-email"
#define DEFAULT_EMAIL_PASSWORD  "password-for-user-allowed-to-send-email"
#define DEFAULT_EMAIL_RECIPIENT "default-recipient-email-name-leave-blank-if-no-default"
#define DEFAULT_EMAIL_PERIOD    15                     // Period in minutes to automatic send of picture
#define DEFAULT_EMAIL_FLASHLED  false                  // true to default flash LED for picture taken. 

#endif /* __camera_email_config_h_defined */

