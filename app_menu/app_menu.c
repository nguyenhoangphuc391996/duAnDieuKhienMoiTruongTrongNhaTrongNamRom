/*
 * app_menu.c
 *
 * Toàn bộ logic menu: navigation stack, render, event handler.
 */

#include "app_menu.h"
#include "lcd.h"
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * String tables
 * ========================================================================= */

static const char * const g_main_menu_items[] = {
    "Chon che do",
    "Cai dat t/gian",
    "Cai dat MinMax",
    "Vi tri DS18B20",
};
#define MAIN_MENU_COUNT   4U

static const char * const g_mode_items[] = {
    "Chay to",
    "Dinh ghim",
    "Qua the",
    "Thanh trung",
    "Nghi",
};
#define MODE_ITEM_COUNT   5U

static const char * const g_time_items[] = {
    "Set Year",
    "Set Month",
    "Set Day",
    "Set Hour",
    "Set Minute",
    "Set Second",
};
#define TIME_ITEM_COUNT   6U

static const char * const g_minmax_mode_items[] = {
    "Chay to",
    "Dinh ghim",
    "Qua the",
    "Thanh trung",
};
#define MINMAX_MODE_COUNT 4U

static const char * const g_minmax_param_items[] = {
    "Nhiet do",
    "Do am",
    "Nong do CO2",
    "Den",
};
#define MINMAX_PARAM_COUNT 4U

static const char * const g_field_minmax[] = { "Min", "Max" };
static const char * const g_field_den[]    = { "Time Start", "Time Stop" };

/* g_mode_short removed - replaced by g_mode_char in render_work1/2 */

/* =========================================================================
 * Navigation helpers
 * ========================================================================= */

/**
 * @brief Đẩy màn hình hiện tại vào stack và chuyển sang màn hình mới.
 */
static void nav_push(app_menu_ctx_t *ctx, app_screen_t new_screen)
{
    if (ctx->stack_top < MENU_NAV_DEPTH - 1U)
    {
        ctx->stack[ctx->stack_top].screen = ctx->screen;
        ctx->stack[ctx->stack_top].cursor = ctx->cursor;
        ctx->stack[ctx->stack_top].scroll  = ctx->scroll;
        ctx->stack_top++;
    }
    ctx->screen = new_screen;
    ctx->cursor = 0U;
    ctx->scroll  = 0U;
    ctx->dirty   = true;
}

/**
 * @brief Lấy màn hình trước từ stack (quay lại).
 */
static void nav_pop(app_menu_ctx_t *ctx)
{
    if (ctx->stack_top > 0U)
    {
        ctx->stack_top--;
        ctx->screen = ctx->stack[ctx->stack_top].screen;
        ctx->cursor = ctx->stack[ctx->stack_top].cursor;
        ctx->scroll  = ctx->stack[ctx->stack_top].scroll;
        ctx->dirty   = true;
    }
}

/* =========================================================================
 * List / Edit motion helpers
 * ========================================================================= */

/** @brief Di chuyển cursor xuống (CW), cuộn nếu cần. */
static void list_cw(app_menu_ctx_t *ctx, uint8_t count)
{
    if (ctx->cursor < (uint8_t)(count - 1U))
    {
        ctx->cursor++;
        if (ctx->cursor > (uint8_t)(ctx->scroll + 1U))
        {
            ctx->scroll++;
        }
        ctx->dirty = true;
    }
}

/** @brief Di chuyển cursor lên (CCW), cuộn nếu cần. */
static void list_ccw(app_menu_ctx_t *ctx)
{
    if (ctx->cursor > 0U)
    {
        ctx->cursor--;
        if (ctx->cursor < ctx->scroll)
        {
            ctx->scroll--;
        }
        ctx->dirty = true;
    }
}

/** @brief Tăng giá trị chỉnh sửa (CW). */
static void edit_cw(app_menu_ctx_t *ctx)
{
    if (ctx->edit_value < ctx->edit_max)
    {
        ctx->edit_value++;
        ctx->dirty = true;
    }
}

