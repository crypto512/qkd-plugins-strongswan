# TODO - QKD Plugin ETSI 014 Compliance & Cleanup

This document tracks remaining issues, technical debt, and potential improvements for the strongSwan QKD plugins and qkd-etsi-api-c-wrapper library.

**Last Updated:** 2025-11-21

---

## CRITICAL PRIORITY

### 1. Implement GET_STATUS Before Key Requests (ETSI 014 Compliance)

**File:** `src/qkd/qkd_etsi_adapter.c:435`

**Issue:** Currently hardcodes key size instead of querying KME capabilities:
```c
request.size = QKD_KEY_SIZE * 8;  /* ETSI 014: size in bits */
```

**ETSI 014 Requirement:** Section 5.1.1 specifies that SAEs should call GET_STATUS first to discover available key sizes and KME capabilities.

**Recommendation:**
```c
// 1. Call GET_STATUS to get KME capabilities
qkd_status_t status;
uint32_t ret = GET_STATUS(kme_hostname, slave_sae_id, &status);
if (ret == QKD_STATUS_OK) {
    request.size = status.key_size;  // Use KME's reported key size
} else {
    // Fallback to configured default if GET_STATUS fails
    request.size = config->key_size;
}
```

**Severity:** HIGH - Violates ETSI 014 workflow specification

---

### 2. Implement Retry Logic for Transient Failures

**Files:**
- `src/qkd/qkd_etsi_adapter.c:427-501`
- `git-clone/qkd-etsi-api-c-wrapper/src/etsi014/backends/qkd_etsi014_backend.c`

**Issue:** No retry logic despite `config->retry_count` parameter existing.

**Current Behavior:** All failures treated equally, no exponential backoff.

**Recommendation:**
- Implement retry loop with exponential backoff for retriable errors:
  - **Retriable:** 503 (Service Unavailable), network timeouts
  - **Non-retriable:** 400 (Bad Request), 401 (Unauthorized), 404 (Not Found)
- Use existing `config->retry_count` and `config->timeout` parameters
- Add jitter to prevent thundering herd

**Example:**
```c
for (int attempt = 0; attempt < config->retry_count; attempt++) {
    uint32_t status = GET_KEY(...);
    if (status == QKD_STATUS_OK) return TRUE;
    if (status != QKD_STATUS_SERVER_ERROR) break;  // Don't retry 4xx errors

    // Exponential backoff: 100ms, 200ms, 400ms...
    usleep(100000 * (1 << attempt));
}
```

**Severity:** CRITICAL - Reduces robustness in production environments

---

## HIGH PRIORITY

### 3. Remove SSL Hostname Verification Bypass

**File:** `git-clone/qkd-etsi-api-c-wrapper/src/etsi014/backends/qkd_etsi014_backend.c`

**Issues:**
- **Line 232:** Hardcoded `CURLOPT_SSL_VERIFYHOST, 0L` for QuKayDee compatibility
- **Lines 343-349:** Environment variable `QKD_SSL_VERIFY_HOST=0` disables verification

**Security Risk:** Man-in-the-middle (MITM) vulnerability when hostname verification disabled.

**Recommendation:**
1. Remove hardcoded bypass on line 232
2. Keep environment variable option for testing, but log security warning:
```c
if (verify_host && strcmp(verify_host, "0") == 0) {
    QKD_DBG_WARN("SSL hostname verification DISABLED - use only for testing!");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
} else {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);  // Full verification
}
```
3. Document in README that production deployments must use proper certificates

**Severity:** HIGH - Security vulnerability

---

### 4. Migrate ETSI 004 Environment Variables to Config

**File:** `src/qkd/qkd_etsi_adapter.c`

**Issues:**
- **Lines 49-51:** Environment variable definitions
  ```c
  #define ENV_QKD_BACKEND "QKD_BACKEND"
  #define ENV_QKD_SOURCE_URI "QKD_SOURCE_URI"
  #define ENV_QKD_DEST_URI "QKD_DEST_URI"
  ```
- **Line 138:** `getenv(ENV_QKD_BACKEND)`
- **Lines 202-203:** `getenv(ENV_QKD_SOURCE_URI)`, `getenv(ENV_QKD_DEST_URI)`

**Inconsistency:** ETSI 014 uses strongswan.conf, but ETSI 004 still uses environment variables.

