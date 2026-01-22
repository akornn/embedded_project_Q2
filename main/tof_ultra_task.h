#ifndef TOF_ULTRA_TASK_H
#define TOF_ULTRA_TASK_H

#include "driver/i2c_master.h"

// Update the declaration to include the bus_handle parameter
void tof_ultra_task_start(i2c_master_bus_handle_t bus_handle);

#endif