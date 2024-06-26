/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */

#define SNTP_SERVER_DNS 1
//#define SNTP_SERVER_ADDRESS "pool.ntp.org"
#define SNTP_SERVER_ADDRESS "213.161.194.93"
#define EAGLE_FLASH_BIN_ADDR (SYSTEM_PARTITION_CUSTOMER_BEGIN + 1)
#define EAGLE_IROM0TEXT_BIN_ADDR (SYSTEM_PARTITION_CUSTOMER_BEGIN + 2)

#include "espmissingincludes.h"
#include "user_interface.h"

#include "auth.h"
#include "broadcastd.h"
#include "cgi.h"
#include "cgithermostat.h"
#include "cgiwifi.h"
#include "config.h"
#include "dht22.h"
#include "ds18b20.h"
#include "ets_sys.h"
#include "httpclient.h"
#include "httpd.h"
#include "httpdespfs.h"
#include "io.h"
#include "lwip/sntp.h"
#include "lwip/timers.h"
#include "mqtt.h"
#include "stdout.h"
//#include "thermostat.h"
#include "time_utils.h"
#include "user_interface.h"
#include "wifi.h"

//#include "netbios.h"
//#include "pwm.h"
//#include "cgipwm.h"

//#include "oled.h"

MQTT_Client mqttClient;

// Function that tells the authentication system what users/passwords live on the system.
// This is disabled in the default build; if you want to try it, enable the authBasic line in
// the builtInUrls below.
int ICACHE_FLASH_ATTR myPassFn(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) {
  if (no == 0) {
    os_strcpy(user, (char *)sysCfg.httpd_user);
    os_strcpy(pass, (char *)sysCfg.httpd_pass);
    return 1;
    // Add more users this way. Check against incrementing no for each user added.
    //	} else if (no==1) {
    //		os_strcpy(user, "user1");
    //		os_strcpy(pass, "something");
    //		return 1;
  }
  return 0;
}

/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[] = {{"/", cgiRedirect, "/index.tpl"},
                                 {"/index.tpl", cgiEspFsTemplate, tplCounter},
                                 {"/about.tpl", cgiEspFsTemplate, tplCounter},

                                 //{"/flash.bin", cgiReadFlash, NULL},

                                 {"/config/*", authBasic, myPassFn},
                                 {"/control/*", authBasic, myPassFn},

                                 {"/control/ui.tpl", cgiEspFsTemplate, tplUI},
                                 {"/control/relay.tpl", cgiEspFsTemplate, tplGPIO},
                                 {"/control/relay.cgi", cgiGPIO, NULL},
                                 {"/control/dht22.tpl", cgiEspFsTemplate, tplDHT},
                                 {"/control/dht22.cgi", cgiDHT22, NULL},
                                 {"/control/ds18b20.tpl", cgiEspFsTemplate, tplDS18b20},
                                 {"/control/ds18b20.cgi", cgiDS18b20, NULL},
                                 {"/control/state.cgi", cgiState, NULL},
                                 {"/control/reset.cgi", cgiReset, NULL},
                                 {"/control/thermostat.tpl", cgiEspFsTemplate, tplThermostat},
                                 {"/control/thermostat.cgi", cgiThermostat, NULL},
#ifdef CGIPWM_H
                                 {"/control/pwm.cgi", cgiPWM, NULL},
#endif
                                 {"/config/wifi", cgiRedirect, "/config/wifi/wifi.tpl"},
                                 {"/config/wifi/", cgiRedirect, "/config/wifi/wifi.tpl"},
                                 {"/config/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
                                 {"/config/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
                                 {"/config/wifi/connect.cgi", cgiWiFiConnect, NULL},
                                 {"/config/wifi/setmode.cgi", cgiWiFiSetMode, NULL},
                                 {"/config/wifi/connstatus.cgi", cgiWiFiConnStatus, NULL},
                                 {"/config/mqtt.tpl", cgiEspFsTemplate, tplMQTT},
                                 {"/config/mqtt.cgi", cgiMQTT, NULL},
                                 {"/config/httpd.tpl", cgiEspFsTemplate, tplHTTPD},
                                 {"/config/httpd.cgi", cgiHTTPD, NULL},
                                 {"/config/broadcastd.tpl", cgiEspFsTemplate, tplBroadcastD},
                                 {"/config/broadcastd.cgi", cgiBroadcastD, NULL},
                                 {"/config/ntp.tpl", cgiEspFsTemplate, tplNTP},
                                 {"/config/ntp.cgi", cgiNTP, NULL},
                                 {"/config/relay.tpl", cgiEspFsTemplate, tplRLYSettings},
                                 {"/config/relay.cgi", cgiRLYSettings, NULL},
                                 {"/config/sensor.tpl", cgiEspFsTemplate, tplSensorSettings},
                                 {"/config/sensor.cgi", cgiSensorSettings, NULL},

                                 {"*", cgiEspFsHook, NULL}, // Catch-all cgi function for the filesystem
                                 {NULL, NULL, NULL}};

void ICACHE_FLASH_ATTR http_callback_IP(char *response, int http_status, char *full_response) {
  // os_printf("http_status=%d\n", http_status);
  if (http_status != HTTP_STATUS_GENERIC_ERROR) {
    // os_printf("strlen(full_response)=%d\n", strlen(full_response));
    // os_printf("response=%s<EOF>\n", response);
    os_printf("External IP address=%s\n", response);
  }
}

void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status) {
  if (status == STATION_GOT_IP) {

    os_printf("Trying to find external IP address\n");
    http_get("http://wtfismyip.com/text", http_callback_IP);

    if (sysCfg.mqtt_enable == 1) {
      MQTT_Connect(&mqttClient);
    } else {
      MQTT_Disconnect(&mqttClient);
    }
  }
}

void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args) {
  MQTT_Client *client = (MQTT_Client *)args;
  os_printf("MQTT: Connected\r\n");
  MQTT_Subscribe(client, (char *)sysCfg.mqtt_relay_subs_topic, 0);
  MQTT_Subscribe(client, (char *)sysCfg.mqtt_temp_subs_topic, 0);
}

