MBEDTLS_DIR	:= $(TOP)/components/thirdparty/mbedtls
CFLAGS_INC +=  -I $(MBEDTLS_DIR)/include  \
                  -I $(MBEDTLS_DIR)/include/mbedtls  \
				  -I $(MBEDTLS_DIR)/include/psa \
                  -I $(MBEDTLS_DIR)/configs

MBEDTLS_CFLAGS ?= -DMBEDTLS_CONFIG_FILE=\"config_ec_ssl_comm.h\"

CFLAGS += $(LWIPCONFIG_CFLAGS)
