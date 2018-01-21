/*
 * Project EnergyMonitorPulse
 * Description: Calculates electricity use from a meter's pulse output
 * Author: Richard Lyon
 * Date: 10 Jan 2018
 *
 * power: (Watts) the instantaneous power demand
 * elapsedEnergy: (kWh) energy consumption since midnight
 * dailyCost: (pence) daily energy cost
 */

// Time between activating WiFi and connecting to Particle; trial and error
#define WIFI_SETTLING_TIME 1000
// Time between connecting to Particle and trying to publish anything
// Withouy any delay, device reverts to 'Listening' mode
#define PUBLISH_SETTLING_TIME 5 * 1000

// Energy contract cost (Pure Planet, 01 Jan 2018)
#define STANDING_CHARGE 32.877; // (pence / day) energy contract standing charge
#define UNIT_COST 11.193; // (pence / kWh) energy contract unit cost

// InfluxDB configuration
#include "Influxdb.h"
Influxdb idb = Influxdb(USERNAME, PASSWORD);

// Pin assignments
int interruptPin = D3;
int ledPin = D7;

// Main variables
double power; // (Watts) current power consumption
double elapsedEnergy; // (kWh) energy consumption since midnight
double dailyCost; // (pence) daily energy cost

// Power intermediate variables
int ppwh = 1; // (pulse per Wh)
long pulseCount = 0; // keeps track of the number of pulses detected
unsigned long pulseTime, lastTime, pulseInterval; // milliseconds
double standingCharge = STANDING_CHARGE; // (pence) energy contract standing charge
double unitCost = UNIT_COST; // (pence / kWh) energy contract unit cost (pence / kWh)

// WiFi cycle control variables
int cycleDurationMillis;
long lastPulseTimeMillis;
long connectStartMillis;

// Midnight calculation variables
int thisday, lastday;

// Led pulse variable
int state = 0; // used to toggle led

// Interrupt control flag
bool pulseFlag = false;

// Particle Functions
int resetDevice(String command);
int setCycleDuration(String duration);

void setup() {
  Serial.begin(9600);

  // Configure pins
  pinMode(ledPin, OUTPUT);
  pinMode(interruptPin, INPUT_PULLDOWN);
  attachInterrupt(interruptPin, flag_pulse, RISING);

  // Set the device name in InfluxDB
  idb.setDeviceName("elmon");

  // Initialise pulseTime;
  pulseTime = millis();

  // Initialise WiFi cycle duration
  setCycleDuration("30000");
  lastPulseTimeMillis = millis();

  // Initialise variables to detect midnight
  lastday = Time.weekday();
  Particle.publish("INFO", String::format("Monitor started with cycleduraton %dms", cycleDurationMillis), PRIVATE);
}

void loop() {

  // Interrupt handler for pulse; calculate energy and power
  if (pulseFlag == true) {
    calculate_energy();
    toggle_led();
    pulseFlag = false;
  }

  // WiFi cycle handler
  if ( (millis() - lastPulseTimeMillis) > cycleDurationMillis) {
    connectStartMillis = millis();

    WiFi.on();
    delay(WIFI_SETTLING_TIME); // allow wifi to connect

    Particle.connect();
    delay(PUBLISH_SETTLING_TIME); // to allow publish events

    // Set InfluxDB variables
    idb.add("power", power);
    idb.add("elapsedEnergy", elapsedEnergy);
    idb.add("dailyCost", dailyCost);
    if(idb.sendAll()){
      Serial.println("> InfluxDB updated");
    };

    WiFi.off();

    lastPulseTimeMillis = millis();
  }
}

// Interrupt handler. Called on the rising edge of a flash.
void flag_pulse(){
  pulseFlag = true;
}

void calculate_energy() {
  // Estimate the time in milliseconds since the last pulse (pulseInterval)
  lastTime = pulseTime;
  pulseTime = millis();
  pulseInterval = pulseTime - lastTime;

  pulseCount++;

  // calculate power
  power = (3600000.0 / pulseInterval)/ppwh;

  // calculate energy used since midnight
  if (Time.weekday() != lastday) {
    elapsedEnergy == 0;
    lastday = Time.weekday();
  } else {
    elapsedEnergy = (1.0*pulseCount/(ppwh*1000)); //multiply by 1000 to convert pulses per wh to kwh
  }

  // calculate cost since midnight
  dailyCost = STANDING_CHARGE + elapsedEnergy*UNIT_COST;
}

// toggle onboard LED to represent received pulses
void toggle_led() {
  digitalWrite(ledPin, (state) ? HIGH : LOW);
  state = !state;
}

int setCycleDuration(String duration) {
  cycleDurationMillis = atoi(duration) - WIFI_SETTLING_TIME - PUBLISH_SETTLING_TIME;
  return cycleDurationMillis;
}
