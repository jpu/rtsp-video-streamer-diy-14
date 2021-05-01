/****************************************************************************************************************************************************
 *  TITLE: HOW TO BUILD A $9 RSTP VIDEO STREAMER: Using The ESP-32 CAM Board || Arduino IDE - DIY #14
 *  DESCRIPTION: This sketch creates a video streamer than uses RTSP. You can configure it to either connect to an existing WiFi network or to create
 *  a new access point that you can connect to, in order to stream the video feed.
 *
 *  By Frenoy Osburn
 *  YouTube Video: https://youtu.be/1xZ-0UGiUsY
 *  BnBe Post: https://www.bitsnblobs.com/rtsp-video-streamer---esp32
 ****************************************************************************************************************************************************/

  /********************************************************************************************************************
 *  Board Settings:
 *  Board: "ESP32 Wrover Module"
 *  Upload Speed: "921600"
 *  Flash Frequency: "80MHz"
 *  Flash Mode: "QIO"
 *  Partition Scheme: "Hue APP (3MB No OTA/1MB SPIFFS)"
 *  Core Debug Level: "None"
 *  COM Port: Depends *On Your System*
 *********************************************************************************************************************/
 
#include "src/OV2640.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

#include "src/SimStreamer.h"
#include "src/OV2640Streamer.h"
#include "src/CRtspSession.h"

const char *ssid =     "AP";         // Put your SSID here
const char *password = "12345";      // Put your PASSWORD here

//#define ENABLE_WEBSERVER
#define ENABLE_RTSPSERVER
#define ENABLE_MQTT
#define ENABLE_OTA

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#define CAM_FRAMESIZE FRAMESIZE_VGA
#define CAM_QUALITY 12
#define CAM_FRAMERATE 10

#include "camera_pins.h"

//Arduino OTA Config
#define OTA_NAME "ESP32CAM"
#define OTA_PORT 8266 //default
#define OTA_PASS NULL

//MQTT Config
#define MQTT_SERVER "homeassistant.localdomain"
#define MQTT_PORT 1883
#define MQTT_USER "mqtt"
#define MQTT_PASS "mqtt"
#define MQTT_CLIENT OTA_NAME
#define MQTT_TOPIC "home/esp32cam/led"

#ifdef ENABLE_OTA
#include <ArduinoOTA.h>
#endif

#ifdef ENABLE_MQTT
#include <PubSubClient.h>
WiFiClient esp_client;
PubSubClient mqtt_client(esp_client);
#endif

#ifdef ENABLE_WEBSERVER
WebServer server(80);
#endif

#ifdef ENABLE_RTSPSERVER
WiFiServer rtspServer(8554);
#endif

OV2640 cam;
CStreamer *streamer;
CRtspSession *session;
WiFiClient rtsp_client;

#ifdef ENABLE_MQTT
void mqtt_client_callback(char* topic, uint8_t* payload, unsigned int length)
{
  char payload_string[8];
  char payload_string_len = length;
  int intensity = 0;

  //constrain the length of the payload
  if (payload_string_len >= sizeof(payload_string))
  {
    payload_string_len = sizeof(payload_string) - 1;
  }

  //convert to a C-string, and then convert to a number
  memset(payload_string, 0, sizeof(payload_string));
  memcpy(payload_string, payload, payload_string_len);
  intensity = atoi((char*)payload_string);

  //TODO set LED
  #if defined(CAMERA_MODEL_AI_THINKER)
    digitalWrite(4, intensity);
  #endif
}

void mqtt_reconnect()
{
  if (mqtt_client.connected() == false)
  {
    Serial.print("MQTT Connecting...");
    do
    {
      if (mqtt_client.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS) == true)
      {
        Serial.println(" Connected!");
        mqtt_client.subscribe(MQTT_TOPIC);
      }
      else
      {
        Serial.print(".");
        delay(1000);
      }
    } while (mqtt_client.connected() == false);
  }
}
#endif

