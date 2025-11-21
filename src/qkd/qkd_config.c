/*
 * Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
 */

/**
 * @file qkd_config.c
 * @brief QKD plugin configuration implementation
 */

#include "qkd_config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <threading/mutex.h>
#include <utils/debug.h>

/* Configuration section path in strongswan.conf */
#define QKD_CONFIG_SECTION "charon.plugins.qkd"

/* Environment variable names (for backward compatibility - DEPRECATED) */
/* ETSI 014 env vars only - ETSI 004 no longer supports env vars */
#define ENV_MASTER_KME "QKD_MASTER_KME_HOSTNAME"
#define ENV_SLAVE_KME "QKD_SLAVE_KME_HOSTNAME"
#define ENV_MASTER_SAE "QKD_MASTER_SAE"
#define ENV_SLAVE_SAE "QKD_SLAVE_SAE"
#define ENV_MASTER_CERT "QKD_MASTER_CERT_PATH"
#define ENV_MASTER_KEY "QKD_MASTER_KEY_PATH"
#define ENV_MASTER_CA "QKD_MASTER_CA_CERT_PATH"
#define ENV_SLAVE_CERT "QKD_SLAVE_CERT_PATH"
#define ENV_SLAVE_KEY "QKD_SLAVE_KEY_PATH"
#define ENV_SLAVE_CA "QKD_SLAVE_CA_CERT_PATH"

/* Default values */
#define DEFAULT_KEY_SIZE 256
#define DEFAULT_TIMEOUT 30000
#define DEFAULT_RETRY_COUNT 3

/* Global configuration instance */
static qkd_config_t *g_config = NULL;
static mutex_t *g_config_mutex = NULL;

/**
 * Helper to get string from config with env fallback
 */
static char *get_config_string(const char *key, const char *env_var,
                                const char *default_val)
{
    const char *value = NULL;
    char config_key[256];

    /* Try strongswan.conf first */
    snprintf(config_key, sizeof(config_key), "%s.%s", QKD_CONFIG_SECTION, key);
    value = lib->settings->get_str(lib->settings, config_key, NULL);

    /* Fall back to environment variable */
    if (!value && env_var) {
        value = getenv(env_var);
    }

    /* Use default if nothing found */
    if (!value) {
        value = default_val;
    }

    return value ? strdup(value) : NULL;
}

/**
 * Helper to get integer from config with env fallback
 */
static uint32_t get_config_int(const char *key, const char *env_var,
                                uint32_t default_val)
{
    char config_key[256];
    uint32_t value;

    /* Try strongswan.conf first */
    snprintf(config_key, sizeof(config_key), "%s.%s", QKD_CONFIG_SECTION, key);
    value = lib->settings->get_int(lib->settings, config_key, 0);

    if (value != 0) {
        return value;
    }

    /* Fall back to environment variable */
    if (env_var) {
        const char *env_val = getenv(env_var);
        if (env_val) {
            char *endptr;
            errno = 0;
            long val = strtol(env_val, &endptr, 10);
            if (errno == 0 && endptr != env_val && *endptr == '\0' && val >= 0) {
                return (uint32_t)val;
            }
        }
    }

    return default_val;
}

/**
 * Helper to get boolean from config
 */
static bool get_config_bool(const char *key, bool default_val)
{
    char config_key[256];

    snprintf(config_key, sizeof(config_key), "%s.%s", QKD_CONFIG_SECTION, key);
    return lib->settings->get_bool(lib->settings, config_key, default_val);
}

