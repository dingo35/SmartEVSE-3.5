# Custom ESP32 SDK libraries

`replace_libraries.py` copies this folder into the PlatformIO espressif32
framework package before each build (hash-checked).

## esp32/

- `libmbedtls_2.a` — mbedTLS SSL with Espressif's dynamic RX/TX buffers.
  At rest a connection holds an idle buffer (~48 B), not the 8K/4K or 16K/16K
  static buffers. Record buffers are sized to the handshake message or the
  `mbedtls_ssl_write` length, then freed. Built against the same
  `CONFIG_MBEDTLS_*` flags as the app (content max 16384). Do not pass
  `MBEDTLS_SSL_OUT_BUFFER_LEN` into `esp_mbedtls_add_tx_buffer`: that function
  adds record overhead itself, and a 17 KB internal alloc fails on this board
  (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096`, mbedtls forced to internal RAM).
- `libmbedcrypto.a`, `libmbedx509.a`, `libmbedtls.a` — size-reduced crypto/x509
  set (error strings removed). Struct layout matches the stock `esp_config.h`.
- `liblwip.a` — 32-socket lwIP with the TCP-PCB killer.
- `libmqtt.a` — esp-mqtt that survives `EAGAIN` instead of aborting.
- `sdkconfig/*.h` — headers matching the patched libs (32 sockets). Copied over
  the framework `sdkconfig.h` for every flash mode.
  