/** @brief Giảm giá trị chỉnh sửa (CCW). */
static void edit_ccw(app_menu_ctx_t *ctx)
{
    if (ctx->edit_value > ctx->edit_min)
    {
        ctx->edit_value--;
        ctx->dirty = true;
    }
}

/* =========================================================================
 * LCD render helpers
 * ========================================================================= */

/**
 * @brief Hiển thị danh sách 2 dòng với con trỏ '>' .
 *
 * Mỗi lần hiển thị 2 item (tương ứng 2 dòng LCD 16x2).
 * scroll xác định item đầu tiên được hiển thị.
 */
static void render_list_2row(const char * const items[],
                              uint8_t count,
                              uint8_t cursor,
                              uint8_t scroll)
{
    char line[32];
    for (uint8_t row = 0U; row < 2U; row++)
    {
        uint8_t idx = scroll + row;
        lcd_put_cur(row, 0);
        if (idx < count)
        {
            snprintf(line, sizeof(line), "%c%-15s",
                     (cursor == idx) ? '>' : ' ',
                     items[idx]);
        }
        else
        {
            snprintf(line, sizeof(line), "%-16s", " ");
        }
        line[16] = '\0';
        lcd_send_string(line);
    }
}

/**
 * @brief Hiển thị màn hình chỉnh sửa giá trị số.
 *
 * Dòng 0: tên trường.
 * Dòng 1: < value > (mũi tên ẩn khi đạt giới hạn).
 */
static void render_edit(const char *field_name,
                         int32_t value,
                         int32_t vmin,
                         int32_t vmax)
{
    char line[32];

    /* Dòng 0: tên trường */
    snprintf(line, sizeof(line), "%-16s", field_name);
    line[16] = '\0';
    lcd_put_cur(0, 0);
    lcd_send_string(line);

    /* Dòng 1: giá trị với mũi tên */
    char left  = (value > vmin) ? '<' : ' ';
    char right = (value < vmax) ? '>' : ' ';
    snprintf(line, sizeof(line), "%c %6ld       %c", left, value, right);
    line[16] = '\0';
    lcd_put_cur(1, 0);
    lcd_send_string(line);
}

/* =========================================================================
 * Screen render functions
 * ========================================================================= */

/* Ký tự viết tắt cho từng chế độ vận hành */
static const char g_mode_char[] = { 'C', 'D', 'Q', 'T', 'N' };

static void render_work1(app_menu_ctx_t *ctx)
{
    /* Buffer 48 bytes: đủ cho worst case snprintf dòng 1 (~39 chars) */
    char line[48];

    /* ---- Dòng 0: [Mode] [HH:MM] [DD/MM/YY] ----
     * Ví dụ: "C 19:50 30/12/26"  (16 chars)
     */
    char mode_c = g_mode_char[ctx->active_mode];
    snprintf(line, sizeof(line), "%c %02u:%02u %02u/%02u/%02u",
             mode_c,
             ctx->time_cfg.hour,   ctx->time_cfg.minute,
             ctx->time_cfg.day,    ctx->time_cfg.month,
             (uint8_t)(ctx->time_cfg.year % 100U));
    line[16] = '\0';
    lcd_put_cur(0, 0);
    lcd_send_string(line);

    /* ---- Dòng 1: T[avg°C] A[%RH] C[ppm] ----
     * Ví dụ: "T30 A90 C4000   "  (16 chars)
     * Nhiệt độ: trung bình các cảm biến DS18B20 (nguyên °C)
     * Độ ẩm   : SCD41 humidity_m_percent_rh / 1000
     * CO2     : SCD41 co2 (ppm)
     */
    int32_t avg_t = 0;
    if (ctx->ds18b20_count > 0U)
    {
        for (uint8_t i = 0U; i < ctx->ds18b20_count; i++)
        {
            avg_t += (int32_t)ctx->ds18b20[i].tempDeciC;
        }
        avg_t /= (int32_t)ctx->ds18b20_count;
        avg_t /= 10;   /* deciC -> °C */
    }
    int32_t humi = ctx->scd41.humidity_m_percent_rh / 1000L;

    snprintf(line, sizeof(line), "T%ld A%ld C%u       ",
             avg_t, humi, ctx->scd41.co2);
    line[16] = '\0';
    lcd_put_cur(1, 0);
    lcd_send_string(line);
}

