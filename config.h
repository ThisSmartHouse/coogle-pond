#ifndef COOGLEPOND_H
#define COOGLEPOND_H

#define NUM_SWITCHES 4
#define SWITCH_TOPIC_ID "pond-power"
#define SWITCH_BASE_TOPIC ""
#define TEMP_BASE_TOPIC "/status/tank/pond"

#define ONEWIRE_PIN 2

#define ONEWIRE_REPORT_INTERVAL 5

// List of pins, in order, that we will be switching on/off
static int switches[NUM_SWITCHES] = {
	14, 12, 13, 16
};

#endif