bool qkd_config_init(void)
{
    if (!g_config_mutex) {
        g_config_mutex = mutex_create(MUTEX_TYPE_DEFAULT);
    }

    g_config_mutex->lock(g_config_mutex);

    if (g_config) {
        g_config_mutex->unlock(g_config_mutex);
        DBG1(DBG_CFG, "QKD_plugin: configuration already initialized");
        return TRUE;
    }

    g_config = calloc(1, sizeof(qkd_config_t));
    if (!g_config) {
        g_config_mutex->unlock(g_config_mutex);
        DBG1(DBG_CFG, "QKD_plugin: failed to allocate configuration");
        return FALSE;
    }

    DBG1(DBG_CFG, "QKD_plugin: loading configuration...");

    /* Load KME hostnames */
    g_config->master_kme_hostname = get_config_string(
        "master_kme_hostname", ENV_MASTER_KME, NULL);
    g_config->slave_kme_hostname = get_config_string(
        "slave_kme_hostname", ENV_SLAVE_KME, NULL);

    /* Load SAE identifiers */
    g_config->master_sae_id = get_config_string(
        "master_sae_id", ENV_MASTER_SAE, NULL);
    g_config->slave_sae_id = get_config_string(
        "slave_sae_id", ENV_SLAVE_SAE, NULL);

    /* Load certificate paths - try master first, then slave */
    g_config->cert_path = get_config_string(
        "cert_path", ENV_MASTER_CERT, NULL);
    if (!g_config->cert_path) {
        g_config->cert_path = get_config_string(
            "cert_path", ENV_SLAVE_CERT, NULL);
    }

    g_config->key_path = get_config_string(
        "key_path", ENV_MASTER_KEY, NULL);
    if (!g_config->key_path) {
        g_config->key_path = get_config_string(
            "key_path", ENV_SLAVE_KEY, NULL);
    }

    g_config->ca_cert_path = get_config_string(
        "ca_cert_path", ENV_MASTER_CA, NULL);
    if (!g_config->ca_cert_path) {
        g_config->ca_cert_path = get_config_string(
            "ca_cert_path", ENV_SLAVE_CA, NULL);
    }

    /* Load numeric parameters */
    g_config->key_size = get_config_int(
        "key_size", NULL, DEFAULT_KEY_SIZE);
    g_config->timeout = get_config_int(
        "timeout", NULL, DEFAULT_TIMEOUT);
    g_config->retry_count = get_config_int(
        "retry_count", NULL, DEFAULT_RETRY_COUNT);

    /* Load debug flag */
    g_config->debug_keys = get_config_bool("debug_keys", FALSE);

    /* Load ETSI 004 specific parameters */
    g_config->qkd_backend = get_config_string(
        "qkd_backend", NULL, "simulated");
    g_config->source_uri = get_config_string(
        "source_uri", NULL, NULL);
    g_config->dest_uri = get_config_string(
        "dest_uri", NULL, NULL);

    /* Log configuration (without sensitive data) */
    DBG1(DBG_CFG, "QKD_plugin: configuration loaded:");
    DBG1(DBG_CFG, "  master_kme_hostname: %s",
         g_config->master_kme_hostname ? g_config->master_kme_hostname : "(not set)");
    DBG1(DBG_CFG, "  slave_kme_hostname: %s",
         g_config->slave_kme_hostname ? g_config->slave_kme_hostname : "(not set)");
    DBG1(DBG_CFG, "  master_sae_id: %s",
         g_config->master_sae_id ? g_config->master_sae_id : "(not set)");
    DBG1(DBG_CFG, "  slave_sae_id: %s",
         g_config->slave_sae_id ? g_config->slave_sae_id : "(not set)");
    DBG1(DBG_CFG, "  cert_path: %s",
         g_config->cert_path ? g_config->cert_path : "(not set)");
    DBG1(DBG_CFG, "  key_size: %u bits", g_config->key_size);
    DBG1(DBG_CFG, "  timeout: %u ms", g_config->timeout);
    DBG1(DBG_CFG, "  retry_count: %u", g_config->retry_count);
    DBG1(DBG_CFG, "  qkd_backend (ETSI 004): %s",
         g_config->qkd_backend ? g_config->qkd_backend : "(not set)");
    DBG1(DBG_CFG, "  source_uri (ETSI 004): %s",
         g_config->source_uri ? g_config->source_uri : "(not set)");
    DBG1(DBG_CFG, "  dest_uri (ETSI 004): %s",
         g_config->dest_uri ? g_config->dest_uri : "(not set)");

    if (g_config->debug_keys) {
        DBG1(DBG_CFG, "QKD_plugin: WARNING - debug_keys enabled (INSECURE)");
    }

    /* Validate but don't fail - allow charon to start */
    qkd_config_validate();

    g_config_mutex->unlock(g_config_mutex);
    return TRUE;
}

void qkd_config_destroy(void)
{
    if (g_config_mutex) {
        g_config_mutex->lock(g_config_mutex);
    }

    if (g_config) {
        free(g_config->master_kme_hostname);
        free(g_config->slave_kme_hostname);
        free(g_config->master_sae_id);
        free(g_config->slave_sae_id);
        free(g_config->cert_path);
        free(g_config->key_path);
        free(g_config->ca_cert_path);
        free(g_config->qkd_backend);
        free(g_config->source_uri);
        free(g_config->dest_uri);

        free(g_config);
        g_config = NULL;
        DBG1(DBG_CFG, "QKD_plugin: configuration destroyed");
    }

    if (g_config_mutex) {
        g_config_mutex->unlock(g_config_mutex);
        g_config_mutex->destroy(g_config_mutex);
        g_config_mutex = NULL;
    }
}

qkd_config_t *qkd_config_get(void)
{
    /* SECURITY NOTE: Returns pointer to global config without locking.
     * This is safe ONLY because:
     * 1. Config is initialized once at plugin load and never modified
     * 2. Config is destroyed only at plugin unload when no threads are active
     * 3. All config fields are read-only after initialization
     *
     * If config becomes mutable, this function must be replaced with:
     * - qkd_config_lock() / qkd_config_unlock() pair, OR
     * - qkd_config_get_copy() that returns a deep copy
     *
     * Callers MUST check for NULL return value. */
    return g_config;
}

bool qkd_config_validate(void)
{
    if (!g_config) {
        DBG1(DBG_CFG, "QKD_plugin: configuration not initialized");
        return FALSE;
    }

    bool valid = TRUE;

    if (!g_config->master_kme_hostname) {
        DBG1(DBG_CFG, "QKD_plugin: missing master_kme_hostname");
        valid = FALSE;
    }

    if (!g_config->slave_kme_hostname) {
        DBG1(DBG_CFG, "QKD_plugin: missing slave_kme_hostname");
        valid = FALSE;
    }

    if (!g_config->master_sae_id) {
        DBG1(DBG_CFG, "QKD_plugin: missing master_sae_id");
        valid = FALSE;
    }

    if (!g_config->slave_sae_id) {
        DBG1(DBG_CFG, "QKD_plugin: missing slave_sae_id");
        valid = FALSE;
    }

    /* Certificate validation is optional (some backends don't need mTLS) */
    if (g_config->cert_path && g_config->key_path && g_config->ca_cert_path) {
        DBG2(DBG_CFG, "QKD_plugin: mTLS certificates configured");
    } else {
        DBG1(DBG_CFG, "QKD_plugin: mTLS not fully configured (may be OK for some backends)");
    }

    if (g_config->key_size % 8 != 0) {
        DBG1(DBG_CFG, "QKD_plugin: key_size must be multiple of 8 bits");
        valid = FALSE;
    }

    return valid;
}
