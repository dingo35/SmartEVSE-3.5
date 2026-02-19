/*
 * firmware_manager.cpp — OTA firmware update logic
 *
 * Extracted from network_common.cpp to isolate security-sensitive
 * firmware update and signature validation code into its own module.
 */

#if defined(ESP32)

#include <WiFi.h>
#include "mbedtls/md_internal.h"
#include "utils.h"
#include "network_common.h"

#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "esp_ota_ops.h"

#include "firmware_manager.h"

#ifndef SENSORBOX_VERSION
#include "esp32.h"
#endif

// Globals shared with network_common.cpp
extern char *downloadUrl;
extern int downloadProgress;
extern int downloadSize;
extern bool shouldReboot;
extern const char* root_ca_github;

static unsigned char *signature = NULL;
#define SIGNATURE_LENGTH 512

// get version nr. of latest release of off github
// input:
// owner_repo format: dingo35/SmartEVSE-3.5
// asset name format: one of firmware.bin, firmware.debug.bin, firmware.signed.bin, firmware.debug.signed.bin
// output:
// version -- null terminated string with latest version of this repo
// downloadUrl -- global pointer to null terminated string with the url where this version can be downloaded
bool getLatestVersion(String owner_repo, String asset_name, char *version) {
    HTTPClient httpClient;
    String useURL = "https://api.github.com/repos/" + owner_repo + "/releases/latest";
    httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    const char* url = useURL.c_str();
    _LOG_A("Connecting to: %s.\n", url );
    if( String(url).startsWith("https") ) {
        httpClient.begin(url, root_ca_github);
    } else {
        httpClient.begin(url);
    }
    httpClient.addHeader("User-Agent", "SmartEVSE-v3");
    httpClient.addHeader("Accept", "application/vnd.github+json");
    httpClient.addHeader("X-GitHub-Api-Version", "2022-11-28" );
    const char* get_headers[] = { "Content-Length", "Content-type", "Accept-Ranges" };
    httpClient.collectHeaders( get_headers, sizeof(get_headers)/sizeof(const char*) );
    int httpCode = httpClient.GET();  //Make the request

    // only handle 200/301, fail on everything else
    if( httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY ) {
        // This error may be a false positive or a consequence of the network being disconnected.
        // Since the network is controlled from outside this class, only significant error messages are reported.
        _LOG_A("Error on HTTP request (httpCode=%i)\n", httpCode);
        httpClient.end();
        return false;
    }
    // The filter: it contains "true" for each value we want to keep
    DynamicJsonDocument  filter(100);
    filter["tag_name"] = true;
    filter["assets"][0]["browser_download_url"] = true;
    filter["assets"][0]["name"] = true;

    // Deserialize the document
    DynamicJsonDocument doc2(1500);
    DeserializationError error = deserializeJson(doc2, httpClient.getStream(), DeserializationOption::Filter(filter));

    if (error) {
        _LOG_A("deserializeJson() failed: %s\n", error.c_str());
        httpClient.end();  // We're done with HTTP - free the resources
        return false;
    }
    const char* tag_name = doc2["tag_name"]; // "v3.6.1"
    if (!tag_name) {
        //no version found
        _LOG_A("ERROR: LatestVersion of repo %s not found.\n", owner_repo.c_str());
        httpClient.end();  // We're done with HTTP - free the resources
        return false;
    }
    else
        //duplicate value so it won't get lost out of scope
        strlcpy(version, tag_name, 32);
        //strlcpy(version, tag_name, sizeof(version));
    _LOG_V("Found latest version:%s.\n", version);

    httpClient.end();  // We're done with HTTP - free the resources
    return true;
}