**Recommendation:**
1. Add to `src/qkd/qkd_config.h`:
   ```c
   typedef struct qkd_config {
       // ... existing fields ...
       char *qkd_backend;      // For ETSI 004
       char *source_uri;       // For ETSI 004
       char *dest_uri;         // For ETSI 004
   } qkd_config_t;
   ```
2. Update `qkd_config.c` to read these from strongswan.conf
3. Modify `qkd_etsi_adapter.c` to use config instead of getenv()

**Severity:** HIGH - Consistency and maintainability issue

---

## MEDIUM PRIORITY

### 5. Move ETSI API Version to Compile-Time

**File:** `src/qkd/qkd_kex.c:43`

**Issue:** Runtime API version detection via environment variable:
```c
const char *api_version = getenv("ETSI_API_VERSION");
```

**Recommendation:** Use compile-time flag (already exists: `ETSI_014_API`) or strongswan.conf setting.

**Severity:** MEDIUM - Runtime configuration should be minimal

---

### 6. Improve HTTP Error Code Mapping

**File:** `git-clone/qkd-etsi-api-c-wrapper/src/etsi014/backends/qkd_etsi014_backend.c:389-407`

**Issue:** Generic error mapping loses specificity:
```c
return (http_code < 500) ? QKD_STATUS_BAD_REQUEST : QKD_STATUS_SERVER_ERROR;
```

**Recommendation:** Add specific status codes to `include/etsi014/api.h`:
```c
#define QKD_STATUS_FORBIDDEN 403
#define QKD_STATUS_NOT_FOUND 404
#define QKD_STATUS_CONFLICT 409
```

Then map in handler:
```c
switch (http_code) {
    case 200: return QKD_STATUS_OK;
    case 400: return QKD_STATUS_BAD_REQUEST;
    case 401: return QKD_STATUS_UNAUTHORIZED;
    case 403: return QKD_STATUS_FORBIDDEN;
    case 404: return QKD_STATUS_NOT_FOUND;
    case 503: return QKD_STATUS_SERVER_ERROR;
    default: return (http_code < 500) ? QKD_STATUS_BAD_REQUEST : QKD_STATUS_SERVER_ERROR;
}
```

**Severity:** MEDIUM - Improves debugging and error handling

---

### 7. Resolve Key Size Units TODO

**File:** `git-clone/qkd-etsi-api-c-wrapper/include/qkd_etsi_api.h:20`

**Issue:**
```c
#define QKD_KEY_SIZE 32     /* Size of key buffer in bytes */
/* TODO: Check if this is always as in QuKayDee simulator (expect size in bites)*/
```

**Clarification Needed:**
- ETSI 014 API uses **bits** for key size in JSON (confirmed in spec section 6.2)
- Internal buffer handling uses **bytes**
- QuKayDee simulator expectations unclear

**Recommendation:**
1. Verify QuKayDee simulator documentation
2. Update comment to clarify:
   ```c
   #define QKD_KEY_SIZE 32  /* Internal buffer size in BYTES (ETSI 014 API uses BITS) */
   ```
3. Ensure conversion is always `bits = bytes * 8` when calling API

**Severity:** MEDIUM - Documentation clarity

---

## LOW PRIORITY (Cosmetic/Documentation)

### 8. Legacy Environment Variable Fallbacks

**File:** `src/qkd/qkd_config.c:22-31`

**Issue:** Maintains backward compatibility with environment variables:
```c
#define ENV_MASTER_KME "QKD_MASTER_KME_HOSTNAME"
#define ENV_SLAVE_KME "QKD_SLAVE_KME_HOSTNAME"
// ... etc
```

**Status:** This is intentional for migration support (well documented in code).

**Recommendation:**
- Document deprecation timeline in README
- Consider removing in v2.0 after users have migrated to strongswan.conf
- Add deprecation warning when env vars are used:
  ```c
  if (getenv(ENV_MASTER_KME)) {
      DBG1(DBG_LIB, "QKD: Using deprecated environment variable %s, migrate to strongswan.conf", ENV_MASTER_KME);
  }
  ```

**Severity:** LOW - Intentional design for backward compatibility

---

### 9. Tuning Parameters via Environment

**File:** `git-clone/qkd-etsi-api-c-wrapper/src/etsi014/backends/qkd_etsi014_backend.c:334-343`

**Issue:** Timeout and SSL settings via environment variables:
- `QKD_CONNECT_TIMEOUT`
- `QKD_TRANSFER_TIMEOUT`
- `QKD_SSL_VERIFY_HOST`

**Status:** These are debugging/tuning knobs with sensible defaults.