static void render_work2(app_menu_ctx_t *ctx)
{
    char line[32];
    char slot[3][8];

    /* ---- Dòng 0: vị trí 1, 2, 3  (cảm biến index 0,1,2) ----
     * Ví dụ: "1:20 2:21 3:22  "  (16 chars)
     * Mỗi slot: "N:TT" (4 chars) + space phân cách
     */
    for (uint8_t i = 0U; i < 3U; i++)
    {
        if (i < ctx->ds18b20_count)
        {
            int16_t t = ctx->ds18b20[i].tempDeciC / 10;
            snprintf(slot[i], sizeof(slot[i]), "%u:%d", (unsigned)(i + 1U), (int)t);
        }
        else
        {
            snprintf(slot[i], sizeof(slot[i]), "%u:--", (unsigned)(i + 1U));
        }
    }
    snprintf(line, sizeof(line), "%-5s%-5s%-6s", slot[0], slot[1], slot[2]);
    line[16] = '\0';
    lcd_put_cur(0, 0);
    lcd_send_string(line);

    /* ---- Dòng 1: vị trí 4, 5, 6  (cảm biến index 3,4,5) ---- */
    for (uint8_t i = 3U; i < 6U; i++)
    {
        if (i < ctx->ds18b20_count)
        {
            int16_t t = ctx->ds18b20[i].tempDeciC / 10;
            snprintf(slot[i - 3U], sizeof(slot[0]), "%u:%d", (unsigned)(i + 1U), (int)t);
        }
        else
        {
            snprintf(slot[i - 3U], sizeof(slot[0]), "%u:--", (unsigned)(i + 1U));
        }
    }
    snprintf(line, sizeof(line), "%-5s%-5s%-6s", slot[0], slot[1], slot[2]);
    line[16] = '\0';
    lcd_put_cur(1, 0);
    lcd_send_string(line);
}

static void render_main_menu(app_menu_ctx_t *ctx)
{
    render_list_2row(g_main_menu_items, MAIN_MENU_COUNT,
                     ctx->cursor, ctx->scroll);
}

static void render_mode_select(app_menu_ctx_t *ctx)
{
    render_list_2row(g_mode_items, MODE_ITEM_COUNT,
                     ctx->cursor, ctx->scroll);
}

static void render_time_menu(app_menu_ctx_t *ctx)
{
    render_list_2row(g_time_items, TIME_ITEM_COUNT,
                     ctx->cursor, ctx->scroll);
}

static void render_time_edit(app_menu_ctx_t *ctx)
{
    render_edit(g_time_items[ctx->edit_field_index],
                ctx->edit_value,
                ctx->edit_min,
                ctx->edit_max);
}

static void render_minmax_mode(app_menu_ctx_t *ctx)
{
    render_list_2row(g_minmax_mode_items, MINMAX_MODE_COUNT,
                     ctx->cursor, ctx->scroll);
}

static void render_minmax_param(app_menu_ctx_t *ctx)
{
    render_list_2row(g_minmax_param_items, MINMAX_PARAM_COUNT,
                     ctx->cursor, ctx->scroll);
}

static void render_minmax_field(app_menu_ctx_t *ctx)
{
    if (ctx->edit_param_index == (uint8_t)PARAM_DEN)
    {
        render_list_2row(g_field_den, 2U, ctx->cursor, ctx->scroll);
    }
    else
    {
        render_list_2row(g_field_minmax, 2U, ctx->cursor, ctx->scroll);
    }
}

