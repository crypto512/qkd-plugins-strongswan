/*
 * Copyright (C) 2024 QURSA Project
 * SPDX-License-Identifier: MIT
 *
 * Authors:
 * - Javier Blanco-Romero (@fj-blanco) - UC3M
 * - Pedro Otero-García (@pedrotega) - UVigo
 *
 */

/*
 * src/etsi014/api.c
 */

#include "etsi014/api.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef QKD_USE_SIMULATED
#include "etsi014/backends/simulated.h"
static const struct qkd_014_backend *active_backend = &simulated_backend;
#elif defined(QKD_USE_ETSI014_BACKEND)
#include "etsi014/backends/qkd_etsi014_backend.h"
static const struct qkd_014_backend *active_backend = &qkd_etsi014_backend;
#else
static const struct qkd_014_backend *active_backend = NULL;
#endif

/* Global certificate configuration */
static qkd_cert_config_t g_cert_config = {NULL, NULL, NULL, NULL};
static pthread_mutex_t g_cert_config_mutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t GET_STATUS(const char *kme_hostname, const char *slave_sae_id,
                    qkd_status_t *status) {
    if (!kme_hostname || !slave_sae_id || !status) {
        QKD_DBG_ERR("Invalid parameters in GET_STATUS");
        return QKD_STATUS_BAD_REQUEST;
    }

    if (!active_backend || !active_backend->get_status) {
        QKD_DBG_ERR("No REST backend available");
        return QKD_STATUS_SERVER_ERROR;
    }

    return active_backend->get_status(kme_hostname, slave_sae_id, status);
}

uint32_t GET_KEY(const char *kme_hostname, const char *slave_sae_id,
                 qkd_key_request_t *request, qkd_key_container_t *container) {
    // print the active backend name
    QKD_DBG_INFO("GET_KEY(): Active backend name: %s\n", active_backend->name);
    if (!kme_hostname || !slave_sae_id || !container) {
        QKD_DBG_ERR("Invalid parameters in GET_KEY");
        return QKD_STATUS_BAD_REQUEST;
    }

    if (!active_backend || !active_backend->get_key) {
        QKD_DBG_ERR("No REST backend available");
        return QKD_STATUS_SERVER_ERROR;
    }

    return active_backend->get_key(kme_hostname, slave_sae_id, request,
                                   container);
}

uint32_t GET_KEY_WITH_IDS(const char *kme_hostname, const char *master_sae_id,
                          qkd_key_ids_t *key_ids,
                          qkd_key_container_t *container) {
    if (!kme_hostname || !master_sae_id || !key_ids || !container) {
        QKD_DBG_ERR("Invalid parameters in GET_KEY_WITH_IDS");
        return QKD_STATUS_BAD_REQUEST;
    }

    if (!active_backend || !active_backend->get_key_with_ids) {
        QKD_DBG_ERR("No REST backend available");
        return QKD_STATUS_SERVER_ERROR;
    }

    return active_backend->get_key_with_ids(kme_hostname, master_sae_id,
                                            key_ids, container);
}

void QKD_014_SET_CERT_CONFIG(const qkd_cert_config_t *config) {
    pthread_mutex_lock(&g_cert_config_mutex);

    /* SECURITY FIX: Copy strings instead of storing pointers to prevent use-after-free
     * Free old strings before replacing to prevent memory leaks */
    free((void*)g_cert_config.cert_path);
    free((void*)g_cert_config.key_path);
    free((void*)g_cert_config.ca_cert_path);
    free((void*)g_cert_config.sae_id);

    if (config) {
        g_cert_config.cert_path = config->cert_path ? strdup(config->cert_path) : NULL;
        g_cert_config.key_path = config->key_path ? strdup(config->key_path) : NULL;
        g_cert_config.ca_cert_path = config->ca_cert_path ? strdup(config->ca_cert_path) : NULL;
        g_cert_config.sae_id = config->sae_id ? strdup(config->sae_id) : NULL;
    } else {
        g_cert_config.cert_path = NULL;
        g_cert_config.key_path = NULL;
        g_cert_config.ca_cert_path = NULL;
        g_cert_config.sae_id = NULL;
    }

    QKD_DBG_INFO("Certificate configuration set:");
    QKD_DBG_INFO("  Cert: %s", g_cert_config.cert_path ? g_cert_config.cert_path : "(null)");
    QKD_DBG_INFO("  Key: %s", g_cert_config.key_path ? g_cert_config.key_path : "(null)");
    QKD_DBG_INFO("  CA: %s", g_cert_config.ca_cert_path ? g_cert_config.ca_cert_path : "(null)");
    QKD_DBG_INFO("  SAE ID: %s", g_cert_config.sae_id ? g_cert_config.sae_id : "(null)");

    pthread_mutex_unlock(&g_cert_config_mutex);
}

int QKD_014_GET_CERT_CONFIG(qkd_cert_config_t *config) {
    if (!config) {
        return -1;
    }

    pthread_mutex_lock(&g_cert_config_mutex);

    /* SECURITY NOTE: Pointers returned are to internal storage managed by this module.
     * Caller must NOT free these pointers. Pointers remain valid until next SET call.
     * For thread safety, caller should copy strings if needed beyond immediate use. */
    config->cert_path = g_cert_config.cert_path;
    config->key_path = g_cert_config.key_path;
    config->ca_cert_path = g_cert_config.ca_cert_path;
    config->sae_id = g_cert_config.sae_id;

    pthread_mutex_unlock(&g_cert_config_mutex);

    /* Return 0 if all required fields are set, -1 otherwise */
    return (config->cert_path && config->key_path && config->ca_cert_path && config->sae_id) ? 0 : -1;
}// Force rebuild $(date)