void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args) {
  //	MQTT_Client* client = (MQTT_Client*)args;
  os_printf("MQTT: Disconnected\r\n");
}
void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char *topic, uint32_t topic_len, const char *data,
                                  uint32_t lengh) {

  char strTopic[topic_len + 1];
  os_memcpy(strTopic, topic, topic_len);
  strTopic[topic_len] = '\0';

  char strData[lengh + 1];
  os_memcpy(strData, data, lengh);
  strData[lengh] = '\0';

  char strTempSubsTopic[strlen((char *)sysCfg.mqtt_temp_subs_topic)];
  os_strcpy(strTempSubsTopic, (char *)sysCfg.mqtt_temp_subs_topic);

  // Did we get a temperature reading (tpoic: strTempSubsTopic )
  if (os_strcmp(strTempSubsTopic, strTopic) == 0) {
    os_printf("Received thermostat temperature of %s via MQTT\n", strData);
    mqttTreading = atoi(strData) * 10;
    char *fractPart;
    fractPart = strchr(strData, '.');

    // deal with the decimal point
    if (fractPart != NULL) {
      char *firstChar = "0";
      os_sprintf(firstChar, "%c", fractPart[1]);
      int fp = atoi(firstChar);
      mqttTreading += fp;
    }

    mqttTreadingTS = sntp_get_current_timestamp(); // timestamp of the reading

  } else {
    char relayNum = strTopic[topic_len - 1];
    char strSubsTopic[strlen((char *)sysCfg.mqtt_relay_subs_topic)];
    os_strcpy(strSubsTopic, (char *)sysCfg.mqtt_relay_subs_topic);
    strSubsTopic[(strlen((char *)sysCfg.mqtt_relay_subs_topic) - 1)] = relayNum;

    os_printf("MQTT strSubsTopic: %s, strTopic: %s \r\n", strSubsTopic, strTopic);

    if (os_strcmp(strSubsTopic, strTopic) == 0) {
      os_printf("Relay %d is now: %s \r\n", relayNum - '0', strData);

      if (relayNum == '1') {
        currGPIO12State = atoi(strData);
        ioGPIO(currGPIO12State, 12);
      }

      if (relayNum == '2') {
        currGPIO13State = atoi(strData);
        ioGPIO(currGPIO13State, 13);
      }

      if (relayNum == '3') {
        currGPIO15State = atoi(strData);
        ioGPIO(currGPIO15State, 15);
      }

      if (sysCfg.relay_latching_enable) {
        sysCfg.relay_1_state = currGPIO12State;
        sysCfg.relay_2_state = currGPIO13State;
        sysCfg.relay_3_state = currGPIO15State;
        CFG_Save();
      }
    }
  }
  os_printf("MQTT topic: %s, data: %s \r\n", strTopic, strData);
}

void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args) {
  //    MQTT_Client* client = (MQTT_Client*)args;
  os_printf("MQTT: Published\r\n");
}