static void render_minmax_edit(app_menu_ctx_t *ctx)
{
    char field_name[17];
    if (ctx->edit_param_index == (uint8_t)PARAM_DEN)
    {
        snprintf(field_name, sizeof(field_name), "%.16s",
                 g_field_den[ctx->edit_field_index]);
    }
    else
    {
        snprintf(field_name, sizeof(field_name), "%.8s %.3s",
                 g_minmax_param_items[ctx->edit_param_index],
                 g_field_minmax[ctx->edit_field_index]);
    }
    render_edit(field_name, ctx->edit_value, ctx->edit_min, ctx->edit_max);
}

static void render_ds18b20_pos(app_menu_ctx_t *ctx)
{
    char line[32];
    lcd_put_cur(0, 0);
    lcd_send_string("Vi tri DS18B20  ");
    snprintf(line, sizeof(line), "So CB: %d       ", ctx->ds18b20_count);
    line[16] = '\0';
    lcd_put_cur(1, 0);
    lcd_send_string(line);
}

/* =========================================================================
 * Time edit helpers
 * ========================================================================= */

static const int32_t g_time_edit_min[] = { 2020, 1,  1,  0,  0,  0 };
static const int32_t g_time_edit_max[] = { 2099, 12, 31, 23, 59, 59 };

static void enter_time_edit(app_menu_ctx_t *ctx, uint8_t field)
{
    const int32_t vals[] = {
        (int32_t)ctx->time_cfg.year,
        (int32_t)ctx->time_cfg.month,
        (int32_t)ctx->time_cfg.day,
        (int32_t)ctx->time_cfg.hour,
        (int32_t)ctx->time_cfg.minute,
        (int32_t)ctx->time_cfg.second,
    };
    ctx->edit_field_index = field;
    ctx->edit_min         = g_time_edit_min[field];
    ctx->edit_max         = g_time_edit_max[field];
    ctx->edit_value       = vals[field];
    nav_push(ctx, SCREEN_TIME_EDIT);
}

static void save_time_edit(app_menu_ctx_t *ctx)
{
    switch (ctx->edit_field_index)
    {
    case 0U: ctx->time_cfg.year   = (uint16_t)ctx->edit_value; break;
    case 1U: ctx->time_cfg.month  = (uint8_t)ctx->edit_value;  break;
    case 2U: ctx->time_cfg.day    = (uint8_t)ctx->edit_value;  break;
    case 3U: ctx->time_cfg.hour   = (uint8_t)ctx->edit_value;  break;
    case 4U: ctx->time_cfg.minute = (uint8_t)ctx->edit_value;  break;
    case 5U: ctx->time_cfg.second = (uint8_t)ctx->edit_value;  break;
    default: break;
    }
}

/* =========================================================================
 * MinMax edit helpers
 * ========================================================================= */

static void get_minmax_range(app_menu_ctx_t *ctx,
                              int32_t *vmin, int32_t *vmax)
{
    switch ((minmax_param_t)ctx->edit_param_index)
    {
    case PARAM_NHIET_DO: *vmin = -400;  *vmax = 800;   break; /* x10 °C  */
    case PARAM_DO_AM:    *vmin = 0;     *vmax = 1000;  break; /* x10 %RH */
    case PARAM_CO2:      *vmin = 400;   *vmax = 5000;  break; /* ppm     */
    case PARAM_DEN:      *vmin = 0;     *vmax = 2359;  break; /* HHMM    */
    default:             *vmin = 0;     *vmax = 100;   break;
    }
}

static int32_t get_minmax_value(app_menu_ctx_t *ctx)
{
    mode_settings_t *cfg = &ctx->mode_cfg[ctx->edit_mode_index];
    switch ((minmax_param_t)ctx->edit_param_index)
    {
    case PARAM_NHIET_DO:
        return (ctx->edit_field_index == 0U) ?
               (int32_t)cfg->nhiet_do.min : (int32_t)cfg->nhiet_do.max;
    case PARAM_DO_AM:
        return (ctx->edit_field_index == 0U) ?
               (int32_t)cfg->do_am.min : (int32_t)cfg->do_am.max;
    case PARAM_CO2:
        return (ctx->edit_field_index == 0U) ?
               (int32_t)cfg->co2.min : (int32_t)cfg->co2.max;
    case PARAM_DEN:
        if (ctx->edit_field_index == 0U)
            return (int32_t)cfg->den.time_start_h * 100 +
                   (int32_t)cfg->den.time_start_m;
        else
            return (int32_t)cfg->den.time_stop_h * 100 +
                   (int32_t)cfg->den.time_stop_m;
    default:
        return 0;
    }
}

