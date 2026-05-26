# `scd41_lib` - Hướng dẫn sử dụng

Tài liệu này mô tả đúng theo trạng thái hiện tại của thư mục `scd41_lib/` và cách nó đang được dùng trong `Core/Src/main.c`.

## 1) Thư viện này gồm những gì?

### Driver SCD4x

File chính:

- `scd4x_i2c.h`
- `scd4x_i2c.c`

Đây là bộ driver I2C cho cảm biến SCD4x của Sensirion, hỗ trợ:

- đọc CO₂, nhiệt độ, độ ẩm
- `start/stop periodic measurement`
- kiểm tra dữ liệu đã sẵn sàng hay chưa
- các lệnh cấu hình như offset nhiệt độ, altitude, áp suất, ASC, self-test, factory reset, reinit...

### HAL I2C wrapper

File chính:

- `sensirion_i2c_hal.h`
- `sensirion_i2c_hal.c`

Module này dùng để gắn driver vào STM32 HAL và CMSIS-RTOS:

- bind `I2C_HandleTypeDef*`
- bind mutex I2C nếu dùng FreeRTOS/CMSIS-RTOS
- tự phục hồi peripheral khi bus I2C bị lỗi

### Helper in dữ liệu

File chính:

- `scd41_print.h`
- `scd41_print.c`

Module này hỗ trợ in kết quả đo ra ITM console.

## 2) Include cần dùng

Trong `Core/Src/main.c`, các header thường dùng là:

```c
#include "main.h"
#include "cmsis_os.h"
#include "itm.h"
#include "scd4x_i2c.h"
#include "scd41_print.h"
```

Nếu cần làm việc trực tiếp với HAL wrapper, có thể include thêm:

```c
#include "sensirion_i2c_hal.h"
```

## 3) Luồng sử dụng hiện tại trong project

Trong project này, luồng đang chạy theo kiểu:

- `TaskInput` khởi tạo runtime của SCD41
- task này gọi `Scd41Api_Service(...)` mỗi giây
- khi có mẫu mới, dữ liệu được đẩy vào queue `Queuescd41hi2c2Handle`
- `TaskUI` đọc queue và in ra ITM bằng `scd41_print_scd41_measurement(...)`

### Hai struct runtime quan trọng

- `scd41_config_t`: chứa cấu hình I2C (`i2c_handle`, `i2c_mutex`, `i2c_address`, `restart_after_failures`)
- `scd41_context_t`: chứa dữ liệu đo và trạng thái runtime (`co2`, `temperature_m_deg_c`, `humidity_m_percent_rh`, `error`, `fault_cause`, ...)

## 4) Ví dụ đúng theo `main.c`

### Task khởi tạo và poll sensor

```c
void StartTaskInput(void *argument)
{
  itm_print("TaskInput started\r\n");

  scd41_config_t scd41_config = {0};
  scd41_context_t scd41_context = {0};

  scd41_config.i2c_handle = &hi2c1;
  scd41_config.i2c_mutex = MutexI2C1Handle;

  scd4x_runtime_init(&scd41_config, &scd41_context);
  scd4x_runtime_start_periodic_measurement(&scd41_config, &scd41_context);

  for (;;)
  {
    osDelay(1000);

    Scd41Api_Service(&scd41_config,
                     &scd41_context,
                     Queuescd41hi2c2Handle,
                     scd4x_runtime_default_itm_event_handler);
  }
}
```

### Task in dữ liệu ra ITM

```c
void StartTaskUI(void *argument)
{
  scd41_queue_item_t measurement = {0};

  for (;;)
  {
    if (osMessageQueueGet(Queuescd41hi2c2Handle,
                          &measurement,
                          NULL,
                          osWaitForever) == osOK)
    {
      scd41_print_scd41_measurement(measurement.co2,
                                    measurement.temperature_m_deg_c,
                                    measurement.humidity_m_percent_rh);
    }
  }
}
```