// SHA-Verify the OTA partition after it's been written
// https://techtutorialsx.com/2018/05/10/esp32-arduino-mbed-tls-using-the-sha-256-algorithm/
// https://github.com/ARMmbed/mbedtls/blob/development/programs/pkey/rsa_verify.c
bool validate_sig( const esp_partition_t* partition, unsigned char *signature, int size )
{
    const char* rsa_key_pub = R"RSA_KEY_PUB(
-----BEGIN PUBLIC KEY-----
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAtjEWhkfKPAUrtX1GueYq
JmDp4qSHBG6ndwikAHvteKgWQABDpwaemZdxh7xVCuEdjEkaecinNOZ0LpSCF3QO
qflnXkvpYVxjdTpKBxo7vP5QEa3I6keJfwpoMzGuT8XOK7id6FHJhtYEXcaufALi
mR/NXT11ikHLtluATymPdoSscMiwry0qX03yIek91lDypBNl5uvD2jxn9smlijfq
9j0lwtpLBWJPU8vsU0uzuj7Qq5pWZFKsjiNWfbvNJXuLsupOazf5sh0yeQzL1CBL
RUsBlYVoChTmSOyvi6kO5vW/6GLOafJF0FTdOQ+Gf3/IB6M1ErSxlqxQhHq0pb7Y
INl7+aFCmlRjyLlMjb8xdtuedlZKv8mLd37AyPAihrq9gV74xq6c7w2y+h9213p8
jgcmo/HvOlGaXEIOVCUu102teOckXjTni2yhEtFISCaWuaIdb5P9e0uBIy1e+Bi6
/7A3aut5MQP07DO99BFETXyFF6EixhTF8fpwVZ5vXeIDvKKEDUGuzAziUEGIZpic
UQ2fmTzIaTBbNlCMeTQFIpZCosM947aGKNBp672wdf996SRwg9E2VWzW2Z1UuwWV
BPVQkHb1Hsy7C9fg5JcLKB9zEfyUH0Tm9Iur1vsuA5++JNl2+T55192wqyF0R9sb
YtSTUJNSiSwqWt1m0FLOJD0CAwEAAQ==
-----END PUBLIC KEY-----
)RSA_KEY_PUB";

    if( !partition ) {
        _LOG_A( "Could not find update partition!.\n");
        return false;
    }
    _LOG_D("Creating mbedtls context.\n");
    mbedtls_pk_context pk;
    mbedtls_md_context_t rsa;
    mbedtls_pk_init( &pk );
    _LOG_D("Parsing public key.\n");

    int ret;
    if( ( ret = mbedtls_pk_parse_public_key( &pk, (const unsigned char*)rsa_key_pub, strlen(rsa_key_pub)+1 ) ) != 0 ) {
        _LOG_A( "Parsing public key failed! mbedtls_pk_parse_public_key %d (%d bytes)\n%s", ret, strlen(rsa_key_pub)+1, rsa_key_pub);
        return false;
    }
    if( !mbedtls_pk_can_do( &pk, MBEDTLS_PK_RSA ) ) {
        _LOG_A( "Public key is not an rsa key -0x%x", -ret );
        return false;
    }
    _LOG_D("Initing mbedtls.\n");
    const mbedtls_md_info_t *mdinfo = mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 );
    mbedtls_md_init( &rsa );
    mbedtls_md_setup( &rsa, mdinfo, 0 );
    mbedtls_md_starts( &rsa );
    int bytestoread = SPI_FLASH_SEC_SIZE;
    int bytesread = 0;
    uint8_t *_buffer = (uint8_t*)malloc(SPI_FLASH_SEC_SIZE);
    if(!_buffer){
        _LOG_A( "malloc failed.\n");
        return false;
    }
    _LOG_D("Parsing content.\n");
    _LOG_V( "Reading partition (%i sectors, sec_size: %i)", size, bytestoread );
    while( bytestoread > 0 ) {
        _LOG_V( "Left: %i (%i)               \r", size, bytestoread );

        if( ESP.partitionRead( partition, bytesread, (uint32_t*)_buffer, bytestoread ) ) {
            mbedtls_md_update( &rsa, (uint8_t*)_buffer, bytestoread );
            bytesread = bytesread + bytestoread;
            size = size - bytestoread;
            if( size <= SPI_FLASH_SEC_SIZE ) {
                bytestoread = size;
            }
        } else {
            _LOG_A( "partitionRead failed!.\n");
            return false;
        }
    }
    free( _buffer );

    unsigned char *hash = (unsigned char*)malloc( mdinfo->size );
    if(!hash){
        _LOG_A( "malloc failed.\n");
        return false;
    }
    mbedtls_md_finish( &rsa, hash );
    ret = mbedtls_pk_verify( &pk, MBEDTLS_MD_SHA256, hash, mdinfo->size, (unsigned char*)signature, SIGNATURE_LENGTH );
    free( hash );
    mbedtls_md_free( &rsa );
    mbedtls_pk_free( &pk );
    if( ret == 0 ) {
        return true;
    }

    // validation failed, overwrite the first few bytes so this partition won't boot!
    log_w( "Validation failed, erasing the invalid partition.\n");
    ESP.partitionEraseRange( partition, 0, ENCRYPTED_BLOCK_SIZE);
    return false;
}


