#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

/**
 * @brief Initializes the MCPWM peripheral for the servo
 */
void servo_init(void);

/**
 * @brief Sets the servo angle
 * @param angle Angle in degrees (usually -90 to 90)
 */
void servo_set_angle(int angle);

/**
 * @brief Task that sweeps the servo back and forth (Alex's original logic)
 */
void servo_sweep_task(void *pvParameters);

#endif