/*
  +----------------------------------------------------------------------+
  | CooglePond for ESP8266                                               |
  +----------------------------------------------------------------------+
  | Copyright (c) 2017 John Coggeshall                                   |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License");      |
  | you may not use this file except in compliance with the License. You |
  | may obtain a copy of the License at:                                 |
  |                                                                      |
  | http://www.apache.org/licenses/LICENSE-2.0                           |
  |                                                                      |
  | Unless required by applicable law or agreed to in writing, software  |
  | distributed under the License is distributed on an "AS IS" BASIS,    |
  | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or      |
  | implied. See the License for the specific language governing         |
  | permissions and limitations under the License.                       |
  +----------------------------------------------------------------------+
  | Authors: John Coggeshall <john@coggeshall.org>                       |
  +----------------------------------------------------------------------+
*/

#include <CoogleIOT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "config.h"

#ifndef ONEWIRE_PIN
#define ONEWIRE_PIN 2
#endif

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

#ifndef NUM_SWITCHES
#define NUM_SWITCHES 2
#endif

#ifndef SWITCH_TOPIC_ID
#define SWITCH_TOPIC_ID "coogle-switch"
#endif

#ifndef SWITCH_BASE_TOPIC
#define SWITCH_BASE_TOPIC ""
#endif

#ifndef TEMP_BASE_TOPIC
#define TEMP_BASE_TOPIC "/temperature"
#endif

#ifndef ONEWIRE_PIN
#define ONEWIRE_PIN 2
#endif

#ifndef ONEWIRE_REPORT_INTERVAL
#define ONEWIRE_REPORT_INTERVAL 5
#endif

#define SWITCH_SWITCH_BASE_TOPIC SWITCH_BASE_TOPIC "/" SWITCH_TOPIC_ID "/switch/"

CoogleIOT *iot;
PubSubClient *mqtt;
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature DS18820(&oneWire);

char msg[150];
int  onewireCount = 0;

void setup() {

  iot = new CoogleIOT(LED_BUILTIN);

  iot->enableSerial(115200);
  iot->initialize();
  DS18820.begin();

  mqtt = iot->getMQTTClient();

  iot->info("Coogle Pond Controller Initializing...");
  iot->info("-=-=-=-=--=--=-=-=-=-=-=-=-=-=-=-=-=-");
  iot->logPrintf(INFO, "Number of Switches: %d", NUM_SWITCHES);
  iot->logPrintf(INFO, "MQTT Topic ID: %s", SWITCH_TOPIC_ID);
  iot->logPrintf(INFO, "Base Topic: %s", SWITCH_SWITCH_BASE_TOPIC);
  iot->info("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
  iot->info("Switches");
  iot->info("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");

  for(int i = 0; i < sizeof(switches) / sizeof(int); i++) {
	pinMode(switches[i], OUTPUT);
	iot->logPrintf(INFO, "  Switch %d on pin %d", i+1, switches[i]);
  }

  if(iot->mqttActive()) {
	  for(int i = 0; i < sizeof(switches) / sizeof(int); i++) {
		  snprintf(msg, 150, SWITCH_SWITCH_BASE_TOPIC "%d/state", i+1);
		  mqtt->publish(msg, "0", true);

		  iot->logPrintf(INFO, "  Publishing to state topic '%s'", msg);

		  snprintf(msg, 150, SWITCH_SWITCH_BASE_TOPIC "%d", i+1);
		  mqtt->subscribe(msg);

		  iot->logPrintf(INFO, "  Subscribed to action topic '%s'", msg);
	  }
  }

  onewireCount = DS18820.getDeviceCount();

  iot->info("Temperature Sensor(s)");
  iot->info("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
  iot->logPrintf(INFO, "OneWire Sensor(s): %d Sensors", onewireCount);
  iot->logPrintf(INFO, "MQTT Base Topic: %s", TEMP_BASE_TOPIC);
  iot->logPrintf(INFO, "Update Interval: %d second(s)", ONEWIRE_REPORT_INTERVAL);
  iot->info("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");

  if(iot->mqttActive()) {
	  for(int i = 0; i < onewireCount; i++) {
		  snprintf(msg, 150, TEMP_BASE_TOPIC "/%d/temperature", i + 1);
		  mqtt->publish(msg, "{ \"temperature_c\" : 0 }", true);
		  iot->logPrintf(INFO, "  Publishing to state topic '%s'", msg);
	  }
	  iot->info("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
  }



  iot->info("");

  if(!iot->mqttActive()) {
	iot->error("Initialization failure, invalid MQTT Server connection.");
    return;
  }

  mqtt->setCallback(mqttCallback);

  iot->registerTimer(ONEWIRE_REPORT_INTERVAL * 1000, &readTemperatureCallback);

  iot->info("Coogle Pond Controller Initialized!");
}

void readTemperatureCallback()
{
	int retryCount;
	int measuredTemperature;
	char tempString[10];
	char topicString[150];
	char topicMessage[150];

	if(!iot->mqttActive()) {
		iot->error("MQTT not active, cannot publish temperatures.");
		return;
	}

	if(onewireCount <= 0) {
		iot->info("No temperature sensors available, cannot read temperature");
		return;
	}

	iot->info("Processing OneWire Sensors...");

	for(int i = 0; i < onewireCount; i++) {
		iot->logPrintf(INFO, "Reading temperature for sensor (idx: %d)", i);

		retryCount = 0;

		do {
			DS18820.requestTemperatures();
			measuredTemperature = DS18820.getTempCByIndex(i);
			retryCount++;
			yield();
		} while ( ((measuredTemperature > 84) || (measuredTemperature < -126)) && (retryCount <= 3));

		if(retryCount <= 3) {
			dtostrf(measuredTemperature, 6, 2, tempString);

			snprintf(topicMessage, 150, "{ \"temperature_c\" : %s }", tempString);
			snprintf(topicString, 150, TEMP_BASE_TOPIC "/%d/temperature", i + 1);

			mqtt->publish(topicString, msg);
		}

		yield();
	}

}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    unsigned int switchPin = 0;
    unsigned int switchId = 0;
    unsigned int switchState = 0;

    iot->logPrintf(DEBUG, "MQTT Callback Triggered. Topic: %s\n", topic);

    for(int i = 0; i < sizeof(switches) / sizeof(int); i++) {
    	  snprintf(msg, 150, SWITCH_SWITCH_BASE_TOPIC "%d", i + 1);

    	  if(strcmp(topic, msg) == 0) {
    		  switchPin = switches[i];
    		  switchId = i + 1;
    	  }
    }

    if(switchPin <= 0) {
      return;
    }

    iot->logPrintf(DEBUG, "Processing request for switch %d on pin %d\n", switchId, switchPin);

    if((char)payload[0] == '1') {
      digitalWrite(switchPin, HIGH);
      switchState = HIGH;
    } else if((char)payload[0] == '0') {
      digitalWrite(switchPin, LOW);
      switchState = LOW;
    }

    snprintf(msg, 150, SWITCH_SWITCH_BASE_TOPIC "%d/state", switchId);

    mqtt->publish(msg, switchState == HIGH ? "1" : "0", true);
}

void loop() {
	iot->loop();
}
