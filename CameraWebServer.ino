#include "esp_camera.h"
#include <nvs_flash.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "send_picture.h"

#define WL_MAC_ADDR_LEN   15

char *host_name;

const char* getHostName(void) {
  return host_name;
}

#include "TinyUPnP.h"
#define LEASE_DURATION 36000      // seconds
#define FRIENDLY_NAME "Camera"

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM

#include "camera_pins.h"
#include "Preferences.h"

#define FACTORY_RESTART  14
#define LED1             33

#define MAX_FLASH_LED_RUNTIME 3600  /* One hour */

// Customize to your system
#include "camera_email_config.h"

#define PREFS_VERSION    1

Preferences preferences;

void startCameraServer(void);

static void resetToFactory(void) {
  Serial.println("Factory restart...");
  Preferences prefs;
  prefs.begin("camera-prefs", false);
  prefs.clear();
  prefs.end();
#ifdef FLASH_LED
  // Flash camera lamp four times
  for (int loops = 0; loops < 4; ++loops) {
    digitalWrite(FLASH_LED, true);
    delay(1000);
    digitalWrite(FLASH_LED, false);
    delay(1000);
  }
#endif
  ESP.restart();
}

#ifdef FACTORY_RESTART
static void testFactoryRestart(void) {
  if (!digitalRead(FACTORY_RESTART)) {
    Serial.println("testFactoryRestart: pin low");

    int counter = 10;
    
    while (counter != 0 and !digitalRead(FACTORY_RESTART)) {
      counter--;
      delay(1000);
      if (!digitalRead(FACTORY_RESTART)) {
        Serial.println("testFactoryRestart: still low...");
      }
      digitalWrite(LED1, !digitalRead(LED1));
    }

    if (counter == 0) {
      resetToFactory();
    }
  }
  
  digitalWrite(LED1, true); // Off state
}
#endif /* FACTORY_RESET */

static int flashLedOnTimer;
void flashLedPower(bool state) {
#ifdef FLASH_LED
  digitalWrite(FLASH_LED, state);
  if (state) {
    flashLedOnTimer = MAX_FLASH_LED_RUNTIME;
  } else {
    flashLedOnTimer = 0;
  }
#endif
}

bool flashLedState(void) {
#ifdef FLASH_LED
  return digitalRead(FLASH_LED);
#else
  return 0;
#endif
}

int picture_period;
int picture_timer;
int picture_flashled;

void setup() {  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

#ifdef FLASH_LED
  pinMode(FLASH_LED, OUTPUT);
  digitalWrite(FLASH_LED, 0);
#endif

#ifdef FACTORY_RESTART
  pinMode(LED1, OUTPUT);
  pinMode(FACTORY_RESTART, INPUT_PULLUP);
  testFactoryRestart();
#endif

  flashLedPower(false);

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\r\n", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  // Read MAC address
  uint8_t mac_address[6];
    
  WiFi.macAddress(mac_address);

  asprintf(&host_name, "camera-%02x%02x", mac_address[4], mac_address[5]);

  preferences.begin("camera-prefs", false);

  // Default connection mode
  String wifi_ssid = host_name;
  String wifi_password = "";
  String wifi_mode = "ap";

  // Probe to see if we have an definition
  int version = preferences.getInt("version", 0);
  if (version != PREFS_VERSION) {
    // Need to initialize preferences
    preferences.clear();
    
    Serial.printf("Wrong PREFS version: %d wanted %d -- resetting\r\n", version, PREFS_VERSION);
    preferences.putInt("version", PREFS_VERSION);
    preferences.putString("wifi_mode", wifi_mode);
    preferences.putString("wifi_ssid", wifi_ssid);
    preferences.putString("wifi_password", wifi_password);
    preferences.putString("email_username", DEFAULT_EMAIL_USER);
    preferences.putString("email_password", DEFAULT_EMAIL_PASSWORD);
    preferences.putString("email_server", DEFAULT_EMAIL_SERVER);
    preferences.putString("email_recipient", DEFAULT_EMAIL_RECIPIENT);
    preferences.putInt("email_period", DEFAULT_EMAIL_PERIOD);
    preferences.putInt("email_flashled", DEFAULT_EMAIL_FLASHLED);
  } else {
    Serial.printf("Found PREFS_VERSION %d\r\n", version);
    wifi_mode = preferences.getString("wifi_mode");
    wifi_ssid = preferences.getString("wifi_ssid");
    wifi_password = preferences.getString("wifi_password");
  }

  // Picture period is in minutes
  picture_period = preferences.getInt("email_period", 0) * 60;
  Serial.printf("picture_period set to %d\r\n", picture_period);
  picture_timer = 0;
  picture_flashled = preferences.getInt("email_flashled", false);

  // Set if WiFi connecting to AP has ever worked.
  int worked_once = preferences.getInt("wifi_worked", 0);
  preferences.end();

  IPAddress ip_address;
  
  if (strcmp(wifi_mode.c_str(), "network") == 0) {
    Serial.printf("WiFi start with STA mode '%s'\r\n", wifi_ssid.c_str());

    WiFi.setHostname(host_name);
    
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

    int timeout = worked_once ? 0 : 30;  // Only wait 30 seconds if this is the first time we've tried
    while ((timeout == 0 || --timeout != 0) && WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
      // If we get here, we're trying to connect for the first time.
      // If failed - reset to factory and try again
      resetToFactory();
    }

    // From now on, persist until it connects or user invokes factory restart
    preferences.begin("camera-prefs", false);
    preferences.putInt("wifi_worked", 1);
    preferences.end();
    
    ip_address = WiFi.localIP();
  } else {
    Serial.printf("WiFi start with in AP mode ssid '%s' and pw '%s'\r\n", wifi_ssid.c_str(), wifi_password.c_str());

    WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str());

    ip_address = WiFi.softAPIP();
  }

