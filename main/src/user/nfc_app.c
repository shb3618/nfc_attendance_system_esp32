/*
 * nfc_app.c
 *
 *  Created on: 2018-02-13 21:50
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <string.h>

#include "esp_log.h"
#include "esp_system.h"

#include "nfc/nfc.h"

#include "core/os.h"
#include "board/pn532.h"
#include "user/gui.h"
#include "user/ntp.h"
#include "user/led.h"
#include "user/audio_player.h"
#include "user/http_app_token.h"

#define TAG "nfc_app"

#define RX_FRAME_PRFX "f222222222"

#define RX_FRAME_PRFX_LEN (10)
#define RX_FRAME_DATA_LEN (32)

#define RX_FRAME_LEN (RX_FRAME_PRFX_LEN + RX_FRAME_DATA_LEN)
#define TX_FRAME_LEN (10)

static uint8_t abtRx[RX_FRAME_LEN + 1] = {0x00};
static uint8_t abtTx[TX_FRAME_LEN + 1] = {0x00, 0xA4, 0x04, 0x00, 0x05, 0xF2, 0x22, 0x22, 0x22, 0x22};

static void nfc_app_task_handle(void *pvParameter)
{
    nfc_target nt;
    nfc_device *pnd;
    nfc_context *context;
    nfc_modulation nm = {
        .nmt = NMT_ISO14443A,
        .nbr = NBR_106
    };
    portTickType xLastWakeTime;

    nfc_init(&context);
    if (context == NULL) {
        ESP_LOGE(TAG, "unable to init libnfc (malloc)");
        goto err;
    }

    while (1) {
        xEventGroupWaitBits(
            user_event_group,
            NFC_APP_RUN_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );
        xLastWakeTime = xTaskGetTickCount();
        // Open NFC device
        while ((pnd = nfc_open(context, "pn532_uart:uart1:115200")) == NULL) {
            ESP_LOGE(TAG, "device reset");
            pn532_setpin_reset(0);
            vTaskDelay(100 / portTICK_RATE_MS);
            pn532_setpin_reset(1);
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        // Transceive some bytes if target available
        int res = 0;
        memset(abtRx, 0, sizeof(abtRx));
        if (nfc_initiator_init(pnd) >= 0) {
            if (nfc_initiator_select_passive_target(pnd, nm, NULL, 0, &nt) >= 0) {
                if ((res = nfc_initiator_transceive_bytes(pnd, abtTx, TX_FRAME_LEN, abtRx, RX_FRAME_LEN, -1)) >= 0) {
                    abtRx[res] = 0x00;
                } else {
                    ESP_LOGW(TAG, "transceive failed");
                }
            } else {
                ESP_LOGI(TAG, "%u bytes mem left", heap_caps_get_free_size(MALLOC_CAP_32BIT));
            }
        } else {
            ESP_LOGE(TAG, "setup device failed");
        }
        // Close NFC device
        nfc_close(pnd);
        // Match received bytes and verify the token if available
        if (res > 0) {
            if (strstr((char *)abtRx, RX_FRAME_PRFX) != NULL &&
                strlen((char *)(abtRx + RX_FRAME_PRFX_LEN)) == RX_FRAME_DATA_LEN) {
                ESP_LOGW(TAG, "token %32s", (char *)(abtRx + RX_FRAME_PRFX_LEN));
                audio_player_play_file(0);
                http_app_verify_token((char *)(abtRx + RX_FRAME_PRFX_LEN));
            } else {
                ESP_LOGW(TAG, "unexpected frame");
            }
        }
        // Task Delay
        vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_RATE_MS);
    }

err:
    nfc_exit(context);

    ESP_LOGE(TAG, "unrecoverable error");
    esp_restart();
}

void nfc_app_set_mode(uint8_t mode)
{
    if (mode != 0) {
        pn532_setpin_reset(1);
        vTaskDelay(100 / portTICK_RATE_MS);
        xEventGroupSetBits(user_event_group, NFC_APP_RUN_BIT);
    } else {
        xEventGroupClearBits(user_event_group, NFC_APP_RUN_BIT);
        pn532_setpin_reset(0);
    }
}

void nfc_app_init(void)
{
    xTaskCreatePinnedToCore(nfc_app_task_handle, "nfcAppT", 5120, NULL, 5, NULL, 0);
}
