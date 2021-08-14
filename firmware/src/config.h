#pragma once

#include <mbedtls/ssl.h>

#define MICLONE_LOG_FILENAME "/miclone.log"

#define BINARY_URL "https://raw.githubusercontent.com/cmasterx/MiOrigin/%s/firmware/binaries/%s"

#define API_URL "http://miorigin.cmasterx.com/api"

#undef  CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN
#define CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN 4096

#undef  CONFIG_MBEDTLS_ASYMMETRIC_CONTENT_LEN
#define CONFIG_MBEDTLS_ASYMMETRIC_CONTENT_LEN CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN

#undef  CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN
#define CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN

// #define CHECK_FOR_FIRMWARE_UPDATE

// #define WIFI_CONNECTIVITY_ENABLE
