/*
 * app_menu.h
 *
 * Menu điều khiển môi trường nhà trồng nấm.
 * Điều hướng bằng EC11 Rotary Encoder:
 *   - Xoay CW/CCW : di chuyển con trỏ hoặc thay đổi giá trị
 *   - Nhấn ngắn   : vào / xác nhận
 *   - Nhấn dài    : quay lại
 *
 * Cấu trúc menu:
 *  [WORK1] <CW> [WORK2]  (SHORT để vào MAIN_MENU)
 *  MAIN_MENU
 *    ├─ Chon che do     -> SCREEN_MODE_SELECT
 *    ├─ Cai dat t/gian  -> SCREEN_TIME_MENU -> SCREEN_TIME_EDIT
 *    ├─ Cai dat MinMax  -> SCREEN_MINMAX_MODE -> PARAM -> FIELD -> EDIT
 *    └─ Vi tri DS18B20  -> SCREEN_DS18B20_POS
 */

#ifndef APP_MENU_H_
#define APP_MENU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "rtrecd.h"
#include "scd4x_i2c.h"
#include "ds18b20_app.h"

/* =========================================================================
 * Constants
 * ========================================================================= */

#define MENU_NAV_DEPTH    10   /**< Độ sâu tối đa của navigation stack */
#define MENU_DS18B20_MAX   6   /**< Số cảm biến DS18B20 tối đa (6 vị trí) */

/* =========================================================================
 * Enums
 * ========================================================================= */

/** @brief Tất cả màn hình trong hệ thống menu */
typedef enum
{
    SCREEN_WORK1 = 0,      /**< Màn hình làm việc 1 (sensor realtime)        */
    SCREEN_WORK2,          /**< Màn hình làm việc 2 (DS18B20 từng vị trí)   */
    SCREEN_WORK3,          /**< Màn hình làm việc 3 (MinMax chế độ hiện tại)*/
    SCREEN_MAIN_MENU,      /**< Menu chính                                   */
    SCREEN_MODE_SELECT,    /**< Chọn chế độ vận hành                        */
    SCREEN_TIME_MENU,      /**< Danh sách trường thời gian                   */
    SCREEN_TIME_EDIT,      /**< Chỉnh sửa 1 trường thời gian                */
    SCREEN_MINMAX_MODE,    /**< Chọn chế độ để cài MinMax                   */
    SCREEN_MINMAX_PARAM,   /**< Chọn thông số (Nhiệt độ/Độ ẩm/CO2/Đèn)     */
    SCREEN_MINMAX_FIELD,   /**< Chọn trường (Min/Max hoặc Start/Stop)       */
    SCREEN_MINMAX_EDIT,    /**< Chỉnh sửa giá trị MinMax                    */
    SCREEN_DS18B20_POS,    /**< Cài đặt vị trí DS18B20                      */
    SCREEN_COUNT
} app_screen_t;

/** @brief Chế độ vận hành */
typedef enum
{
    MODE_CHAY_TO = 0,
    MODE_DINH_GHIM,
    MODE_QUA_THE,
    MODE_THANH_TRUNG,
    MODE_NGHI,
    MODE_COUNT
} app_mode_t;

/** @brief Thông số MinMax */
typedef enum
{
    PARAM_NHIET_DO = 0,
    PARAM_DO_AM,
    PARAM_CO2,
    PARAM_DEN,
    PARAM_COUNT
} minmax_param_t;

/* =========================================================================
 * Settings structures
 * ========================================================================= */

/** @brief Cặp ngưỡng Min/Max (đơn vị: x10 cho nhiệt/ẩm, ppm cho CO2) */
typedef struct
{
    int16_t min;
    int16_t max;
} minmax_range_t;

/** @brief Cài đặt thời gian bật/tắt đèn */
typedef struct
{
    uint8_t time_start_h;  /**< Giờ bật   */
    uint8_t time_start_m;  /**< Phút bật  */
    uint8_t time_stop_h;   /**< Giờ tắt   */
    uint8_t time_stop_m;   /**< Phút tắt  */
} minmax_den_t;

/** @brief Cài đặt cho 1 chế độ (nhiet do / do am / co2 / den) */
typedef struct
{
    minmax_range_t nhiet_do;   /**< °C   : 20~35 (chay to/dinh ghim/qua the), 20~100 (thanh trung) */
    minmax_range_t do_am;      /**< %RH  : 50~95  */
    minmax_range_t co2;        /**< ppm  : 400~5000 (bước 500) */
    minmax_den_t   den;        /**< giờ  : 0~24 (time_start_h / time_stop_h) */
} mode_settings_t;

/** @brief Cài đặt thời gian thực */
typedef struct
{
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} app_time_t;

/* =========================================================================
 * Navigation stack frame
 * ========================================================================= */

