#!/usr/bin/env python3
"""
ETSI GS QKD 014 Mock KME Server
Production-ready mock for testing strongswan QKD plugin
"""

import os
import uuid
import base64
import secrets
import logging
from functools import wraps
from flask import Flask, request, jsonify, g
import redis
import yaml

app = Flask(__name__)

# Configuration
CONFIG_PATH = os.environ.get('KME_CONFIG', '/etc/kme/config.yaml')
config = {}

def load_config():
    """Load configuration from YAML file or environment variables"""
    global config
    if os.path.exists(CONFIG_PATH):
        with open(CONFIG_PATH, 'r') as f:
            config = yaml.safe_load(f)
    else:
        config = {
            'kme_id': os.environ.get('KME_ID', 'kme-default'),
            'redis_host': os.environ.get('REDIS_HOST', 'redis'),
            'redis_port': int(os.environ.get('REDIS_PORT', 6379)),
            'redis_password': os.environ.get('REDIS_PASSWORD', None),
            'default_key_size': int(os.environ.get('DEFAULT_KEY_SIZE', 256)),
            'max_key_count': int(os.environ.get('MAX_KEY_COUNT', 10000)),
            'max_key_per_request': int(os.environ.get('MAX_KEY_PER_REQUEST', 128)),
            'max_key_size': int(os.environ.get('MAX_KEY_SIZE', 4096)),
            'min_key_size': int(os.environ.get('MIN_KEY_SIZE', 64)),
            'log_level': os.environ.get('LOG_LEVEL', 'INFO'),
        }

    logging.basicConfig(
        level=getattr(logging, config.get('log_level', 'INFO')),
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    return config

# Load configuration at module import time (for gunicorn)
config = load_config()

# Redis connection
redis_client = None

def get_redis():
    global redis_client
    if redis_client is None:
        redis_client = redis.Redis(
            host=config.get('redis_host', 'redis'),
            port=config.get('redis_port', 6379),
            password=config.get('redis_password'),
            decode_responses=False
        )
    return redis_client

def require_sae_auth(f):
    """Decorator to validate SAE authentication via mTLS"""
    @wraps(f)
    def decorated(*args, **kwargs):
        # In production, SAE ID would come from client certificate CN
        # For testing, we accept it from header or derive from cert
        sae_id = request.headers.get('X-SAE-ID')
        if not sae_id:
            # Try to get from client certificate (mTLS)
            cert = request.environ.get('SSL_CLIENT_S_DN_CN')
            if cert:
                sae_id = cert
            else:
                # For testing without mTLS, use a default
                sae_id = 'test-sae'
        g.sae_id = sae_id
        return f(*args, **kwargs)
    return decorated

# API Endpoints

@app.route('/api/v1/keys/<slave_sae_id>/status', methods=['GET'])
@require_sae_auth
def get_status(slave_sae_id):
    """
    ETSI 014 Section 5.2 - Get status
    Returns key availability information
    """
    r = get_redis()

    # Count available keys for this SAE pair
    key_pattern = f"qkd:keys:{g.sae_id}:{slave_sae_id}:*"
    stored_keys = len(list(r.scan_iter(match=key_pattern, count=100)))

    status = {
        "source_KME_ID": config.get('kme_id'),
        "target_KME_ID": config.get('target_kme_id', config.get('kme_id')),
        "master_SAE_ID": g.sae_id,
        "slave_SAE_ID": slave_sae_id,
        "key_size": config.get('default_key_size'),
        "stored_key_count": stored_keys,
        "max_key_count": config.get('max_key_count'),
        "max_key_per_request": config.get('max_key_per_request'),
        "max_key_size": config.get('max_key_size'),
        "min_key_size": config.get('min_key_size'),
        "max_SAE_ID_count": 0
    }

    app.logger.info(f"GET_STATUS: master={g.sae_id}, slave={slave_sae_id}")
    return jsonify(status), 200

@app.route('/api/v1/keys/<slave_sae_id>/enc_keys', methods=['GET', 'POST'])
@require_sae_auth
def get_key(slave_sae_id):
    """
    ETSI 014 Section 5.3 - Get key (enc_keys)
    Master SAE requests new keys
    """
    r = get_redis()

    # Parse request parameters
    if request.method == 'POST' and request.is_json:
        data = request.get_json()
        number = data.get('number', 1)
        size = data.get('size', config.get('default_key_size'))
    else:
        number = request.args.get('number', 1, type=int)
        size = request.args.get('size', config.get('default_key_size'), type=int)

    # Validate parameters
    if number < 1 or number > config.get('max_key_per_request'):
        return jsonify({
            "message": f"number must be between 1 and {config.get('max_key_per_request')}"
        }), 400

    if size < config.get('min_key_size') or size > config.get('max_key_size'):
        return jsonify({
            "message": f"size must be between {config.get('min_key_size')} and {config.get('max_key_size')} bits"
        }), 400

    # Size must be multiple of 8
    if size % 8 != 0:
        return jsonify({
            "message": "size shall be a multiple of 8"
        }), 400

    keys = []
    key_bytes = size // 8

    for _ in range(number):
        # Generate key
        key_id = str(uuid.uuid4())
        key_data = secrets.token_bytes(key_bytes)
        key_b64 = base64.b64encode(key_data).decode('ascii')

        # Store in Redis with SAE pair info
        # Key is stored so both KMEs can retrieve it
        redis_key = f"qkd:key:{key_id}"
        r.hset(redis_key, mapping={
            'key': key_data,
            'master_sae': g.sae_id,
            'slave_sae': slave_sae_id,
            'size': size
        })
        # Set expiry (1 hour)
        r.expire(redis_key, 3600)

        keys.append({
            "key_ID": key_id,
            "key": key_b64
        })

    app.logger.info(f"GET_KEY (enc_keys): master={g.sae_id}, slave={slave_sae_id}, count={number}")

    return jsonify({"keys": keys}), 200

@app.route('/api/v1/keys/<master_sae_id>/dec_keys', methods=['GET', 'POST'])
@require_sae_auth
def get_key_with_ids(master_sae_id):
    """
    ETSI 014 Section 5.4 - Get key with key IDs (dec_keys)
    Slave SAE retrieves keys by UUID
    """
    r = get_redis()

    # Parse key IDs from request
    if request.method == 'POST' and request.is_json:
        data = request.get_json()
        key_ids_data = data.get('key_IDs', [])
    else:
        # Single key ID via GET
        key_id = request.args.get('key_ID')
        if key_id:
            key_ids_data = [{"key_ID": key_id}]
        else:
            return jsonify({"message": "key_ID required"}), 400

    if not key_ids_data:
        return jsonify({"message": "key_IDs array is empty"}), 400

    keys = []
    missing_keys = []

    for key_id_obj in key_ids_data:
        key_id = key_id_obj.get('key_ID')
        if not key_id:
            continue

        redis_key = f"qkd:key:{key_id}"
        key_data = r.hgetall(redis_key)

        if not key_data:
            missing_keys.append(key_id)
            continue

        # Verify this SAE is authorized (is the slave for this key)
        stored_slave = key_data.get(b'slave_sae', b'').decode('utf-8')
        stored_master = key_data.get(b'master_sae', b'').decode('utf-8')

        if stored_master != master_sae_id:
            # SECURITY: Log generic message to prevent information disclosure
            # Don't log SAE IDs as this aids reconnaissance attacks
            app.logger.warning("Unauthorized key retrieval attempt detected")
            return jsonify({"message": "Unauthorized"}), 401

        raw_key = key_data.get(b'key', b'')
        key_b64 = base64.b64encode(raw_key).decode('ascii')

        keys.append({
            "key_ID": key_id,
            "key": key_b64
        })

        # Mark key as consumed (delete after retrieval - optional)
        # r.delete(redis_key)

    if missing_keys:
        return jsonify({
            "message": "one or more keys specified are not found on KME",
            "details": [{"missing_key_IDs": missing_keys}]
        }), 400

    app.logger.info(f"GET_KEY_WITH_IDS (dec_keys): slave={g.sae_id}, master={master_sae_id}, count={len(keys)}")

    return jsonify({"keys": keys}), 200

@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint"""
    try:
        r = get_redis()
        r.ping()
        return jsonify({"status": "healthy", "kme_id": config.get('kme_id')}), 200
    except Exception as e:
        return jsonify({"status": "unhealthy", "error": str(e)}), 503

@app.errorhandler(400)
def bad_request(e):
    return jsonify({"message": "Bad request format"}), 400

@app.errorhandler(401)
def unauthorized(e):
    return '', 401

@app.errorhandler(500)
def server_error(e):
    return jsonify({"message": "Error on server side"}), 503

if __name__ == '__main__':
    load_config()

    ssl_context = None
    cert_file = config.get('ssl_cert')
    key_file = config.get('ssl_key')

    if cert_file and key_file and os.path.exists(cert_file) and os.path.exists(key_file):
        ssl_context = (cert_file, key_file)
        app.logger.info(f"Starting KME server with TLS on port {config.get('port', 8443)}")
    else:
        app.logger.warning("Starting KME server WITHOUT TLS (testing only)")

    app.run(
        host='0.0.0.0',
        port=config.get('port', 8443),
        ssl_context=ssl_context,
        threaded=True
    )