static void save_minmax_value(app_menu_ctx_t *ctx)
{
    mode_settings_t *cfg = &ctx->mode_cfg[ctx->edit_mode_index];
    switch ((minmax_param_t)ctx->edit_param_index)
    {
    case PARAM_NHIET_DO:
        if (ctx->edit_field_index == 0U) cfg->nhiet_do.min = (int16_t)ctx->edit_value;
        else                             cfg->nhiet_do.max = (int16_t)ctx->edit_value;
        break;
    case PARAM_DO_AM:
        if (ctx->edit_field_index == 0U) cfg->do_am.min = (int16_t)ctx->edit_value;
        else                             cfg->do_am.max = (int16_t)ctx->edit_value;
        break;
    case PARAM_CO2:
        if (ctx->edit_field_index == 0U) cfg->co2.min = (int16_t)ctx->edit_value;
        else                             cfg->co2.max = (int16_t)ctx->edit_value;
        break;
    case PARAM_DEN:
        if (ctx->edit_field_index == 0U)
        {
            cfg->den.time_start_h = (uint8_t)(ctx->edit_value / 100);
            cfg->den.time_start_m = (uint8_t)(ctx->edit_value % 100);
        }
        else
        {
            cfg->den.time_stop_h = (uint8_t)(ctx->edit_value / 100);
            cfg->den.time_stop_m = (uint8_t)(ctx->edit_value % 100);
        }
        break;
    default:
        break;
    }
}

/* =========================================================================
 * Per-screen event handlers
 * ========================================================================= */

static void handle_work1(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CW:
        ctx->screen = SCREEN_WORK2;
        ctx->dirty  = true;
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
        nav_push(ctx, SCREEN_MAIN_MENU);
        break;
    default:
        break;
    }
}

static void handle_work2(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CCW:
        /* fall-through */
    case RTRECD_EVENT_BUTTON_LONG:
        ctx->screen = SCREEN_WORK1;
        ctx->dirty  = true;
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
        nav_push(ctx, SCREEN_MAIN_MENU);
        break;
    default:
        break;
    }
}

static void handle_main_menu(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CW:
        list_cw(ctx, MAIN_MENU_COUNT);
        break;
    case RTRECD_EVENT_ROTATE_CCW:
        list_ccw(ctx);
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
        switch (ctx->cursor)
        {
        case 0U: nav_push(ctx, SCREEN_MODE_SELECT); break;
        case 1U: nav_push(ctx, SCREEN_TIME_MENU);   break;
        case 2U: nav_push(ctx, SCREEN_MINMAX_MODE); break;
        case 3U: nav_push(ctx, SCREEN_DS18B20_POS); break;
        default: break;
        }
        break;
    case RTRECD_EVENT_BUTTON_LONG:
        nav_pop(ctx); /* quay về màn hình làm việc */
        break;
    default:
        break;
    }
}

static void handle_mode_select(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CW:
        list_cw(ctx, MODE_ITEM_COUNT);
        break;
    case RTRECD_EVENT_ROTATE_CCW:
        list_ccw(ctx);
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
        ctx->active_mode = (app_mode_t)ctx->cursor;
        ctx->dirty = true;
        nav_pop(ctx);
        break;
    case RTRECD_EVENT_BUTTON_LONG:
        nav_pop(ctx);
        break;
    default:
        break;
    }
}

static void handle_time_menu(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CW:
        list_cw(ctx, TIME_ITEM_COUNT);
        break;
    case RTRECD_EVENT_ROTATE_CCW:
        list_ccw(ctx);
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
        enter_time_edit(ctx, ctx->cursor);
        break;
    case RTRECD_EVENT_BUTTON_LONG:
        nav_pop(ctx);
        break;
    default:
        break;
    }
}

