#pragma once

/**
 * @brief Start the ToF + Ultrasonic feedback task
 *
 * Combines:
 *  - Ultrasonic sensor → servo vibration (≤ 80 cm)
 *  - ToF sensor        → buzzer beeping   (≤ 500 mm)
 */
void tof_buzzer_task_start(void);