bool forceUpdate(const char* firmwareURL, bool validate) {
    HTTPClient httpClient;
    //WiFiClientSecure _client;
    int partition = U_FLASH;

    httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    _LOG_A("Connecting to: %s.\n", firmwareURL );
    if( String(firmwareURL).startsWith("https") ) {
        //_client.setCACert(root_ca_github); // OR
        //_client.setInsecure(); //not working for github
        httpClient.begin(firmwareURL, root_ca_github);
    } else {
        httpClient.begin(firmwareURL);
    }
    httpClient.addHeader("User-Agent", "SmartEVSE-v3");
    httpClient.addHeader("Accept", "application/vnd.github+json");
    httpClient.addHeader("X-GitHub-Api-Version", "2022-11-28" );
    const char* get_headers[] = { "Content-Length", "Content-type", "Accept-Ranges" };
    httpClient.collectHeaders( get_headers, sizeof(get_headers)/sizeof(const char*) );

    int updateSize = 0;
    int httpCode = httpClient.GET();
    String contentType;

    if( httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY ) {
        updateSize = httpClient.getSize();
        contentType = httpClient.header( "Content-type" );
        String acceptRange = httpClient.header( "Accept-Ranges" );
        if( acceptRange == "bytes" ) {
            _LOG_V("This server supports resume!\n");
        } else {
            _LOG_V("This server does not support resume!\n");
        }
    } else {
        _LOG_A("ERROR: Server responded with HTTP Status %i.\n", httpCode );
        return false;
    }

    _LOG_D("updateSize : %i, contentType: %s.\n", updateSize, contentType.c_str());
    Stream * stream = httpClient.getStreamPtr();
    if( updateSize<=0 || stream == nullptr ) {
        _LOG_A("HTTP Error.\n");
        return false;
    }

    // some network streams (e.g. Ethernet) can be laggy and need to 'breathe'
    if( ! stream->available() ) {
        uint32_t timeout = millis() + 10000;
        while( ! stream->available() ) {
            if( millis()>timeout ) {
                _LOG_A("Stream timed out.\n");
                return false;
            }
            vTaskDelay(1);
        }
    }

    if( validate ) {
        if( updateSize == UPDATE_SIZE_UNKNOWN || updateSize <= SIGNATURE_LENGTH ) {
            _LOG_A("Malformed signature+fw combo.\n");
            return false;
        }
        updateSize -= SIGNATURE_LENGTH;
    }

    if( !Update.begin(updateSize, partition) ) {
        _LOG_A("ERROR Not enough space to begin OTA, partition size mismatch? Update failed!\n");
        Update.abort();
        return false;
    }

    Update.onProgress( [](uint32_t progress, uint32_t size) {
      _LOG_V("Firmware update progress %i/%i.\n", progress, size);
      //move this data to global var
      downloadProgress = progress;
      downloadSize = size;
      //give background tasks some air
      //vTaskDelay(100 / portTICK_PERIOD_MS);
    });

    // read signature
    if( validate ) {
        signature = (unsigned char *) malloc(SIGNATURE_LENGTH);                       //tried to free in in all exit scenarios, RISK of leakage!!!
        stream->readBytes( signature, SIGNATURE_LENGTH );
    }

    _LOG_I("Begin %s OTA. This may take 2 - 5 mins to complete. Things might be quiet for a while.. Patience!\n", partition==U_FLASH?"Firmware":"Filesystem");

    // Some activity may appear in the Serial monitor during the update (depends on Update.onProgress)
    int written = Update.writeStream(*stream);                                 // although writeStream returns size_t, we don't expect >2Gb

    if ( written == updateSize ) {
        _LOG_D("Written : %d successfully", written);
        updateSize = written; // flatten value to prevent overflow when checking signature
    } else {
        _LOG_A("Written only : %u/%u Premature end of stream?", written, updateSize);
        Update.abort();
        FREE(signature);
        return false;
    }

    if (!Update.end()) {
        _LOG_A("An Update Error Occurred. Error #: %d", Update.getError());
        FREE(signature);
        return false;
    }

    if( validate ) { // check signature
        _LOG_I("Checking partition %d to validate", partition);

        //getPartition( partition ); // updated partition => '_target_partition' pointer
        const esp_partition_t* _target_partition = esp_ota_get_next_update_partition(NULL);

        #define CHECK_SIG_ERROR_PARTITION_NOT_FOUND -1
        #define CHECK_SIG_ERROR_VALIDATION_FAILED   -2

        if( !_target_partition ) {
            _LOG_A("Can't access partition #%d to check signature!", partition);
            FREE(signature);
            return false;
        }

        _LOG_D("Checking signature for partition %d...", partition);

        const esp_partition_t* running_partition = esp_ota_get_running_partition();

        if( partition == U_FLASH ) {
            // /!\ An OTA partition is automatically set as bootable after being successfully
            // flashed by the Update library.
            // Since we want to validate before enabling the partition, we need to cancel that
            // by temporarily reassigning the bootable flag to the running-partition instead
            // of the next-partition.
            esp_ota_set_boot_partition( running_partition );
            // By doing so the ESP will NOT boot any unvalidated partition should a reset occur
            // during signature validation (crash, oom, power failure).
        }

        if( !validate_sig( _target_partition, signature, updateSize ) ) {
            FREE(signature);
            // erase partition
            esp_partition_erase_range( _target_partition, _target_partition->address, _target_partition->size );
            _LOG_A("Signature check failed!.\n");
            return false;
        } else {
            FREE(signature);
            _LOG_D("Signature check successful!.\n");
            if( partition == U_FLASH ) {
                // Set updated partition as bootable now that it's been verified
                esp_ota_set_boot_partition( _target_partition );
            }
        }
    }
    _LOG_D("OTA Update complete!.\n");
    if (Update.isFinished()) {
        _LOG_V("Update succesfully completed at %s partition\n", partition==U_SPIFFS ? "spiffs" : "firmware" );
        return true;
    } else {
        _LOG_A("ERROR: Update not finished! Something went wrong!.\n");
    }
    return false;
}


