/*
 * app_settings.c
 *
 * Lưu/Load cài đặt vào Flash page 62 (0x0800F800).
 *
 * Cấu trúc Flash (packed, tổng ~76 bytes, nằm gọn trong 1KB page):
 *
 *   [uint32_t magic      ]  4 bytes  — nhận dạng dữ liệu hợp lệ
 *   [uint8_t  version    ]  1 byte   — phiên bản cấu trúc
 *   [uint8_t  active_mode]  1 byte   — chế độ đang chọn
 *   [uint8_t  _pad[2]    ]  2 bytes  — căn chỉnh 4-byte
 *   [mode_settings_t ×4  ] 64 bytes  — MinMax 4 chế độ
 *   [uint32_t checksum   ]  4 bytes  — tổng kiểm tra
 *   ─────────────────────────────────
 *   Tổng: 76 bytes
 *
 * Ghi Flash bằng HAL halfword (16-bit) để tương thích STM32F1.
 */

#include "app_settings.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stddef.h>

/* =========================================================================
 * Internal Flash data structure
 * ========================================================================= */

typedef struct __attribute__((packed))
{
    uint32_t        magic;
    uint8_t         version;
    uint8_t         active_mode;
    uint8_t         _pad[2];          /* giữ mode_cfg căn chỉnh 4-byte */
    mode_settings_t mode_cfg[4];      /* 4 chế độ × 16 bytes = 64 bytes */
    uint32_t        checksum;
} app_settings_flash_t;

/* Kiểm tra tại compile-time kích thước phải là bội của 2 (halfword write) */
_Static_assert((sizeof(app_settings_flash_t) % 2U) == 0U,
               "app_settings_flash_t size must be even for halfword Flash write");

/* =========================================================================
 * Checksum
 * ========================================================================= */

static uint32_t calc_checksum(const app_settings_flash_t *s)
{
    uint32_t sum = 0U;
    const uint8_t *p = (const uint8_t *)s;
    const size_t len = offsetof(app_settings_flash_t, checksum);
    for (size_t i = 0U; i < len; i++)
    {
        sum += (uint32_t)p[i];
    }
    return sum;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void app_settings_load(app_menu_ctx_t *ctx)
{
    const app_settings_flash_t *fs =
        (const app_settings_flash_t *)APP_SETTINGS_FLASH_ADDR;

    /* Kiểm tra magic */
    if (fs->magic != APP_SETTINGS_MAGIC)          return;
    /* Kiểm tra version */
    if (fs->version != APP_SETTINGS_VERSION)      return;
    /* Kiểm tra checksum */
    if (fs->checksum != calc_checksum(fs))        return;
    /* Kiểm tra mode hợp lệ (không tính MODE_NGHI vì không có MinMax) */
    if (fs->active_mode >= (uint8_t)MODE_COUNT)   return;

    /* Tất cả hợp lệ → nạp vào ctx */
    ctx->active_mode = (app_mode_t)fs->active_mode;
    memcpy(ctx->mode_cfg, fs->mode_cfg, sizeof(ctx->mode_cfg));
}

void app_settings_save(app_menu_ctx_t *ctx)
{
    /* Chuẩn bị dữ liệu ghi */
    app_settings_flash_t s;
    memset(&s, 0xFFU, sizeof(s));   /* Flash default state = 0xFF */

    s.magic       = APP_SETTINGS_MAGIC;
    s.version     = APP_SETTINGS_VERSION;
    s.active_mode = (uint8_t)ctx->active_mode;
    memcpy(s.mode_cfg, ctx->mode_cfg, sizeof(s.mode_cfg));
    s.checksum    = calc_checksum(&s);

    /* ---- Ghi Flash ---- */
    HAL_FLASH_Unlock();

    /* Xóa page 62 */
    FLASH_EraseInitTypeDef erase = {
        .TypeErase   = FLASH_TYPEERASE_PAGES,
        .PageAddress = APP_SETTINGS_FLASH_ADDR,
        .NbPages     = 1U,
    };
    uint32_t page_error = 0U;
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return; /* Erase thất bại, không ghi tiếp */
    }

    /* Ghi theo halfword (16-bit) — yêu cầu của STM32F1 */
    const uint16_t *src  = (const uint16_t *)&s;
    uint32_t        addr = APP_SETTINGS_FLASH_ADDR;
    const size_t    n    = sizeof(s) / 2U;

    for (size_t i = 0U; i < n; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, src[i]) != HAL_OK)
        {
            break; /* Ghi thất bại — dữ liệu không đầy đủ, checksum sẽ fail khi load */
        }
        addr += 2U;
    }

    HAL_FLASH_Lock();

    ctx->settings_dirty = false;
}