// Main routine
void ICACHE_FLASH_ATTR user_init(void) {

  stdoutInit();
  os_delay_us(65535);
  CFG_Load();

  ioInit();

  WIFI_Connect(wifiConnectCb);
  httpdInit(builtInUrls, sysCfg.httpd_port);

  if (sysCfg.ntp_enable == 1) {
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_timezone(sysCfg.ntp_tz);
    sntp_init();
  }

  if (sysCfg.mqtt_enable == 1) {
    MQTT_InitConnection(&mqttClient, (uint8_t *)sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.mqtt_use_ssl);
    MQTT_InitClient(&mqttClient, (uint8_t *)sysCfg.mqtt_devid, (uint8_t *)sysCfg.mqtt_user, (uint8_t *)sysCfg.mqtt_pass,
                    sysCfg.mqtt_keepalive, 1);
    MQTT_OnConnected(&mqttClient, mqttConnectedCb);
    MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
    MQTT_OnPublished(&mqttClient, mqttPublishedCb);
    MQTT_OnData(&mqttClient, mqttDataCb);
  }

  if (sysCfg.sensor_dht22_enable)
    DHTInit(SENSOR_DHT22, 30000);

  if (sysCfg.sensor_ds18b20_enable)
    ds_init(30000);

  broadcastd_init();

  thermostat_init(30000);

  /*
          //Netbios to set the name
          struct softap_config wiconfig;
          os_memset(netbios_name, ' ', sizeof(netbios_name)-1);
          if(wifi_softap_get_config(&wiconfig)) {
                  int i;
                  for(i = 0; i < sizeof(netbios_name)-1; i++) {
                          if(wiconfig.ssid[i] < ' ') break;
                          netbios_name[i] = wiconfig.ssid[i];
                  };
          }
          else os_sprintf(netbios_name, "ESP8266");
          netbios_name[sizeof(netbios_name)-1]='\0';
          netbios_init();
  */

  os_printf("Relay Board Ready\n");
  os_printf("Free heap size:%d\n", system_get_free_heap_size());
  os_printf("Firmware version %s\n", FWVER);
  os_printf("SDK version:%s\n", system_get_sdk_version());
#ifdef CGIPWM_H
  // Mind the PWM pin!! defined in pwm.h
  duty = 0;
  pwm_init(50, &duty);
  pwm_set_duty(duty, 0);
  pwm_start();
#endif

  // OLEDInit();
}
/*
#define SPI_FLASH_SIZE_MAP FLASH_SIZE_4M_MAP_256_256

static const partition_item_t partition_table[] = {

    {EAGLE_FLASH_BIN_ADDR, 0x00000, 0xB000},
    {EAGLE_IROM0TEXT_BIN_ADDR, 0x10000, 0x40000}, // try 40000
    {SYSTEM_PARTITION_RF_CAL, 0x7C000, 0x1000},
    {SYSTEM_PARTITION_PHY_DATA, 0x7A000, 0x1000},
    {SYSTEM_PARTITION_SYSTEM_PARAMETER, 0x7E000, 0x1000},
    // {SYSTEM_PARTITION_BOOTLOADER,0x0,0x1000},
    //{SYSTEM_PARTITION_OTA_1, 0x1000, SYSTEM_PARTITION_OTA_SIZE},
    //{SYSTEM_PARTITION_OTA_2, SYSTEM_PARTITION_OTA_2_ADDR, SYSTEM_PARTITION_OTA_SIZE},
};

void ICACHE_FLASH_ATTR user_pre_init(void) {
  if (!system_partition_table_regist(partition_table, sizeof(partition_table) / sizeof(partition_table[0]),
                                     SPI_FLASH_SIZE_MAP)) {
    os_printf("system_partition_table_regist fail\r\n");
    while (1)
      ;
  }
}
*/
uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void) {
  enum flash_size_map size_map = system_get_flash_size_map();
  uint32 rf_cal_sec = 0;

  switch (size_map) {
  case FLASH_SIZE_4M_MAP_256_256:
    rf_cal_sec = 128 - 5;
    break;

  case FLASH_SIZE_8M_MAP_512_512:
    rf_cal_sec = 256 - 5;
    break;

  case FLASH_SIZE_16M_MAP_512_512:
  case FLASH_SIZE_16M_MAP_1024_1024:
    rf_cal_sec = 512 - 5;
    break;

  case FLASH_SIZE_32M_MAP_512_512:
  case FLASH_SIZE_32M_MAP_1024_1024:
    rf_cal_sec = 1024 - 5;
    break;

  case FLASH_SIZE_64M_MAP_1024_1024:
    rf_cal_sec = 2048 - 5;
    break;
  case FLASH_SIZE_128M_MAP_1024_1024:
    rf_cal_sec = 4096 - 5;
    break;
  default:
    rf_cal_sec = 0;
    break;
  }
  return rf_cal_sec;
}

void ICACHE_FLASH_ATTR user_rf_pre_init(void) {
  // system_phy_freq_trace_enable(at_get_rf_auto_trace_from_flash());
  system_phy_freq_trace_enable(0);
}