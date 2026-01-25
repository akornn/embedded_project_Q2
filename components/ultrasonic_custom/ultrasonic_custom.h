#ifndef ULTRASONIC_CUSTOM_H
#define ULTRASONIC_CUSTOM_H

#include <stdint.h>
#include <stdbool.h>

// Initialize with your pins
void ultrasonic_custom_init(int trig_pin, int echo_pin);

// Get distance in cm (returns -1 if no reading)
float ultrasonic_custom_get_distance_cm(void);

// Check if new measurement is available
bool ultrasonic_custom_measurement_available(void);

#endif