#ifdef ENABLE_WEBSERVER
void handle_jpg_stream(void)
{
    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response);

    while (1)
    {
        cam.run();
        if (!client.connected())
            break;
        response = "--frame\r\n";
        response += "Content-Type: image/jpeg\r\n\r\n";
        server.sendContent(response);

        client.write((char *)cam.getfb(), cam.getSize());
        server.sendContent("\r\n");
        if (!client.connected())
            break;
    }
}

void handle_jpg(void)
{
    WiFiClient client = server.client();

    cam.run();
    if (!client.connected())
    {
        return;
    }
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-disposition: inline; filename=capture.jpg\r\n";
    response += "Content-type: image/jpeg\r\n\r\n";
    server.sendContent(response);
    client.write((char *)cam.getfb(), cam.getSize());
}

void handleNotFound()
{
    String message = "Server is running!\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    server.send(200, "text/plain", message);
}
#endif

void setup()
{
    Serial.begin(115200);

    //setup camera LED
    #if defined(CAMERA_MODEL_AI_THINKER)
      pinMode(4, OUTPUT);
    #endif

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
    config.frame_size = CAM_FRAMESIZE;
    config.jpeg_quality = CAM_QUALITY; 
    config.fb_count = 2;       
  
    #if defined(CAMERA_MODEL_ESP_EYE)
      pinMode(13, INPUT_PULLUP);
      pinMode(14, INPUT_PULLUP);
    #endif
  
    cam.init(config);
    
    IPAddress ip;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(F("."));
    }
    ip = WiFi.localIP();
    Serial.println(F("WiFi connected"));
    Serial.println("");
    Serial.println(ip);
    Serial.print("Stream Link: rtsp://");
    Serial.print(ip);
    Serial.println(":8554/mjpeg/1");

#ifdef ENABLE_WEBSERVER
    server.on("/", HTTP_GET, handle_jpg_stream);
    server.on("/jpg", HTTP_GET, handle_jpg);
    server.onNotFound(handleNotFound);
    server.begin();
#endif

#ifdef ENABLE_MQTT
    //Setup MQTT
    mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt_client.setCallback(mqtt_client_callback);
    mqtt_reconnect();
#endif

#ifdef ENABLE_OTA
    //Setup OTA
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(OTA_NAME);
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.begin();
#endif

#ifdef ENABLE_RTSPSERVER
    rtspServer.begin();
#endif
}

void loop()
{
#ifdef ENABLE_WEBSERVER
    server.handleClient();
#endif

#ifdef ENABLE_OTA
    ArduinoOTA.handle();
#endif

#ifdef ENABLE_MQTT
    mqtt_reconnect();
    mqtt_client.loop();
#endif

#ifdef ENABLE_RTSPSERVER
    uint32_t msecPerFrame = (1000 / CAM_FRAMERATE);
    static uint32_t lastimage = millis();

    // If we have an active client connection, just service that until gone
    if(session) {
        session->handleRequests(0); // we don't use a timeout here,
        // instead we send only if we have new enough frames

        uint32_t now = millis();
        if((now > (lastimage + msecPerFrame)) || (now < lastimage)) { // handle clock rollover
            session->broadcastCurrentFrame(now);
            lastimage = now;

            // check if we are overrunning our max frame rate
            now = millis();
            if(now > (lastimage + msecPerFrame))
                printf("warning exceeding max frame rate of %d ms\n", now - lastimage);
        }

        if(session->m_stopped) {
            delete session;
            delete streamer;
            session = NULL;
            streamer = NULL;
        }
    }
    else {
        rtsp_client = rtspServer.accept();

        if(rtsp_client) {
            streamer = new OV2640Streamer(&rtsp_client, cam);             // our streamer for UDP/TCP based RTP transport

            session = new CRtspSession(&rtsp_client, streamer); // our threads RTSP session and state
        }
    }
#endif
}