**Recommendation:**
- Document these options in README/CHANGELOG
- Consider moving to `qkd_cert_config_t` structure for API-based configuration
- Keep environment variable fallback for quick testing

**Severity:** LOW - Acceptable for tuning parameters

---

### 10. Missing ETSI 014 Optional Features

**File:** `git-clone/qkd-etsi-api-c-wrapper/include/etsi014/api.h`

**Features Not Implemented:**
- `additional_slave_SAE_IDs` (multicast key distribution) - Line 45
- `extension_mandatory` / `extension_optional` (vendor extensions) - Lines 47-48
- Extension fields in responses (Lines 38, 54, 56, 62)

**Status:** These are truly optional per ETSI 014 specification.

**Recommendation:**
- Document in README that these features are not yet supported
- Add if specific use case emerges (e.g., multicast IPsec)

**Severity:** LOW - Optional features per spec

---

### 11. ETSI 004 QoS Parameter Configuration

**File:** `src/qkd/qkd_etsi_adapter.c:236-251`

**Issue:** Hardcoded QoS parameters:
```c
(*handle)->qos.Priority = 0;
(*handle)->qos.Max_bps = 40000;
(*handle)->qos.Min_bps = 5000;
(*handle)->qos.Jitter = 10;
(*handle)->qos.TTL = 3600;
```

**Recommendation:**
- Add to `qkd_config_t` if ETSI 004 support is needed:
  ```c
  uint32_t qos_priority;
  uint32_t qos_max_bps;
  uint32_t qos_min_bps;
  uint32_t qos_jitter;
  uint32_t qos_ttl;
  ```

**Severity:** LOW - ETSI 004 less common than ETSI 014

---

### 12. Extension Field Data Structures

**File:** `git-clone/qkd-etsi-api-c-wrapper/include/etsi014/api.h`

**Issue:** Extension fields defined as `void*` with no structure:
- Line 38: `void *status_extension;`
- Line 47-48: `void *extension_mandatory;`, `void *extension_optional;`
- Line 54, 56: Key extensions

**Status:** ETSI 014 allows vendor-specific extensions but doesn't define structure.

**Recommendation:**
- Document that extensions are not yet supported
- Define structure when specific vendor extension needed

**Severity:** LOW - Vendor-specific, no standard defined

---

## FILES ANALYZED

### Plugin Code (src/qkd/)
- ✅ qkd_etsi_adapter.c
- ✅ qkd_etsi_adapter.h
- ✅ qkd_kex.c
- ✅ qkd_kex.h
- ✅ qkd_config.c
- ✅ qkd_config.h
- ✅ qkd_plugin.c
- ✅ qkd_plugin.h

### Wrapper Library (git-clone/qkd-etsi-api-c-wrapper/)
- ✅ src/etsi014/api.c
- ✅ src/etsi014/backends/qkd_etsi014_backend.c
- ✅ src/etsi014/backends/simulated.c
- ✅ src/etsi004/api.c
- ✅ include/qkd_etsi_api.h
- ✅ include/etsi014/api.h
- ✅ include/etsi014/backends/qkd_etsi014_backend.h

---

## CURRENT STATUS

**Working Features:**
- ✅ Full tunnel establishment (ECDH + Kyber + QKD)
- ✅ ETSI 014 API implementation (GET_STATUS, GET_KEY, GET_KEY_WITH_IDS)
- ✅ mTLS authentication with X-SAE-ID header
- ✅ Thread-safe configuration management
- ✅ Secure memory handling (RAND_bytes, memset_s)
- ✅ Docker test environment with KME mock

**Known Limitations:**
- ⚠️ No GET_STATUS call before key requests (non-compliant with ETSI 014)
- ⚠️ No retry logic for transient failures
- ⚠️ SSL hostname verification can be disabled
- ⚠️ Mixed environment variable and config file usage
- ⚠️ Generic HTTP error handling

---

## RECOMMENDATIONS FOR NEXT RELEASE

### v1.1 (Maintenance Release)
1. Implement GET_STATUS before key requests (Issue #1)
2. Add retry logic (Issue #2)
3. Fix SSL verification (Issue #3)

### v2.0 (Major Release)
1. Complete environment variable migration (Issues #4, #5, #8)
2. Improve error handling (Issue #6)
3. Remove deprecated features
4. Add comprehensive logging

### Future Considerations
- Support for multicast key distribution
- Vendor extension framework
- QoS parameter configuration for ETSI 004
- Performance optimizations
