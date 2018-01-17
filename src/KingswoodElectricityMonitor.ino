/*
 * Project KingswoodEnergyMonitor
 * Description: Calculates electricity use from a meter's pulse output
 * Author: Richard Lyon
 * Date: 10 Jan 2018
 *
 * power: (Watts) the instantaneous power demand
 * elapsedEnergy: (kWh) energy consumption since midnight
 * dailyCost: (pence) daily energy cost
 */

// Energy contract cost (Pure Planet, 01 Jan 2018)
#define STANDING_CHARGE 32.877; // (pence / day) energy contract standing charge
#define UNIT_COST 11.193; // (pence / kWh) energy contract unit cost

// Ubidots configuration
#include <Ubidots.h>
#define DEVICE_ID "elmon"
#define TOKEN "A1E-Ke3IBMVqyEUnzfGP9aDQRAmY2vGweM"  // Ubidots TOKEN
Ubidots ubidots(TOKEN);

// Pin assignments
int interruptPin = D3;
int ledPin = D7;

// Main variables
double power; // (Watts) current power consumption
double elapsedEnergy; // (kWh) energy consumption since midnight
double dailyCost; // (pence) daily energy cost

// Intermediate variables
int ppwh = 1; // (pulse per Wh)
long pulseCount = 0; // keeps track of the number of pulses detected
unsigned long pulseTime, lastTime, pulseInterval; // milliseconds
double standingCharge = STANDING_CHARGE; // (pence) energy contract standing charge
double unitCost = UNIT_COST; // (pence / kWh) energy contract unit cost (pence / kWh)
int thishour, lasthour, thisminute, lastminute;
int state = 0; // used to toggle led
int duration = 6000; // (seconds) initial simulated duration of pulses

// Interrupt control flag
bool pulseFlag = false;

void setup() {
  Serial.begin(9600);

  // Configure pins
  pinMode(ledPin, OUTPUT);
  pinMode(interruptPin, INPUT_PULLDOWN);
  attachInterrupt(interruptPin, flag_pulse, RISING);

  // Set Particle variables
  Particle.variable("power", power);
  Particle.variable("elapsedEngy", elapsedEnergy);
  Particle.variable("dailyCost", dailyCost);

  // Set ubidots device label and ID
  ubidots.setDeviceName(DEVICE_ID);

  // Set ubidots variables
  ubidots.add("power", power);
  ubidots.add("elapsedEnergy", elapsedEnergy);
  ubidots.add("dailyCost", dailyCost);
  ubidots.sendAll();

  // Initialise pulseTime;
  pulseTime = millis();

  // Initialise variables to detect midnight
  thishour = Time.hour();
  thisminute = Time.minute();
  lasthour = thishour;
  lastminute = thisminute;
}

void loop() {

  if (pulseFlag == true) {
    calculate_energy();
    toggle_led();
    pulseFlag = false;
  }

  // if we've gone through midnight, reset elapsedEnergy
  lasthour = thishour;
  lastminute = thisminute;
  thishour = Time.hour();
  thisminute = Time.minute();
  // this is only true minute after midnight - reset elapsedEnergy
  // Note: We can't guarantee we'll be at exactly 0 seconds after midnight
  // so this will fail only after the first minute has elapsed.  Assume negligible.
  if (thishour == 0 && thisminute == 0 &&
      lasthour == 23 && lastminute == 59 ) {
    elapsedEnergy == 0;
    Serial.println("> Midnight. Elapsed energy reset.");
  }
  Serial.print("> Power: ");
  Serial.println(power);
}

// Interrupt handler. Called on the rising esge of a flash.
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
  elapsedEnergy = (1.0*pulseCount/(ppwh*1000)); //multiply by 1000 to convert pulses per wh to kwh

  // calculate cost since midnight
  dailyCost = STANDING_CHARGE + elapsedEnergy*UNIT_COST;
}

// toggle onboard LED to represent received pulses
void toggle_led() {
  digitalWrite(ledPin, (state) ? HIGH : LOW);
  state = !state;
}
