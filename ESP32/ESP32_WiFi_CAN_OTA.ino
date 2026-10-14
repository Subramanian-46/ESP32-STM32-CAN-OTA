/*
 * ESP32 OTA Version 2
 *
 * Wi-Fi firmware download + STM32 OTA-over-CAN
 *
 * Flow:
 *
 * PC
 *  |
 * | firmware.bin
 * | HTTP
 * v
 * ESP32
 *  |
 * | firmware_buffer[]
 * |
 * | CAN / TWAI
 * v
 * STM32G0B1RET6
 *
 *
 * CAN protocol:
 *
 * CAN_ID_DOWNLINK  0x200   ESP32 -> STM32
 * CAN_ID_UPLINK    0x201   STM32 -> ESP32
 *
 * Frame layout:
 *
 * [cmd][seq_hi][seq_lo][payload up to 5 bytes]
 *
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/twai.h>
#include <string.h>


// ============================================================
// WIFI CONFIGURATION
// ============================================================

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// Your laptop IP address
// Python server:
// S:\Download\ESP32_OTA
//
// Server command:
// python -m http.server 8000
//
const char* FIRMWARE_URL =
    "http://:8000/firmware.bin";


// ============================================================
// CAN PIN CONFIGURATION
// ============================================================

#define CAN_TX_PIN GPIO_NUM_17
#define CAN_RX_PIN GPIO_NUM_16


// ============================================================
// CAN PROTOCOL CONSTANTS
// ============================================================

#define CAN_ID_DOWNLINK   0x200
#define CAN_ID_UPLINK     0x201

#define CMD_ENTER_UPDATE  0x01
#define CMD_ACK           0x02
#define CMD_FW_INFO       0x03
#define CMD_DATA          0x04
#define CMD_DATA_ACK      0x05
#define CMD_DATA_NACK     0x06
#define CMD_TRANSFER_DONE 0x07
#define CMD_CRC_RESULT    0x08
#define CMD_REBOOT        0x09


// ============================================================
// OTA CONFIGURATION
// ============================================================

#define MAX_RETRIES       3
#define ACK_TIMEOUT_MS    1000

// Firmware version
const uint16_t firmware_version = 0x0001;


// ============================================================
// DOWNLOADED FIRMWARE BUFFER
// ============================================================
//
// This replaces the old:
//
// uint8_t dummy_firmware[64];
//
// The firmware downloaded from the PC will be stored here.
//
uint8_t* firmware_buffer = nullptr;

uint32_t firmware_size = 0;


// ============================================================
// CRC32
// ============================================================

uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int b = 0; b < 8; b++)
        {
            crc = (crc & 1)
                ? (crc >> 1) ^ 0xEDB88320
                : (crc >> 1);
        }
    }

    return ~crc;
}


// ============================================================
// CONNECT TO WIFI
// ============================================================

bool connectWiFi()
{
    Serial.println();
    Serial.println("======================================");
    Serial.println("Connecting to Wi-Fi");
    Serial.println("======================================");

    WiFi.mode(WIFI_STA);

    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        Serial.print(".");

        attempts++;

        if (attempts >= 60)
        {
            Serial.println();
            Serial.println("ERROR: Wi-Fi connection timeout!");

            return false;
        }
    }

    Serial.println();
    Serial.println();

    Serial.println("Wi-Fi connected!");

    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());

    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    return true;
}


// ============================================================
// DOWNLOAD FIRMWARE FROM PC
// ============================================================

bool downloadFirmware()
{
    Serial.println();
    Serial.println("======================================");
    Serial.println("FIRMWARE DOWNLOAD");
    Serial.println("======================================");
    Serial.print("URL: ");
    Serial.println(FIRMWARE_URL);

    HTTPClient http;
    if (!http.begin(FIRMWARE_URL)) {
        Serial.println("ERROR: HTTP connection could not be started!");
        return false;
    }
    http.setTimeout(10000);
    Serial.println();
    Serial.println("Connecting to firmware server...");

    int httpCode = http.GET();
    Serial.print("HTTP response code: ");
    Serial.println(httpCode);

    if (httpCode != HTTP_CODE_OK) {
        Serial.println("ERROR: HTTP download failed!");
        if (httpCode > 0) {
            Serial.print("HTTP error: ");
            Serial.println(http.errorToString(httpCode));
        }
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    Serial.print("Firmware size: ");
    Serial.print(contentLength);
    Serial.println(" bytes");
    if (contentLength <= 0) {
        Serial.println("ERROR: Invalid firmware size!");
        http.end();
        return false;
    }

    firmware_size = (uint32_t)contentLength;
    Serial.println();
    Serial.println("Allocating ESP32 RAM...");
    firmware_buffer = (uint8_t*)malloc(firmware_size);
    if (firmware_buffer == nullptr) {
        Serial.println("ERROR: Not enough ESP32 RAM!");
        firmware_size = 0;
        http.end();
        return false;
    }
    Serial.print("RAM allocated: ");
    Serial.print(firmware_size);
    Serial.println(" bytes");

    WiFiClient* stream = http.getStreamPtr();
    uint32_t total_received = 0;
    int lastPercentage = -1;

    Serial.println();
    Serial.println("Downloading firmware...");
    Serial.println();
    Serial.println("Download Progress: 0%");

    while (total_received < firmware_size) {
        size_t available_bytes = stream->available();
        if (available_bytes > 0) {
            uint32_t remaining = firmware_size - total_received;
            size_t bytes_to_read = (available_bytes < remaining) ? available_bytes : remaining;
            size_t bytes_read = stream->readBytes(firmware_buffer + total_received, bytes_to_read);
            if (bytes_read > 0) {
                total_received += bytes_read;
                int percentage = (int)(((uint64_t)total_received * 100) / firmware_size);
                if (percentage >= lastPercentage + 5) {
                    lastPercentage = percentage;
                    Serial.printf("Download Progress: %d%%  (%lu / %lu bytes)\n", percentage, (unsigned long)total_received, (unsigned long)firmware_size);
                }
            }
        }
        delay(1);
    }

    if (lastPercentage < 100)
        Serial.println("Download Progress: 100%  (download complete)");

    Serial.println();
    Serial.println("Firmware download complete!");
    http.end();

    if (total_received != firmware_size) {
        Serial.println("ERROR: Firmware size mismatch!");
        free(firmware_buffer);
        firmware_buffer = nullptr;
        firmware_size = 0;
        return false;
    }

    Serial.print("Total bytes received: ");
    Serial.println(total_received);
    Serial.println();
    Serial.println("======================================");
    Serial.println("WIFI FIRMWARE DOWNLOAD SUCCESS!");
    Serial.println("======================================");
    Serial.println();
    Serial.println("Firmware is now stored in ESP32 RAM.");
    Serial.println();
    Serial.println("CAN OTA has NOT been started yet.");
    Serial.println();
    return true;
}


// ============================================================
// DISPLAY DOWNLOADED FIRMWARE INFORMATION
// ============================================================

void showFirmwareInformation()
{
    Serial.println();
    Serial.println("======================================");
    Serial.println("FIRMWARE INFORMATION");
    Serial.println("======================================");

    Serial.print("Firmware size: ");
    Serial.print(firmware_size);
    Serial.println(" bytes");


    // Calculate CRC

    uint32_t crc =
        crc32_calc(
            firmware_buffer,
            firmware_size
        );

    Serial.print("CRC32: 0x");

    Serial.printf(
        "%08lX",
        (unsigned long)crc
    );

    Serial.println();


    // ========================================================
    // FIRST 16 BYTES
    // ========================================================

    Serial.println();
    Serial.println("First 16 bytes:");

    uint32_t firstCount =
        (firmware_size < 16)
        ? firmware_size
        : 16;

    for (uint32_t i = 0; i < firstCount; i++)
    {
        Serial.printf(
            "%02X ",
            firmware_buffer[i]
        );
    }

    Serial.println();


    // ========================================================
    // LAST 16 BYTES
    // ========================================================

    Serial.println();
    Serial.println("Last 16 bytes:");

    uint32_t start =
        (firmware_size > 16)
        ? firmware_size - 16
        : 0;

    for (uint32_t i = start;
         i < firmware_size;
         i++)
    {
        Serial.printf(
            "%02X ",
            firmware_buffer[i]
        );
    }

    Serial.println();
}


// ============================================================
// TWAI / CAN SETUP
// ============================================================

void twai_setup()
{
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(
            CAN_TX_PIN,
            CAN_RX_PIN,
            TWAI_MODE_NORMAL
        );

    twai_timing_config_t t_config =
        TWAI_TIMING_CONFIG_500KBITS();

    twai_filter_config_t f_config =
        TWAI_FILTER_CONFIG_ACCEPT_ALL();


    // Install driver

    if (twai_driver_install(
            &g_config,
            &t_config,
            &f_config
        ) != ESP_OK)
    {
        Serial.println(
            "TWAI driver install failed"
        );

        while (1)
        {
            delay(1000);
        }
    }


    // Start driver

    if (twai_start() != ESP_OK)
    {
        Serial.println(
            "TWAI start failed"
        );

        while (1)
        {
            delay(1000);
        }
    }

    Serial.println();
    Serial.println(
        "TWAI started "
        "(500 kbit/s, Classic CAN)"
    );
}


// ============================================================
// SEND CAN FRAME
// ============================================================
//
// Frame:
//
// Byte 0 = command
// Byte 1 = sequence high
// Byte 2 = sequence low
// Byte 3-7 = payload
//
// Maximum payload = 5 bytes
//

void sendFrame(
    uint8_t cmd,
    uint16_t seq,
    const uint8_t *payload,
    uint8_t len
)
{
    twai_message_t tx = {0};

    tx.identifier =
        CAN_ID_DOWNLINK;

    tx.extd = 0;

    tx.data_length_code = 8;


    // Command

    tx.data[0] = cmd;


    // Sequence number

    tx.data[1] =
        (seq >> 8) & 0xFF;

    tx.data[2] =
        seq & 0xFF;


    // Payload

    if (payload != NULL && len > 0)
    {
        if (len > 5)
        {
            len = 5;
        }

        memcpy(
            &tx.data[3],
            payload,
            len
        );
    }


    // Transmit

    twai_transmit(
        &tx,
        pdMS_TO_TICKS(ACK_TIMEOUT_MS)
    );
}


// ============================================================
// SEND COMMAND AND WAIT FOR ACK
// ============================================================

bool sendAndWaitAck(
    uint8_t cmd,
    uint16_t seq,
    const uint8_t *payload,
    uint8_t len,
    uint8_t expectedAckCmd,
    uint8_t *ackPayloadOut
)
{
    for (
        int attempt = 0;
        attempt < MAX_RETRIES;
        attempt++
    )
    {
        // Send command

        sendFrame(
            cmd,
            seq,
            payload,
            len
        );


        // Wait for response

        twai_message_t rx;

        uint32_t deadline =
            millis() + ACK_TIMEOUT_MS;


        while (millis() < deadline)
        {
            if (
                twai_receive(
                    &rx,
                    pdMS_TO_TICKS(100)
                ) == ESP_OK
            )
            {
                if (
                    rx.identifier ==
                    CAN_ID_UPLINK &&
                    rx.data[0] ==
                    expectedAckCmd
                )
                {
                    if (ackPayloadOut != NULL)
                    {
                        memcpy(
                            ackPayloadOut,
                            &rx.data[3],
                            5
                        );
                    }

                    return true;
                }

                // Ignore unrelated frames
            }
        }


        Serial.printf(
            "Timeout waiting for ack 0x%02X, retry %d\n",
            expectedAckCmd,
            attempt + 1
        );
    }


    return false;
}


// ============================================================
// RUN OTA TRANSFER
// ============================================================
//
// This function is almost identical to your original code.
//
// The ONLY major change is:
//
// dummy_firmware[]
//
// has been replaced by:
//
// firmware_buffer[]
//
// and:
//
// dummy_fw_size
//
// has been replaced by:
//
// firmware_size
//
// ============================================================

bool runOtaTransfer()
{
    uint16_t seq = 0;

    Serial.println();
    Serial.println("======================================");
    Serial.println("CAN OTA STATUS");
    Serial.println("======================================");
    Serial.println();
    Serial.println("CAN OTA STARTED");
    Serial.println();

    // ========================================================
    // 1. ENTER UPDATE
    // ========================================================

    Serial.println("STEP 1: ENTER UPDATE");
    Serial.println("--------------------");
    Serial.println("Sending ENTER_UPDATE...");

    if (
        !sendAndWaitAck(
            CMD_ENTER_UPDATE,
            seq++,
            NULL,
            0,
            CMD_ACK,
            NULL
        )
    )
    {
        Serial.println(
            "ENTER_UPDATE failed"
        );

        twai_status_info_t status;
        twai_get_status_info(&status);

        Serial.printf(
            "TWAI state=%d, TEC=%lu, REC=%lu, msgs_to_tx=%lu\n",
            status.state,
            status.tx_error_counter,
            status.rx_error_counter,
            status.msgs_to_tx
        );

        return false;
    }

    Serial.println(
        "ENTER_UPDATE ACK received."
    );


    // ========================================================
    // 2. FW INFO
    // ========================================================
    //
    // Payload:
    //
    // Byte 0 = firmware size MSB
    // Byte 1
    // Byte 2
    // Byte 3 = firmware size LSB
    // Byte 4 = firmware version low byte
    //

    uint8_t infoPayload[5];


    infoPayload[0] =
        (firmware_size >> 24) & 0xFF;

    infoPayload[1] =
        (firmware_size >> 16) & 0xFF;

    infoPayload[2] =
        (firmware_size >> 8) & 0xFF;

    infoPayload[3] =
        firmware_size & 0xFF;

    infoPayload[4] =
        firmware_version & 0xFF;


    Serial.println();
    Serial.println("Sending FW_INFO...");

    Serial.print("Firmware size = ");
    Serial.println(firmware_size);

    Serial.print("Firmware version = 0x");
    Serial.printf(
        "%04X\n",
        firmware_version
    );


    if (
        !sendAndWaitAck(
            CMD_FW_INFO,
            seq++,
            infoPayload,
            5,
            CMD_ACK,
            NULL
        )
    )
    {
        Serial.println(
            "FW_INFO failed"
        );

        return false;
    }

    Serial.println(
        "FW_INFO ACK received."
    );


    // ========================================================
    // 3. SEND FIRMWARE DATA
    // ========================================================
    //
    // 5 bytes per CAN frame.
    //
    // firmware_buffer[] is the firmware downloaded
    // from the PC.
    //

    uint32_t offset = 0;

    uint16_t dataSeq = 0;


    Serial.println();
    Serial.println(
        "Sending firmware data..."
    );


    while (offset < firmware_size)
    {
        // Calculate remaining bytes

        uint32_t remaining =
            firmware_size - offset;


        // Maximum CAN payload = 5 bytes

        uint8_t chunkLen =
            (remaining < 5)
            ? remaining
            : 5;


        // Send this chunk

        if (
            !sendAndWaitAck(
                CMD_DATA,
                dataSeq,
                &firmware_buffer[offset],
                chunkLen,
                CMD_DATA_ACK,
                NULL
            )
        )
        {
            Serial.printf(
                "DATA chunk seq %u failed\n",
                dataSeq
            );

            return false;
        }


        // Move to next chunk

        offset += chunkLen;

        dataSeq++;


        // Progress display every 5%

        static int lastCanPercentage = -1;
        int canPercentage = (int)(((uint64_t)offset * 100) / firmware_size);

        if (canPercentage >= lastCanPercentage + 5)
        {
            lastCanPercentage = canPercentage;
            Serial.printf(
                "CAN OTA Progress: %d%%  (%lu / %lu bytes)\n",
                canPercentage,
                (unsigned long)offset,
                (unsigned long)firmware_size
            );
        }
    }


    Serial.println();

    Serial.printf(
        "All %lu bytes sent.\n",
        (unsigned long)firmware_size
    );


    // ========================================================
    // 4. CALCULATE CRC32
    // ========================================================

    uint32_t crc =
        crc32_calc(
            firmware_buffer,
            firmware_size
        );


    Serial.print(
        "Firmware CRC32 = 0x"
    );

    Serial.printf(
        "%08lX\n",
        (unsigned long)crc
    );


    // ========================================================
    // 5. SEND TRANSFER DONE
    // ========================================================

    uint8_t crcPayload[4];


    crcPayload[0] =
        (crc >> 24) & 0xFF;

    crcPayload[1] =
        (crc >> 16) & 0xFF;

    crcPayload[2] =
        (crc >> 8) & 0xFF;

    crcPayload[3] =
        crc & 0xFF;


    uint8_t ackPayload[5] = {0};


    Serial.println();
    Serial.println(
        "Sending TRANSFER_DONE..."
    );


    if (
        !sendAndWaitAck(
            CMD_TRANSFER_DONE,
            seq++,
            crcPayload,
            4,
            CMD_CRC_RESULT,
            ackPayload
        )
    )
    {
        Serial.println(
            "TRANSFER_DONE failed / "
            "no CRC_RESULT"
        );

        return false;
    }


    // ========================================================
    // 6. CHECK STM32 CRC RESULT
    // ========================================================

    if (ackPayload[0] == 1)
    {
        Serial.println();
        Serial.println(
            "STM32 reports CRC OK - "
            "update accepted."
        );

        return true;
    }
    else
    {
        Serial.println();
        Serial.println(
            "STM32 reports CRC FAIL - "
            "update rejected."
        );

        return false;
    }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);


    Serial.println();
    Serial.println();
    Serial.println(
        "======================================"
    );

    Serial.println(
        "ESP32 OTA Version 2"
    );

    Serial.println(
        "Wi-Fi -> RAM -> CAN -> STM32"
    );

    Serial.println(
        "======================================"
    );


    // ========================================================
    // STEP 1: CONNECT TO WIFI
    // ========================================================

    if (!connectWiFi())
    {
        Serial.println();
        Serial.println(
            "Wi-Fi connection failed."
        );

        return;
    }


    // ========================================================
    // STEP 2: DOWNLOAD FIRMWARE
    // ========================================================

    if (!downloadFirmware())
    {
        Serial.println();
        Serial.println(
            "Firmware download FAILED."
        );

        return;
    }


    // ========================================================
    // STEP 3: SHOW FIRMWARE INFORMATION
    // ========================================================

    showFirmwareInformation();


    // ========================================================
    // STEP 4: START CAN
    // ========================================================

    twai_setup();


    Serial.println();
    Serial.println(
        "======================================"
    );

    Serial.println(
        "Wi-Fi download successful."
    );

    Serial.println(
        "Firmware is stored in ESP32 RAM."
    );

    Serial.println(
        "CAN OTA is ready."
    );

    Serial.println(
        "======================================"
    );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // Do not start OTA unless firmware was downloaded.

    if (
        firmware_buffer == nullptr ||
        firmware_size == 0
    )
    {
        Serial.println(
            "No firmware available."
        );

        delay(5000);

        return;
    }


    Serial.println();
    Serial.println(
        "--- Starting OTA transfer ---"
    );


    bool ok =
        runOtaTransfer();


    if (ok)
    {
        Serial.println();
        Serial.println(
            "======================================"
        );

        Serial.println(
            "=== OTA TRANSFER SUCCESS ==="
        );

        Serial.println(
            "======================================"
        );
    }
    else
    {
        Serial.println();
        Serial.println(
            "======================================"
        );

        Serial.println(
            "=== OTA TRANSFER FAILED ==="
        );

        Serial.println(
            "======================================"
        );
    }


    delay(5000);
}
