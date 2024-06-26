
#include "config.h"
#include "dht22.h"
#include "ds18b20.h"
#include "espmissingincludes.h"
#include "ets_sys.h"
#include "gpio.h"
#include "io.h"
#include "lwip/sntp.h"
#include "mqtt.h"
#include "os_type.h"
#include "osapi.h"
#include "stdout.h"
#include "time_utils.h"

#include "thermostat.h"

#include <stdlib.h>

time_t thermostatRelayOffTime = 0;
int scheduleThermSetPoint = -9999;

static int ICACHE_FLASH_ATTR wd(int year, int month, int day) {
  size_t JND = day + ((153 * (month + 12 * ((14 - month) / 12) - 3) + 2) / 5) +
               (365 * (year + 4800 - ((14 - month) / 12))) + ((year + 4800 - ((14 - month) / 12)) / 4) -
               ((year + 4800 - ((14 - month) / 12)) / 100) + ((year + 4800 - ((14 - month) / 12)) / 400) - 32045;
  return (int)JND % 7;
}

int ICACHE_FLASH_ATTR getRoomTemp() {
  // return the roomTemp for the configured thermostat sensor

  int roomTemp = -9999;

  // report the correct room temp depending on which sensor
  // is assigned to thermostat
  if (sysCfg.sensor_dht22_enable && (sysCfg.thermostat1_input == 1 || sysCfg.thermostat1_input == 2)) {
    struct sensor_reading *result = readDHT();
    if (result->success) {

      if (sysCfg.thermostat1_input == 2) { // Humidistat
        roomTemp = result->humidity * 100;
      } else {
        roomTemp = (int)(result->temperature * 100);
      }
    } else {
      roomTemp = -9999;
    }
  } else if (sysCfg.sensor_ds18b20_enable && sysCfg.thermostat1_input == 0) {
    struct sensor_reading *result = read_ds18b20();
    if (result->success) {
      int SignBit, Whole, Fract;
      roomTemp = result->temperature;

      SignBit = roomTemp & 0x8000;          // test most sig bit
      if (SignBit)                          // negative
        roomTemp = (roomTemp ^ 0xffff) + 1; // 2's comp

      Whole = roomTemp >> 4; // separate off the whole and fractional portions
      Fract = (roomTemp & 0xf) * 10 / 16;

      if (SignBit) // negative
        Whole *= -1;

      roomTemp = Whole * 10 + Fract;
    } else {
      roomTemp = -9999;
    }
  }

  else if (sysCfg.thermostat1_input == 3) { // Mqtt reading should be degC *10
    if (sntp_get_current_timestamp() - mqttTreadingTS > sysCfg.mqtt_temp_timeout_secs) {
      // mqttTreading too old, invalidate it by setting to -9999
      os_printf("Thermostat: MQTT temperature reading stale (older than %d minutes)\n",
                sysCfg.mqtt_temp_timeout_secs / 60);
      mqttTreading = -9999;
    } else {
      roomTemp = mqttTreading; // Treading is tenth of a degree, eg 24.5 = 245
    }
  } else if (sysCfg.thermostat1_input == 4) { // Serial reading should be degC *10
    roomTemp = serialTreading * 10;
  } else if (sysCfg.thermostat1_input == 5) { // Fixed value 10C
    roomTemp = 100;
  }

  return roomTemp;
}

void ICACHE_FLASH_ATTR thermostat(int current_t, int setpoint) {

  if (current_t < setpoint - sysCfg.thermostat1hysteresislow) {
    os_printf("Thermostat: Current temperature (%d) is below setpoint.\n", current_t);
    if (sysCfg.thermostat1opmode == THERMOSTAT_HEATING)
      thermostatRelayOn();
    else
      thermostatRelayOff();
  } else if (current_t > setpoint + sysCfg.thermostat1hysteresishigh) {
    os_printf("Thermostat: Current temperature (%d) is above setpoint.\n", current_t);
    if (sysCfg.thermostat1opmode == THERMOSTAT_HEATING)
      thermostatRelayOff();
    else
      thermostatRelayOn();
  }
}

