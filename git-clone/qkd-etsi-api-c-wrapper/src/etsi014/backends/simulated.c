/*
 * Copyright (C) 2024 QURSA Project
 * SPDX-License-Identifier: MIT
 *
 * Authors:
 * - Javier Blanco-Romero (@fj-blanco) - UC3M
 */

/*
 * src/etsi014/backends/simulated.c
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <pthread.h>
 #include <openssl/bio.h>
 #include <openssl/buffer.h>
 #include <openssl/evp.h>
 #include <openssl/rand.h>
 #include <uuid/uuid.h>
 #include "etsi014/api.h"
 #include "etsi014/backends/simulated.h"
 #include "debug.h"

 #ifdef QKD_USE_SIMULATED

 #define MAX_KEYS 1024
 #define DEFAULT_KEY_SIZE 32
 #define MAX_KEY_SIZE 512
 #define MIN_KEY_SIZE 8
 #define API_DELAY_MS 10

 static struct {
     unsigned char *key_data;  // Raw key bytes
     size_t key_size;          // Key size in bytes
     char *key_id;             // UUID format
     int in_use;               // Flag to track if slot is used
 } key_store[MAX_KEYS];

 static size_t stored_keys = 0;
 static pthread_mutex_t key_store_mutex = PTHREAD_MUTEX_INITIALIZER;
 
 static char* base64_encode(const unsigned char* input, int length) {
     BIO *bmem, *b64;
     BUF_MEM *bptr;
     
     b64 = BIO_new(BIO_f_base64());
     bmem = BIO_new(BIO_s_mem());
     b64 = BIO_push(b64, bmem);
     
     BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
     BIO_write(b64, input, length);
     BIO_flush(b64);
     
     BIO_get_mem_ptr(b64, &bptr);
     
     char *buff = malloc(bptr->length + 1);
     if (!buff) {
         BIO_free_all(b64);
         return NULL;
     }
     
     memcpy(buff, bptr->data, bptr->length);
     buff[bptr->length] = 0;
     
     BIO_free_all(b64);
     return buff;
 }
 
 // Generate a valid UUID string
 static char* generate_uuid_string() {
     uuid_t uuid;
     char *uuid_str = malloc(37); // 36 chars + null terminator
     
     if (!uuid_str) {
         return NULL;
     }
     
     uuid_generate(uuid);
     uuid_unparse(uuid, uuid_str);
     
     return uuid_str;
 }
 
 static uint32_t sim_get_status(const char *kme_hostname,
                                const char *slave_sae_id,
                                qkd_status_t *status) {
     usleep(API_DELAY_MS * 1000);
     if (!kme_hostname || !slave_sae_id || !status) {
         QKD_DBG_ERR("sim_get_status: NULL parameter");
         return QKD_STATUS_BAD_REQUEST;
     }

     pthread_mutex_lock(&key_store_mutex);
     size_t current_stored = stored_keys;
     pthread_mutex_unlock(&key_store_mutex);

     status->key_size = DEFAULT_KEY_SIZE * 8;  // Return in bits per ETSI spec
     status->stored_key_count = current_stored;
     status->max_key_count = MAX_KEYS;
     status->max_key_per_request = 128;
     status->max_key_size = MAX_KEY_SIZE * 8;
     status->min_key_size = MIN_KEY_SIZE * 8;
     status->max_SAE_ID_count = 0;

     status->source_KME_ID = strdup(kme_hostname);
     status->target_KME_ID = strdup("simulated-target-kme");
     status->master_SAE_ID = strdup("simulated-master");
     status->slave_SAE_ID = strdup(slave_sae_id);
     status->status_extension = NULL;

     return QKD_STATUS_OK;
 }
 
 static uint32_t sim_get_key(const char *kme_hostname,
                            const char *slave_sae_id,
                            qkd_key_request_t *request,
                            qkd_key_container_t *container) {
     usleep(API_DELAY_MS * 1000);

     if (!container) {
         QKD_DBG_ERR("sim_get_key: NULL container");
         return QKD_STATUS_BAD_REQUEST;
     }

     // Determine key size from request (ETSI 014: size is in BITS)
     int num_keys = (request && request->number > 0) ? request->number : 1;
     int key_size_bits = (request && request->size > 0) ? request->size : (DEFAULT_KEY_SIZE * 8);
     int key_size_bytes = key_size_bits / 8;

     // Validate: size must be multiple of 8 bits
     if (key_size_bits % 8 != 0) {
         QKD_DBG_ERR("sim_get_key: key size %d must be multiple of 8 bits", key_size_bits);
         return QKD_STATUS_BAD_REQUEST;
     }

     // Validate key size range (ETSI 014 compliant)
     if (key_size_bytes < MIN_KEY_SIZE || key_size_bytes > MAX_KEY_SIZE) {
         QKD_DBG_ERR("sim_get_key: key size %d bits (%d bytes) out of range [%d-%d] bytes",
                     key_size_bits, key_size_bytes, MIN_KEY_SIZE, MAX_KEY_SIZE);
         return QKD_STATUS_BAD_REQUEST;
     }
     if (num_keys > 128) {
         QKD_DBG_ERR("sim_get_key: too many keys requested %d", num_keys);
         return QKD_STATUS_BAD_REQUEST;
     }

     container->key_count = num_keys;
     container->keys = calloc(num_keys, sizeof(qkd_key_t));
     container->key_container_extension = NULL;
     if (!container->keys) {
         return QKD_STATUS_SERVER_ERROR;
     }

     pthread_mutex_lock(&key_store_mutex);

     for (int i = 0; i < num_keys; i++) {
         // Generate random key using OpenSSL CSPRNG
         unsigned char *random_key = malloc(key_size_bytes);
         if (!random_key) {
             goto cleanup_error;
         }
         if (RAND_bytes(random_key, key_size_bytes) != 1) {
             QKD_DBG_ERR("sim_get_key: RAND_bytes failed");
             free(random_key);
             goto cleanup_error;
         }

         container->keys[i].key = base64_encode(random_key, key_size_bytes);
         if (!container->keys[i].key) {
             free(random_key);
             goto cleanup_error;
         }

         container->keys[i].key_ID = generate_uuid_string();
         if (!container->keys[i].key_ID) {
             free(random_key);
             free(container->keys[i].key);
             goto cleanup_error;
         }
         container->keys[i].key_ID_extension = NULL;
         container->keys[i].key_extension = NULL;

         // Store key for later retrieval via GET_KEY_WITH_IDS
         if (stored_keys < MAX_KEYS) {
             key_store[stored_keys].key_data = random_key;
             key_store[stored_keys].key_size = key_size_bytes;
             key_store[stored_keys].key_id = strdup(container->keys[i].key_ID);
             key_store[stored_keys].in_use = 1;
             stored_keys++;
         } else {
             free(random_key);
             QKD_DBG_WARN("sim_get_key: key_store full, key not stored for retrieval");
         }
     }

     pthread_mutex_unlock(&key_store_mutex);
     return QKD_STATUS_OK;

 cleanup_error:
     pthread_mutex_unlock(&key_store_mutex);
     // Free any already allocated keys
     for (int j = 0; j < num_keys; j++) {
         free(container->keys[j].key);
         free(container->keys[j].key_ID);
     }
     free(container->keys);
     container->keys = NULL;
     container->key_count = 0;
     return QKD_STATUS_SERVER_ERROR;
 }
 
 static uint32_t sim_get_key_with_ids(const char *kme_hostname,
                                      const char *master_sae_id,
                                      qkd_key_ids_t *key_ids,
                                      qkd_key_container_t *container) {
     usleep(API_DELAY_MS * 1000);

     if (!key_ids || key_ids->key_ID_count == 0 || !key_ids->key_IDs || !container) {
         QKD_DBG_ERR("sim_get_key_with_ids: invalid parameters");
         return QKD_STATUS_BAD_REQUEST;
     }

     int num_requested = key_ids->key_ID_count;
     container->keys = calloc(num_requested, sizeof(qkd_key_t));
     container->key_container_extension = NULL;
     if (!container->keys) {
         return QKD_STATUS_SERVER_ERROR;
     }

     int found_count = 0;
     pthread_mutex_lock(&key_store_mutex);

     for (int i = 0; i < num_requested; i++) {
         const char *requested_id = key_ids->key_IDs[i].key_ID;
         if (!requested_id) {
             continue;
         }

         // Search key_store for matching key_id
         int found = 0;
         for (size_t j = 0; j < stored_keys; j++) {
             if (key_store[j].in_use && key_store[j].key_id &&
                 strcmp(key_store[j].key_id, requested_id) == 0) {
                 // Found the key
                 container->keys[found_count].key = base64_encode(
                     key_store[j].key_data, key_store[j].key_size);
                 container->keys[found_count].key_ID = strdup(requested_id);
                 container->keys[found_count].key_ID_extension = NULL;
                 container->keys[found_count].key_extension = NULL;

                 if (container->keys[found_count].key &&
                     container->keys[found_count].key_ID) {
                     found_count++;
                     found = 1;
                     QKD_DBG_INFO("sim_get_key_with_ids: found key %s", requested_id);
                 }
                 break;
             }
         }

         if (!found) {
             QKD_DBG_WARN("sim_get_key_with_ids: key %s not found", requested_id);
         }
     }

     pthread_mutex_unlock(&key_store_mutex);

     if (found_count == 0) {
         free(container->keys);
         container->keys = NULL;
         container->key_count = 0;
         QKD_DBG_ERR("sim_get_key_with_ids: no keys found");
         return QKD_STATUS_BAD_REQUEST;
     }

     container->key_count = found_count;
     return QKD_STATUS_OK;
 }
 
 // Cleanup function for releasing memory - exported for external use
 void qkd_014_cleanup_key_store(void) {
     pthread_mutex_lock(&key_store_mutex);
     for (size_t i = 0; i < stored_keys; i++) {
         if (key_store[i].key_data) {
             // Securely clear key material before freeing
             memset(key_store[i].key_data, 0, key_store[i].key_size);
             free(key_store[i].key_data);
             key_store[i].key_data = NULL;
         }
         free(key_store[i].key_id);
         key_store[i].key_id = NULL;
         key_store[i].in_use = 0;
     }
     stored_keys = 0;
     pthread_mutex_unlock(&key_store_mutex);
 }

 // Helper to free key container returned by GET_KEY/GET_KEY_WITH_IDS
 void qkd_014_free_key_container(qkd_key_container_t *container) {
     if (!container) return;
     if (container->keys) {
         for (int i = 0; i < container->key_count; i++) {
             if (container->keys[i].key) {
                 // Securely clear key data
                 size_t len = strlen(container->keys[i].key);
                 memset(container->keys[i].key, 0, len);
                 free(container->keys[i].key);
             }
             free(container->keys[i].key_ID);
         }
         free(container->keys);
         container->keys = NULL;
     }
     container->key_count = 0;
 }

 // Helper to free status struct
 void qkd_014_free_status(qkd_status_t *status) {
     if (!status) return;
     free(status->source_KME_ID);
     free(status->target_KME_ID);
     free(status->master_SAE_ID);
     free(status->slave_SAE_ID);
     status->source_KME_ID = NULL;
     status->target_KME_ID = NULL;
     status->master_SAE_ID = NULL;
     status->slave_SAE_ID = NULL;
 }
 
 const struct qkd_014_backend simulated_backend = {
     .name = "simulated",
     .get_status = sim_get_status,
     .get_key = sim_get_key,
     .get_key_with_ids = sim_get_key_with_ids
 };
 
 #endif /* QKD_USE_SIMULATED */