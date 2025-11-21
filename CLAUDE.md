# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

strongSwan plugins implementing Quantum Key Distribution (QKD) for IKEv2. Two plugins:
- `qkd`: Pure QKD key exchange via ETSI GS QKD 004/014 APIs
- `qkd-kem`: Hybrid QKD + post-quantum KEM (Kyber, ML-KEM, FrodoKEM, BIKE, HQC)

## Build Commands

```bash
# First time: build strongSwan dependency
./scripts/build_strongswan.sh

# Initialize autotools
autoreconf -i

# Configure (adjust paths as needed)
./configure --with-strongswan-headers=/usr/include/strongswan \
            --with-plugin-dir=/usr/lib/ipsec/plugins \
            --with-qkd-etsi-api=/usr/local \
            --with-qkd-kem-provider=/usr/local \
            --with-qkd-initiation-mode=client \
            --with-etsi-api-version=014

# Build and install
make
sudo make install

# Clean build artifacts
./scripts/clean.sh
```

## Configure Options

- `--with-qkd-initiation-mode=client|server`: Who initiates QKD exchange (default: client)
- `--with-etsi-api-version=014|004`: ETSI QKD API version (default: 014)

## Architecture

Source in `src/`:
- `qkd/`: Pure QKD plugin - interfaces with ETSI QKD API for key retrieval
- `qkd-kem/`: Hybrid plugin - combines QKD keys with post-quantum KEMs

Plugins register key exchange methods with strongSwan's charon daemon. The QKD plugin communicates with external QKD devices via ETSI REST APIs.

## Testing

See [qkd-ipsec-docker-test](https://github.com/qursa-uc3m/qkd-ipsec-docker-test) for dockerized test environment.
- Use make target in docker directory to build