static void handle_time_edit(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CW:
        edit_cw(ctx);
        break;
    case RTRECD_EVENT_ROTATE_CCW:
        edit_ccw(ctx);
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
        save_time_edit(ctx);
        nav_pop(ctx); /* xác nhận, về time_menu */
        break;
    case RTRECD_EVENT_BUTTON_LONG:
        nav_pop(ctx); /* huỷ, về time_menu */
        break;
    default:
        break;
    }
}

static void handle_minmax_mode(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CW:
        list_cw(ctx, MINMAX_MODE_COUNT);
        break;
    case RTRECD_EVENT_ROTATE_CCW:
        list_ccw(ctx);
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
        ctx->edit_mode_index = ctx->cursor;
        nav_push(ctx, SCREEN_MINMAX_PARAM);
        break;
    case RTRECD_EVENT_BUTTON_LONG:
        nav_pop(ctx);
        break;
    default:
        break;
    }
}

static void handle_minmax_param(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CW:
        list_cw(ctx, MINMAX_PARAM_COUNT);
        break;
    case RTRECD_EVENT_ROTATE_CCW:
        list_ccw(ctx);
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
        ctx->edit_param_index = ctx->cursor;
        nav_push(ctx, SCREEN_MINMAX_FIELD);
        break;
    case RTRECD_EVENT_BUTTON_LONG:
        nav_pop(ctx);
        break;
    default:
        break;
    }
}

static void handle_minmax_field(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CW:
        list_cw(ctx, 2U);
        break;
    case RTRECD_EVENT_ROTATE_CCW:
        list_ccw(ctx);
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
    {
        int32_t vmin, vmax;
        ctx->edit_field_index = ctx->cursor;
        get_minmax_range(ctx, &vmin, &vmax);
        ctx->edit_min   = vmin;
        ctx->edit_max   = vmax;
        ctx->edit_value = get_minmax_value(ctx);
        nav_push(ctx, SCREEN_MINMAX_EDIT);
        break;
    }
    case RTRECD_EVENT_BUTTON_LONG:
        nav_pop(ctx);
        break;
    default:
        break;
    }
}

static void handle_minmax_edit(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ev)
    {
    case RTRECD_EVENT_ROTATE_CW:
        edit_cw(ctx);
        break;
    case RTRECD_EVENT_ROTATE_CCW:
        edit_ccw(ctx);
        break;
    case RTRECD_EVENT_BUTTON_SHORT:
        save_minmax_value(ctx);
        nav_pop(ctx); /* xác nhận */
        break;
    case RTRECD_EVENT_BUTTON_LONG:
        nav_pop(ctx); /* huỷ      */
        break;
    default:
        break;
    }
}