void ICACHE_FLASH_ATTR thermostatRelayOn() {
  // to avoid relay cycling only turn the relay on
  // after it has been off for sysConfig.therm_relay_rest_min
  if (sntp_get_current_timestamp() > thermostatRelayOffTime + (sysCfg.therm_relay_rest_min * 60)) {

    if (sysCfg.relay_1_thermostat == 1) {
      currGPIO12State = 1;
      ioGPIO(currGPIO12State, 12);
    }
    if (sysCfg.relay_2_thermostat == 1) {
      currGPIO13State = 1;
      ioGPIO(currGPIO12State, 13);
    }
    if (sysCfg.relay_3_thermostat == 1) {
      currGPIO15State = 1;
      ioGPIO(currGPIO15State, 15);
    }
  } else {
    os_printf("Thermostat: Attempt to turn thermostat relay on during rest period, ignored\n");
  }
}

void ICACHE_FLASH_ATTR thermostatRelayOff() {

  if (sysCfg.relay_1_thermostat == 1 && currGPIO12State != 0) {
    currGPIO12State = 0;
    ioGPIO(currGPIO12State, 12);
    thermostatRelayOffTime = sntp_get_current_timestamp();
  }
  if (sysCfg.relay_2_thermostat == 1 && currGPIO13State != 0) {
    currGPIO13State = 0;
    ioGPIO(currGPIO12State, 13);
    thermostatRelayOffTime = sntp_get_current_timestamp();
  }
  if (sysCfg.relay_3_thermostat == 1 && currGPIO15State != 0) {
    currGPIO15State = 0;
    ioGPIO(currGPIO15State, 15);
    thermostatRelayOffTime = sntp_get_current_timestamp();
  }
}

static void ICACHE_FLASH_ATTR pollThermostatCb(void *arg) {
  unsigned long epoch = sntp_get_current_timestamp();
  int year = get_year(&epoch);
  int month = get_month(&epoch, year);
  int day = day = 1 + (epoch / 86400);
  int dow = wd(year, month, day);
  epoch = epoch % 86400;
  unsigned int hour = epoch / 3600;
  epoch %= 3600;
  unsigned int min = epoch / 60;
  int minadj = (min * 100 / 60);
  int currtime = hour * 100 + minadj;

  if (sysCfg.thermostat1state == 0) {
    os_printf("Thermostat: Not enabled.\n");
    // turn relays associated with the thermostat off
    thermostatRelayOff();
    return;
  }

  int Treading = getRoomTemp();

  // Update the thermostat setpoint if in AUTO mode as it is reported via MQTT
  // Failing to do this here means it defaults to -9999 in auto mode if Treading is invalid.
  if (sysCfg.thermostat1mode == THERMOSTAT_AUTO) {
    for (int sched = 0; sched < 8 && sysCfg.thermostat1schedule.weekSched[dow].daySched[sched].active == 1; sched++) {
      if (currtime >= sysCfg.thermostat1schedule.weekSched[dow].daySched[sched].start &&
          currtime < sysCfg.thermostat1schedule.weekSched[dow].daySched[sched].end) {

        scheduleThermSetPoint = sysCfg.thermostat1schedule.weekSched[dow].daySched[sched].setpoint;
        os_printf("Thermostat: Current schedule (%d) setpoint is: %d\n", sched,
                  sysCfg.thermostat1schedule.weekSched[dow].daySched[sched].setpoint);
      }
    }
  }

  if (Treading == -9999 || Treading > 400 || Treading < -200) { // Check for valid reading
    // if reading is > 40C, or < -3C or -9999 (invalid read) treat as invalid
    os_printf("Thermostat: Invalid temperature reading (%d is not in range -20C to +40C) turning off relay.\n",
              (int)Treading);
    // turn off - do not act on bad data !
    thermostatRelayOff();
    return;
  }

  if (sysCfg.thermostat1mode == THERMOSTAT_MANUAL) {
    thermostat(Treading, (int)sysCfg.thermostat1manualsetpoint);
    return;
  } else if (sysCfg.thermostat1mode == THERMOSTAT_AUTO) {
    if (year < 2022) {
      // Something is wrong with the NTP time, maybe not enabled?
      os_printf("Thermostat: NTP time seems incorrect - year is < 2022. \n");
      return;
    } else {
      thermostat(Treading, (int)scheduleThermSetPoint);
    }
  } else {
    os_printf("Thermostat: Unknown Thermostat mode. No action.");
    return;
  }
}

void ICACHE_FLASH_ATTR thermostat_init(uint32_t polltime) {

  os_printf("Thermostat: init; poll interval of %d sec\n", (int)polltime / 1000);

  static ETSTimer thermostatTimer;
  os_timer_setfn(&thermostatTimer, pollThermostatCb, NULL);
  os_timer_arm(&thermostatTimer, polltime, 1);
}
