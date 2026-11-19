Import("env")
import os
import shutil
import hashlib

# replace_libraries.py — PlatformIO pre-script.
#
# Copies the custom ESP32 SDK libraries from custom_libs/esp32/ into the
# framework package before compilation (idempotent: hash-checked, skipped
# when already up to date). Replaces the separate replace_mbedtls.py 
#
# custom_libs/esp32/ contains:
#  - libmbedcrypto/libmbedtls/libmbedtls_2/libmbedx509 — mbedtls rebuilt with
#    error strings removed, 8K max context, dynamic TLS buffers, no session
#    tickets/renegotiation/peer-certificate retention (~15 KB internal RAM
#    saved at rest with several TLS connections).
#  - liblwip — 32-socket lwIP with the backported TCP-PCB killer.
#  - libmqtt — esp-mqtt that survives EAGAIN instead of aborting the
#    connection.
#  - sdkconfig/sdkconfig.<flashmode>.h — headers matching the patched libs
#    (32 sockets etc.); copied into all four flash-mode include dirs.
#
# A .original backup of each stock library is kept next to it (created once)
# so the framework package can always be restored by hand.

def file_hash(filepath):
    h = hashlib.md5()
    with open(filepath, 'rb') as f:
        for chunk in iter(lambda: f.read(8192), b''):
            h.update(chunk)
    return h.hexdigest()

def files_identical(file1, file2):
    if not os.path.exists(file1) or not os.path.exists(file2):
        return False
    if os.path.getsize(file1) != os.path.getsize(file2):
        return False
    return file_hash(file1) == file_hash(file2)

def copy_custom_libraries(env):
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    if not framework_dir:
        return

    custom_lib_path = os.path.join(env["PROJECT_DIR"], "custom_libs", "esp32")
    sdk_path = os.path.join(framework_dir, "tools", "sdk", "esp32")
    if not os.path.isdir(custom_lib_path):
        print(f"Warning: Custom library path not found: {custom_lib_path}")
        return

    # SDK libraries (mbedtls set + patched lwip/mqtt, whatever is present)
    for lib in ["libmbedcrypto.a", "libmbedtls.a", "libmbedtls_2.a",
                "libmbedx509.a", "liblwip.a", "libmqtt.a"]:
        src = os.path.join(custom_lib_path, lib)
        dst = os.path.join(sdk_path, "lib", lib)
        if not os.path.exists(src):
            continue  # not customized — keep the stock lib
        backup = dst + ".original"
        if not os.path.exists(backup) and os.path.exists(dst):
            shutil.copy2(dst, backup)
            print(f"Created backup: {backup}")
        if files_identical(src, dst):
            print(f"{lib} already up to date")
        else:
            shutil.copy2(src, dst)
            print(f"Replaced {lib} with custom version")

    # Patched sdkconfig headers: flash-mode variants
    cfg_src = os.path.join(custom_lib_path, "sdkconfig")
    if os.path.isdir(cfg_src):
        for variant in ["dio_qspi", "dout_qspi", "qio_qspi", "qout_qspi"]:
            s = os.path.join(cfg_src, f"sdkconfig.{variant}.h")
            d = os.path.join(sdk_path, variant, "include", "sdkconfig.h")
            if os.path.exists(s) and not files_identical(s, d):
                shutil.copy2(s, d)
                print(f"Updated sdkconfig.{variant}.h")

    # Custom mbedtls/esp_config.h — MUST match the custom mbedtls .a libs.
    # The mbedtls_ssl_context/ssl_session struct layouts depend on
    # KEEP_PEER_CERTIFICATE / RENEGOTIATION / SESSION_TICKETS, and the
    # dynamic TLS-buffer sizes depend on ASYMMETRIC_CONTENT_LEN; all apps +
    # Arduino-core objects are compiled against this header, so it has to be
    # identical to the config the libraries were built with. Without this,
    # app-side TLS (Mongoose HTTPS server, MicroOcpp) silently corrupts.
    esp_cfg_src = os.path.join(custom_lib_path, "include", "mbedtls", "esp_config.h")
    esp_cfg_dst = os.path.join(sdk_path, "include", "mbedtls", "port", "include", "mbedtls", "esp_config.h")
    if os.path.exists(esp_cfg_src):
        backup = esp_cfg_dst + ".original"
        if not os.path.exists(backup) and os.path.exists(esp_cfg_dst):
            shutil.copy2(esp_cfg_dst, backup)
            print(f"Created backup: {backup}")
        if files_identical(esp_cfg_src, esp_cfg_dst):
            print("esp_config.h already up to date")
        else:
            shutil.copy2(esp_cfg_src, esp_cfg_dst)
            print("Replaced mbedtls/esp_config.h with custom version")

# Run immediately when script is loaded (before build starts)
copy_custom_libraries(env)