// put firmware update in separate task so we can feed progress to the html page
void FirmwareUpdate(void *parameter) {
    //_LOG_A("DINGO: url=%s.\n", downloadUrl);
    if (forceUpdate(downloadUrl, 1)) {
#ifndef SENSORBOX_VERSION
        _LOG_A("Firmware update succesfull; rebooting as soon as no EV is charging.\n");
#else
        _LOG_A("Firmware update succesfull; rebooting.\n");
#endif
        downloadProgress = -1;
        shouldReboot = true;
    } else {
        _LOG_A("ERROR: Firmware update failed.\n");
        //_http.end();
        downloadProgress = -2;
    }
    if (downloadUrl) free(downloadUrl);
    vTaskDelete(NULL);                                                        //end this task so it will not take up resources
}

void RunFirmwareUpdate(void) {
    _LOG_V("Starting firmware update from downloadUrl=%s.\n", downloadUrl);
    downloadProgress = 0;                                                       // clear errors, if any
    xTaskCreate(
        FirmwareUpdate, // Function that should be called
        "FirmwareUpdate",// Name of the task (for debugging)
        4096,           // Stack size (bytes)
        NULL,           // Parameter to pass
        3,              // Task priority - medium
        NULL            // Task handle
    );
}

#endif // ESP32
