/*
 * Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
 */

/**
 * @file qkd_config.h
 * @brief QKD plugin configuration management
 *
 * Provides configuration loading from strongswan.conf with fallback to
 * environment variables for backward compatibility.
 */

#ifndef QKD_CONFIG_H_
#define QKD_CONFIG_H_

#include <library.h>

/**
 * QKD plugin configuration structure
 */
typedef struct qkd_config_t {
    /** KME hostname for master SAE (enc_keys endpoint) */
    char *master_kme_hostname;

    /** KME hostname for slave SAE (dec_keys endpoint) */
    char *slave_kme_hostname;

    /** Master SAE identifier */
    char *master_sae_id;

    /** Slave SAE identifier */
    char *slave_sae_id;

    /** Client certificate path for mTLS */
    char *cert_path;

    /** Client private key path for mTLS */
    char *key_path;

    /** CA certificate path for mTLS */
    char *ca_cert_path;

    /** Key size in bits (default: 256) */
    uint32_t key_size;

    /** Request timeout in milliseconds (default: 30000) */
    uint32_t timeout;

    /** Number of retry attempts (default: 3) */
    uint32_t retry_count;

    /** Enable debug logging of keys (INSECURE - for testing only) */
    bool debug_keys;

    /* ETSI 004 specific configuration */
    /** QKD backend type for ETSI 004 (default: "simulated") */
    char *qkd_backend;

    /** Source URI for ETSI 004 connections */
    char *source_uri;

    /** Destination URI for ETSI 004 connections */
    char *dest_uri;

} qkd_config_t;

/**
 * Get the global QKD configuration instance
 *
 * Configuration is loaded from strongswan.conf section:
 *   charon.plugins.qkd {
 *       # ETSI 014 parameters
 *       master_kme_hostname = https://...
 *       slave_kme_hostname = https://...
 *       master_sae_id = ...
 *       slave_sae_id = ...
 *       cert_path = /path/to/cert.pem
 *       key_path = /path/to/key.pem
 *       ca_cert_path = /path/to/ca.pem
 *       key_size = 256
 *       timeout = 30000
 *       retry_count = 3
 *       # ETSI 004 parameters
 *       qkd_backend = simulated
 *       source_uri = alice
 *       dest_uri = bob
 *   }
 *
 * Falls back to environment variables for backward compatibility (DEPRECATED):
 *   ETSI 014 only: QKD_MASTER_KME_HOSTNAME, QKD_SLAVE_KME_HOSTNAME,
 *                  QKD_MASTER_SAE, QKD_SLAVE_SAE,
 *                  QKD_MASTER_CERT_PATH, QKD_MASTER_KEY_PATH, QKD_MASTER_CA_CERT_PATH
 *   Note: ETSI 004 no longer supports environment variables
 *
 * @return  pointer to configuration, NULL if not initialized
 */
qkd_config_t *qkd_config_get(void);

/**
 * Initialize QKD configuration
 *
 * @return  TRUE if configuration loaded successfully
 */
bool qkd_config_init(void);

/**
 * Destroy QKD configuration and free resources
 */
void qkd_config_destroy(void);

/**
 * Validate configuration completeness
 *
 * @return  TRUE if all required parameters are set
 */
bool qkd_config_validate(void);

#endif /* QKD_CONFIG_H_ */