/** @brief 1 frame trên navigation stack */
typedef struct
{
    app_screen_t screen;
    uint8_t      cursor;
    uint8_t      scroll;
} nav_frame_t;

/* =========================================================================
 * Main menu context
 * ========================================================================= */

typedef struct
{
    /* --- Navigation --- */
    nav_frame_t  stack[MENU_NAV_DEPTH]; /**< Navigation stack                */
    uint8_t      stack_top;             /**< Index của frame tiếp theo        */
    app_screen_t screen;                /**< Màn hình hiện tại               */
    uint8_t      cursor;                /**< Con trỏ trong list hiện tại     */
    uint8_t      scroll;                /**< Vị trí cuộn (top visible index) */
    bool         dirty;                 /**< true = cần vẽ lại LCD           */
    bool         time_rtc_dirty;        /**< true = time_cfg đã sửa, cần ghi RTC */
    bool         settings_dirty;        /**< true = mode/MinMax đã sửa, cần lưu Flash */

    /* --- Sensor data (cập nhật từ TaskUI) --- */
    scd41_queue_item_t scd41;
    Ds18b20QueueItem   ds18b20[MENU_DS18B20_MAX];
    uint8_t            ds18b20_count;

    /* --- Settings --- */
    app_mode_t      active_mode;
    mode_settings_t mode_cfg[4]; /**< [0]=Chay to [1]=Dinh ghim [2]=Qua the [3]=Thanh trung */
    app_time_t      time_cfg;
    uint8_t         ds18b20_role[MENU_DS18B20_MAX]; /**< Gán vai trò cho từng cảm biến */

    /* --- Edit context (dùng cho SCREEN_TIME_EDIT và SCREEN_MINMAX_EDIT) --- */
    int32_t edit_value;        /**< Giá trị đang chỉnh                       */
    int32_t edit_min;          /**< Giới hạn dưới                            */
    int32_t edit_max;          /**< Giới hạn trên                            */
    uint8_t edit_field_index;  /**< Index trường đang sửa (time field / min-max field) */
    uint8_t edit_mode_index;   /**< Index chế độ đang cài MinMax             */
    uint8_t edit_param_index;  /**< Index thông số đang cài MinMax           */

} app_menu_ctx_t;

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Khởi tạo context menu, gán giá trị mặc định.
 * @note  Gọi từ TaskLCD trước vòng lặp chính.
 */
void app_menu_init(app_menu_ctx_t *ctx);

/**
 * @brief Xử lý 1 event từ EC11 encoder.
 * @note  Gọi từ TaskUI khi có event mới trong QueueEC11.
 */
void app_menu_handle_event(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev);

/**
 * @brief Vẽ lại LCD nếu có thay đổi (dirty flag).
 * @note  Gọi từ TaskLCD theo chu kỳ.
 */
void app_menu_render(app_menu_ctx_t *ctx);

/**
 * @brief Cập nhật dữ liệu SCD41 vào context.
 * @note  Gọi từ TaskUI khi nhận được data từ QueueSCD41.
 */
void app_menu_update_scd41(app_menu_ctx_t *ctx, const scd41_queue_item_t *data);

/**
 * @brief Cập nhật dữ liệu DS18B20 vào context.
 * @note  Gọi từ TaskUI khi nhận được data từ QueueDS18B20.
 */
void app_menu_update_ds18b20(app_menu_ctx_t *ctx, const Ds18b20QueueItem *data);

/**
 * @brief Đánh dấu dirty để TaskLCD vẽ lại màn hình làm việc.
 * @note  Gọi sau khi cập nhật sensor data.
 */
void app_menu_mark_dirty(app_menu_ctx_t *ctx);

/**
 * @brief Đọc thời gian từ RTC phần cứng và cập nhật vào ctx->time_cfg.
 * @note  Gọi định kỳ từ TaskUI (mỗi 1 giây) để màn hình làm việc luôn cập nhật.
 * @param ctx   Con trỏ menu context.
 * @param hrtc  Con trỏ RTC handle (từ CubeMX).
 */
void app_menu_update_time_from_rtc(app_menu_ctx_t *ctx, RTC_HandleTypeDef *hrtc);

/**
 * @brief Ghi ctx->time_cfg vào RTC phần cứng.
 * @note  Gọi từ TaskUI sau khi phát hiện ctx->time_rtc_dirty == true.
 *        Hàm tự clear cờ time_rtc_dirty sau khi ghi xong.
 * @param ctx   Con trỏ menu context.
 * @param hrtc  Con trỏ RTC handle (từ CubeMX).
 */
void app_menu_write_time_to_rtc(app_menu_ctx_t *ctx, RTC_HandleTypeDef *hrtc);

#ifdef __cplusplus
}
#endif

#endif /* APP_MENU_H_ */
