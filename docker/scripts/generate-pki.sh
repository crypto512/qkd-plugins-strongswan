#!/bin/bash
# Generate PKI for strongSwan QKD testing
set -e

PKI_DIR="${1:-./pki}"
VALIDITY_DAYS=365

echo "Generating PKI in ${PKI_DIR}..."

mkdir -p "${PKI_DIR}"/{ca,alice,bob,kme}

# Generate CA
echo "Generating CA..."
openssl genrsa -out "${PKI_DIR}/ca/ca.key" 4096
openssl req -x509 -new -nodes \
    -key "${PKI_DIR}/ca/ca.key" \
    -sha256 -days ${VALIDITY_DAYS} \
    -out "${PKI_DIR}/ca/ca.pem" \
    -subj "/C=EU/ST=Test/L=QKD/O=TestOrg/CN=QKD-Test-CA"

# Generate Alice certificate
echo "Generating Alice certificate..."
openssl genrsa -out "${PKI_DIR}/alice/alice.key" 2048
openssl req -new \
    -key "${PKI_DIR}/alice/alice.key" \
    -out "${PKI_DIR}/alice/alice.csr" \
    -subj "/C=EU/ST=Test/L=QKD/O=TestOrg/CN=alice"

openssl x509 -req \
    -in "${PKI_DIR}/alice/alice.csr" \
    -CA "${PKI_DIR}/ca/ca.pem" \
    -CAkey "${PKI_DIR}/ca/ca.key" \
    -CAcreateserial \
    -out "${PKI_DIR}/alice/alice.pem" \
    -days ${VALIDITY_DAYS} \
    -sha256 \
    -extfile <(cat <<EOF
subjectAltName = DNS:alice, IP:10.1.0.10
EOF
)

# Generate Bob certificate
echo "Generating Bob certificate..."
openssl genrsa -out "${PKI_DIR}/bob/bob.key" 2048
openssl req -new \
    -key "${PKI_DIR}/bob/bob.key" \
    -out "${PKI_DIR}/bob/bob.csr" \
    -subj "/C=EU/ST=Test/L=QKD/O=TestOrg/CN=bob"

openssl x509 -req \
    -in "${PKI_DIR}/bob/bob.csr" \
    -CA "${PKI_DIR}/ca/ca.pem" \
    -CAkey "${PKI_DIR}/ca/ca.key" \
    -CAcreateserial \
    -out "${PKI_DIR}/bob/bob.pem" \
    -days ${VALIDITY_DAYS} \
    -sha256 \
    -extfile <(cat <<EOF
subjectAltName = DNS:bob, IP:10.1.0.20
EOF
)

# Generate KME server certificate
echo "Generating KME server certificate..."
openssl genrsa -out "${PKI_DIR}/kme/kme.key" 2048
openssl req -new \
    -key "${PKI_DIR}/kme/kme.key" \
    -out "${PKI_DIR}/kme/kme.csr" \
    -subj "/C=EU/ST=Test/L=QKD/O=TestOrg/CN=kme"

openssl x509 -req \
    -in "${PKI_DIR}/kme/kme.csr" \
    -CA "${PKI_DIR}/ca/ca.pem" \
    -CAkey "${PKI_DIR}/ca/ca.key" \
    -CAcreateserial \
    -out "${PKI_DIR}/kme/kme.pem" \
    -days ${VALIDITY_DAYS} \
    -sha256 \
    -extfile <(cat <<EOF
subjectAltName = DNS:kme-alice, DNS:kme-bob, DNS:kme, IP:10.1.0.100, IP:10.1.0.101
EOF
)

# Generate SAE client certificates (for mTLS with KME)
echo "Generating SAE client certificate for Alice..."
openssl genrsa -out "${PKI_DIR}/alice/alice-sae.key" 2048
openssl req -new \
    -key "${PKI_DIR}/alice/alice-sae.key" \
    -out "${PKI_DIR}/alice/alice-sae.csr" \
    -subj "/C=EU/ST=Test/L=QKD/O=TestOrg/CN=alice-sae"

openssl x509 -req \
    -in "${PKI_DIR}/alice/alice-sae.csr" \
    -CA "${PKI_DIR}/ca/ca.pem" \
    -CAkey "${PKI_DIR}/ca/ca.key" \
    -CAcreateserial \
    -out "${PKI_DIR}/alice/alice-sae.pem" \
    -days ${VALIDITY_DAYS} \
    -sha256

echo "Generating SAE client certificate for Bob..."
openssl genrsa -out "${PKI_DIR}/bob/bob-sae.key" 2048
openssl req -new \
    -key "${PKI_DIR}/bob/bob-sae.key" \
    -out "${PKI_DIR}/bob/bob-sae.csr" \
    -subj "/C=EU/ST=Test/L=QKD/O=TestOrg/CN=bob-sae"

openssl x509 -req \
    -in "${PKI_DIR}/bob/bob-sae.csr" \
    -CA "${PKI_DIR}/ca/ca.pem" \
    -CAkey "${PKI_DIR}/ca/ca.key" \
    -CAcreateserial \
    -out "${PKI_DIR}/bob/bob-sae.pem" \
    -days ${VALIDITY_DAYS} \
    -sha256

# Cleanup CSR files
rm -f "${PKI_DIR}"/*/*.csr

# Set permissions
chmod 600 "${PKI_DIR}"/*/*.key
chmod 644 "${PKI_DIR}"/*/*.pem

echo "PKI generation complete!"
echo ""
echo "Files generated:"
find "${PKI_DIR}" -type f | sort