## 5) API runtime quan trọng

- `scd4x_runtime_init(&config, &context)`
- `scd4x_runtime_set_event_callback(&config, &context, callback, user_context)`
- `scd4x_runtime_start_periodic_measurement(&config, &context)`
- `scd4x_runtime_read_if_ready(&config, &context)`
- `scd4x_runtime_poll(&config, &context)`
- `Scd41Api_Service(&config, &context, queue, callback)`

### Callback mặc định

- `scd4x_runtime_default_itm_event_handler(const scd41_config_t*, const scd41_context_t*, ...)`

Callback này sẽ in thông tin lỗi/phục hồi ra ITM console, rất hữu ích khi bus I2C không ổn định.

## 6) Các API low-level có sẵn

### HAL wrapper

- `sensirion_i2c_hal_set_handle(I2C_HandleTypeDef* i2c_handle)`
- `sensirion_i2c_hal_set_mutex(osMutexId_t i2c_mutex)`
- `sensirion_i2c_hal_init()`
- `sensirion_i2c_hal_recover_bus()`

### Driver SCD4x low-level

- `scd4x_init(uint8_t i2c_address)`
- `scd4x_start_periodic_measurement()`
- `scd4x_stop_periodic_measurement()`
- `scd4x_get_data_ready_status(bool* data_ready)`
- `scd4x_read_measurement(uint16_t* co2, int32_t* temperature_m_deg_c, int32_t* humidity_m_percent_rh)`
- `scd4x_read_measurement_raw(...)`

### Một số lệnh cấu hình khác

- `scd4x_set_temperature_offset_raw(...)`
- `scd4x_set_sensor_altitude(...)`
- `scd4x_set_ambient_pressure(...)` / `scd4x_set_ambient_pressure_raw(...)`
- `scd4x_set_automatic_self_calibration_enabled(...)`
- `scd4x_set_automatic_self_calibration_target(...)`
- `scd4x_persist_settings()`
- `scd4x_reinit()`
- `scd4x_perform_self_test(...)`
- `scd4x_perform_factory_reset()`

## 7) Hành vi phục hồi lỗi

Trong `sensirion_i2c_hal.c`, khi giao dịch I2C lỗi, thư viện sẽ gọi:

```c
sensirion_i2c_hal_recover_bus();
```

Mục đích là thử reset lại peripheral I2C để tăng khả năng tự phục hồi khi:

- mất nguồn cảm biến rồi cấp lại
- rút/cắm lại SDA hoặc SCL
- bus bị treo tạm thời

## 8) Lưu ý quan trọng

- Cần tạo mutex trước khi gắn vào `sensirion_i2c_hal_set_mutex(...)`.
- Nếu không dùng RTOS, có thể truyền `NULL` cho mutex, nhưng khi đó sẽ không còn bảo vệ truy cập I2C.
- Trong project hiện tại, queue phải chứa `scd41_queue_item_t` chứ không phải `uint16_t`. Nếu kích thước queue sai, dữ liệu nhiệt độ/độ ẩm sẽ bị đọc rác và log ITM sẽ sai rất mạnh.
- `scd41_print_scd41_measurement(...)` chỉ là helper in kết quả, không thay thế cho driver đo.

## 9) Tóm tắt nhanh

```c
scd41_config_t config = {0};
scd41_context_t context = {0};

config.i2c_handle = &hi2c1;
config.i2c_mutex = MutexI2C1Handle;

scd4x_runtime_init(&config, &context);
scd4x_runtime_set_event_callback(&config,
                                 &context,
                                 scd4x_runtime_default_itm_event_handler,
                                 NULL);
(void)scd4x_runtime_start_periodic_measurement(&config, &context);
```

Sau đó lặp:

```c
if (scd4x_runtime_poll(&config, &context)) {
  scd41_print_scd41_measurement(context.co2,
                                context.temperature_m_deg_c,
                                context.humidity_m_percent_rh);
}
```