static void handle_ds18b20_pos(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    /* TODO: logic cài đặt vị trí DS18B20 sẽ được thêm sau */
    switch (ev)
    {
    case RTRECD_EVENT_BUTTON_LONG:
        nav_pop(ctx);
        break;
    default:
        break;
    }
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void app_menu_init(app_menu_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->screen = SCREEN_WORK1;
    ctx->dirty  = true;

    /* Thời gian mặc định */
    ctx->time_cfg.year   = 2026U;
    ctx->time_cfg.month  = 1U;
    ctx->time_cfg.day    = 1U;
    ctx->time_cfg.hour   = 0U;
    ctx->time_cfg.minute = 0U;
    ctx->time_cfg.second = 0U;

    /* Chế độ mặc định */
    ctx->active_mode = MODE_NGHI;

    /* MinMax mặc định cho cả 4 chế độ */
    for (uint8_t i = 0U; i < 4U; i++)
    {
        ctx->mode_cfg[i].nhiet_do.min   = 150;   /* 15.0 °C  */
        ctx->mode_cfg[i].nhiet_do.max   = 350;   /* 35.0 °C  */
        ctx->mode_cfg[i].do_am.min      = 600;   /* 60.0 %RH */
        ctx->mode_cfg[i].do_am.max      = 900;   /* 90.0 %RH */
        ctx->mode_cfg[i].co2.min        = 400;   /* 400  ppm */
        ctx->mode_cfg[i].co2.max        = 1500;  /* 1500 ppm */
        ctx->mode_cfg[i].den.time_start_h = 6U;
        ctx->mode_cfg[i].den.time_start_m = 0U;
        ctx->mode_cfg[i].den.time_stop_h  = 20U;
        ctx->mode_cfg[i].den.time_stop_m  = 0U;
    }
}

void app_menu_handle_event(app_menu_ctx_t *ctx, rtrecd_queue_item_t ev)
{
    switch (ctx->screen)
    {
    case SCREEN_WORK1:        handle_work1(ctx, ev);        break;
    case SCREEN_WORK2:        handle_work2(ctx, ev);        break;
    case SCREEN_MAIN_MENU:    handle_main_menu(ctx, ev);    break;
    case SCREEN_MODE_SELECT:  handle_mode_select(ctx, ev);  break;
    case SCREEN_TIME_MENU:    handle_time_menu(ctx, ev);    break;
    case SCREEN_TIME_EDIT:    handle_time_edit(ctx, ev);    break;
    case SCREEN_MINMAX_MODE:  handle_minmax_mode(ctx, ev);  break;
    case SCREEN_MINMAX_PARAM: handle_minmax_param(ctx, ev); break;
    case SCREEN_MINMAX_FIELD: handle_minmax_field(ctx, ev); break;
    case SCREEN_MINMAX_EDIT:  handle_minmax_edit(ctx, ev);  break;
    case SCREEN_DS18B20_POS:  handle_ds18b20_pos(ctx, ev);  break;
    default: break;
    }
}

void app_menu_render(app_menu_ctx_t *ctx)
{
    if (!ctx->dirty) return;
    ctx->dirty = false;

    switch (ctx->screen)
    {
    case SCREEN_WORK1:        render_work1(ctx);        break;
    case SCREEN_WORK2:        render_work2(ctx);        break;
    case SCREEN_MAIN_MENU:    render_main_menu(ctx);    break;
    case SCREEN_MODE_SELECT:  render_mode_select(ctx);  break;
    case SCREEN_TIME_MENU:    render_time_menu(ctx);    break;
    case SCREEN_TIME_EDIT:    render_time_edit(ctx);    break;
    case SCREEN_MINMAX_MODE:  render_minmax_mode(ctx);  break;
    case SCREEN_MINMAX_PARAM: render_minmax_param(ctx); break;
    case SCREEN_MINMAX_FIELD: render_minmax_field(ctx); break;
    case SCREEN_MINMAX_EDIT:  render_minmax_edit(ctx);  break;
    case SCREEN_DS18B20_POS:  render_ds18b20_pos(ctx);  break;
    default: break;
    }
}

void app_menu_update_scd41(app_menu_ctx_t *ctx, const scd41_queue_item_t *data)
{
    ctx->scd41 = *data;
    if (ctx->screen == SCREEN_WORK1 || ctx->screen == SCREEN_WORK2)
    {
        ctx->dirty = true;
    }
}

void app_menu_update_ds18b20(app_menu_ctx_t *ctx, const Ds18b20QueueItem *data)
{
    uint8_t idx = data->sensorIndex;
    if (idx < MENU_DS18B20_MAX)
    {
        ctx->ds18b20[idx] = *data;
        if ((uint8_t)(idx + 1U) > ctx->ds18b20_count)
        {
            ctx->ds18b20_count = (uint8_t)(idx + 1U);
        }
    }
    if (ctx->screen == SCREEN_WORK1 || ctx->screen == SCREEN_WORK2)
    {
        ctx->dirty = true;
    }
}

void app_menu_mark_dirty(app_menu_ctx_t *ctx)
{
    ctx->dirty = true;
}
