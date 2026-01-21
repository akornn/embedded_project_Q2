# VL53L1X_ULD_ESP_WRAPPER

i created this wrapper to simplify the use of vl53l1x time of flight sensors in esp-idf

there are two potential uses for this component:
1) as demonstrated bellow and in the `example.c` its possible to have the sensor configured and measuring with minimal set up required.
2) using the API directly, provided that user implements the I2C functions that can be found in `core/src/vl53l1_platform.c` *(more on this at the end)*

> future chages to the wraper section include ***potential* breaking changes**

# how to include on your esp-idf project

## using espressif registry

add the dependency and include the component on the cmake file for example
```yml
# /main/idf_component.yml
dependencies:
  idf:
      version: '>=5.5.0'
  saleca/vl53l1x_uld_esp_wrapper: ^0.0.6 # <-- add the component as a dependency
```
```cmake
# /main/CMakeLists.txt
idf_component_register(
    SRCS 
        "main.c"
                    
    INCLUDE_DIRS 
        "."
                    
    REQUIRES
        vl53l1x_uld_esp_wrapper # <-- add the component to the REQUIRES list
        esp_driver_gpio) 
```

## otherwise
[download the component](https://github.com/Saleca/vl53l1x_uld_esp_wrapper.git) into `{root}/components/vl53l1x_uld_esp_wrapper` and add the component to the REQUIRES lists as shown above.

# how to get a measurement

first the `vl53l1x_uld_esp_wrapper` component has to be initialized with at least `scl_gpio` and `sda_gpio` in `vl53l1x_handle.i2c_handle`, this will initialize the platform functions and the `driver/i2c_master.h` component. 

for this example you only need to include `vl53l1x.h`.
```c
vl53l1x_i2c_handle_t vl53l1x_i2c_handle = VL53L1X_I2C_INIT;
vl53l1x_i2c_handle.scl_gpio = SCL_GPIO;
vl53l1x_i2c_handle.sda_gpio = SDA_GPIO;

vl53l1x_handle_t vl53l1x_handle = VL53L1X_INIT;
vl53l1x_handle.i2c_handle = &vl53l1x_i2c_handle;
if (!vl53l1x_init(&vl53l1x_handle))
{
    ESP_LOGE(TAG, "vl53l1x initialization failed");
    return;
}
```

then to add the sensor(s), just assign the `vl53l1x_handle_t` address from before to a `vl53l1x_device_handle_t`.
optionally change the address if not default `vl53l1x_device.i2c_address = 0x01;` the initialization will fail if address is not correct but there will be a logw in case some other address is found.

```c
vl53l1x_device_handle_t vl53l1x_device = VL53L1X_DEVICE_INIT;
vl53l1x_device.vl53l1x_handle = &vl53l1x_handle;
if (!vl53l1x_add_device(&vl53l1x_device))
{
    ESP_LOGE(TAG, "failed to add vl53l1x device 0x%02X", vl53l1x_device.i2c_address);
    return;
}
```

and finaly to get a measurement just call `vl53l1x_get_mm` with the `vl53l1x_device_handle_t` address from before.

```c
uint16_t distance = vl53l1x_get_mm(&vl53l1x_device);
```

now build and flash to the device.


## to use the API directly
the API expects an i2c implementation, by default the `vl53l1x_init` assigns my implementation with `driver/i2c_master.h` but if you need other i2c configuration you can either modify directly the methods in `vl53l1x_platform.h` or you can create your implementation using the following definitions and assign the methods when initializing the sensor.

```c
// for all definitions see vl53l1x_platform.h
typedef int8_t (*vl53l1x_write_multi)(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count);

extern vl53l1x_write_multi g_vl53l1x_write_multi_ptr;
```
example of usage:
```c
// new i2c definition
int8_t i2c_write_multi(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count);

// needed before any interation with the API
g_vl53l1x_write_multi_ptr = i2c_write_multi;
```
then you can initialize the sensor for minimal example see `vl53l1x.c` `vl53l1x_add_device` implementation.


### any criticism, suggestions or contributions are welcome. 

future changes potentially include:

- rearrangement of abstraction layer

- modifying the raw API in favor of using handles bypassing the API platform abstraction declutering the component.

- add sensor configuration handle 

- add calibration procedures

- proper error handling (at the moment the wrapper logs most of the errors but only returns bool)

> although the component wrapper is under **MIT** licensing the contents from `core` directory are sourced from the **ST VL53L1X ULD API** and are governed by the open source **SLA0103** license from STMicroelectronics