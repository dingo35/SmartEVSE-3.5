#ifndef FIRMWARE_MANAGER_H
#define FIRMWARE_MANAGER_H

#if defined(ESP32)

#include <Arduino.h>
#include "esp_ota_ops.h"

#define SIGNATURE_LENGTH 512

// Check for latest version on GitHub.
// owner_repo: format "owner/repo" (e.g. "dingo35/SmartEVSE-3.5")
// asset_name: firmware asset name (e.g. "firmware.signed.bin")
// version: output buffer for version string (min 32 bytes)
// Returns true if version was found.
bool getLatestVersion(String owner_repo, String asset_name, char *version);

// Validate firmware signature using embedded RSA public key.
// partition: the OTA partition containing the firmware
// signature: the signature bytes (SIGNATURE_LENGTH)
// size: size of the firmware data (excluding signature)
bool validate_sig(const esp_partition_t* partition, unsigned char *signature, int size);

// Download and flash firmware from URL.
// firmwareURL: download URL for the firmware binary
// validate: if true, verify signature after flashing
bool forceUpdate(const char* firmwareURL, bool validate);

// FreeRTOS task wrapper for firmware update.
void FirmwareUpdate(void *parameter);

// Launch firmware update in a background FreeRTOS task.
void RunFirmwareUpdate(void);

#endif // ESP32

#endif // FIRMWARE_MANAGER_H