#ifdef FLASH_LED
  // Flash camera lamp twice
  for (int loops = 0; loops < 2; ++loops) {
    digitalWrite(FLASH_LED, true);
    delay(1000);
    digitalWrite(FLASH_LED, false);
    delay(1000);
  }
#endif

  Serial.println("");
  Serial.println("WiFi connected");


#ifdef NOTUSED
  // Enable mdns notification
  if(!MDNS.begin("camera")) {
    Serial.println("Error starting mDNS");
  } else {
    Serial.println("MDNS started");
  }
#endif

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(ip_address);
  Serial.println("' to connect");

#ifdef NOTUSED
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("video", "tcp", 81);
#endif

#ifdef NOTUSED
  // Add UPNP information
  // -1 for blocking (preferably, use a timeout value in [ms])
  TinyUPnP *tinyUPnP = new TinyUPnP(20000);
  
//  // you may repeat 'addPortMappingConfig' more than once
//  portMappingResult portMappingAdded;
  
  tinyUPnP->addPortMappingConfig(ip_address, 80, RULE_PROTOCOL_TCP, LEASE_DURATION, FRIENDLY_NAME);
  
//    while (portMappingAdded != SUCCESS && portMappingAdded != ALREADY_MAPPED) {
//      portMappingAdded = tinyUPnP->commitPortMappings();
//      Serial.println("");
//    
//      if (portMappingAdded != SUCCESS && portMappingAdded != ALREADY_MAPPED) {
//        // for debugging, you can see this in your router too under forwarding or UPnP
//        tinyUPnP->printAllPortMappings();
//        Serial.println(F("This was printed because adding the required port mapping failed"));
//        delay(30000);  // 30 seconds before trying again
//      }
//    }  
  // finally, commit the port mappings to the IGD
  bool portMappingAdded = tinyUPnP->commitPortMappings();
#endif
}



void loop() {
  
  while (true) {
    delay(1000);
  //Serial.printf("Loop: picture_period is %d\r\n", picture_period);
    if (flashLedOnTimer != 0) {
      flashLedOnTimer--;
      if (flashLedOnTimer <= 0) {
        flashLedPower(false);
      }
    }
    if (picture_period != 0) {
      // Serial.printf("Loop: picture_period is %d timer %d\r\n", picture_period, picture_timer);
      if (++picture_timer > picture_period) {
        bool turned_led_on = false;
        if (!flashLedOnTimer && picture_flashled) {
            flashLedPower(true);
            turned_led_on = true;
        }
        send_picture();
        if (turned_led_on) {
            flashLedPower(false);
        }
        picture_timer -= picture_period;
      }
    }
//  testFactoryRestart();
  }
}
