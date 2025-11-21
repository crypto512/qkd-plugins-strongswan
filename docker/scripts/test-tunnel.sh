#!/bin/bash
# Test script for QKD IPsec tunnel
set -e

echo "=== QKD IPsec Tunnel Test ==="
echo ""

# Check services are running
echo "1. Checking service status..."
docker compose ps

echo ""
echo "2. Checking KME health..."
docker compose exec -T kme-alice curl -s http://localhost:8443/health | python3 -m json.tool
docker compose exec -T kme-bob curl -s http://localhost:8443/health | python3 -m json.tool

echo ""
echo "3. Loading swanctl configuration on Alice..."
docker compose exec -T alice swanctl --load-all

echo ""
echo "4. Loading swanctl configuration on Bob..."
docker compose exec -T bob swanctl --load-all

echo ""
echo "5. Checking IKE SAs before ping..."
docker compose exec -T alice swanctl --list-sas || echo "No SAs yet"

echo ""
echo "6. Triggering tunnel establishment via ping..."
docker compose exec -T alice ping -c 3 10.1.0.20 || true

echo ""
echo "7. Checking IKE SAs after ping..."
docker compose exec -T alice swanctl --list-sas

echo ""
echo "8. Checking plugin activity in Alice logs..."
docker compose logs alice 2>&1 | grep -i "qkd" | tail -20 || echo "No QKD logs found"

echo ""
echo "9. Checking plugin activity in Bob logs..."
docker compose logs bob 2>&1 | grep -i "qkd" | tail -20 || echo "No QKD logs found"

echo ""
echo "10. Checking KME Alice logs..."
docker compose logs kme-alice 2>&1 | grep -E "(GET_KEY|GET_STATUS)" | tail -10 || echo "No key operations logged"

echo ""
echo "=== Test Complete ==="
