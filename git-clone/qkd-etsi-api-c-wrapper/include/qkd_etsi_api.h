/*
 * Copyright (C) 2024 QURSA Project
 * SPDX-License-Identifier: MIT
 *
 * Authors:
 * - Javier Blanco-Romero (@fj-blanco) - UC3M
 * - Daniel Sobral Blanco (@dasobral) - UC3M
 */

/*
 * qkd_etsi_api.h
 */

#ifndef QKD_ETSI_API_H
#define QKD_ETSI_API_H

#include "qkd_config.h"

/*
 * Internal key buffer size in BYTES (32 bytes = 256 bits)
 * Note: ETSI 014 API specifies key sizes in BITS in JSON requests/responses
 * Always convert: api_size_bits = QKD_KEY_SIZE * 8
 */
#define QKD_KEY_SIZE 32

#define QKD_KSID_SIZE 16    /* UUID_v4 16 bytes (128 bits) */
#define QKD_MAX_URI_LEN 256 /* Maximum length for URIs */

#endif /* QKD_ETSI_API_H */
