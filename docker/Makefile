# QKD strongSwan Docker Stack Makefile
BUILDER_NAME := qkd-builder

.PHONY: builder build build-no-cache up down clean test test-mode-client test-mode-server

# Create dedicated builder if not exists
builder:
	@docker buildx inspect $(BUILDER_NAME) >/dev/null 2>&1 || \
		docker buildx create --name $(BUILDER_NAME) --driver docker-container
	@docker buildx use $(BUILDER_NAME)

# Build with cache
build: builder
	BUILDX_BAKE_ENTITLEMENTS_FS=0 docker buildx bake -f docker-bake.hcl --builder $(BUILDER_NAME) --load

# Build without cache
build-no-cache: builder
	BUILDX_BAKE_ENTITLEMENTS_FS=0 docker buildx bake -f docker-bake.hcl --builder $(BUILDER_NAME) --load --no-cache

# Start stack
up:
	docker compose up -d
	@sleep 10
	docker compose ps

# Stop stack
down:
	docker compose down -v

# Clean builder cache only
clean:
	docker buildx prune -f --builder $(BUILDER_NAME)

# Remove builder entirely
clean-builder:
	docker buildx rm $(BUILDER_NAME) || true

# Helper function for single test run
define run_single_test
	@echo "[1/7] Generating PKI certificates..."
	@bash scripts/generate-pki.sh pki
	@echo ""
	@echo "[2/7] Starting containers..."
	@docker compose up -d
	@echo ""
	@echo "[3/7] Waiting for containers to be healthy..."
	@sleep 20
	@docker compose ps
	@echo ""
	@echo "[4/7] Checking KME health endpoints..."
	@docker exec qkd-alice curl -k -s https://kme-alice:8443/health | grep -q healthy && echo "  ✓ KME-Alice: healthy" || echo "  ✗ KME-Alice: unhealthy"
	@docker exec qkd-bob curl -k -s https://kme-bob:8444/health | grep -q healthy && echo "  ✓ KME-Bob: healthy" || echo "  ✗ KME-Bob: unhealthy"
	@echo ""
	@echo "[5/7] Loading swanctl configurations..."
	@docker exec qkd-alice swanctl --load-all
	@docker exec qkd-bob swanctl --load-all
	@echo ""
	@echo "[6/7] Initiating QKD tunnel..."
	@docker exec qkd-alice swanctl --initiate --child qkd-child
	@sleep 2
	@echo ""
	@echo "[7/7] Testing encrypted connectivity..."
	@docker exec qkd-alice ping -c 4 10.1.0.20 > /tmp/qkd-ping-test.log 2>&1
	@echo ""
	@echo "Tunnel Status (Alice):"
	@docker exec qkd-alice swanctl --list-sas
	@echo ""
	@echo "Ping Test Results:"
	@cat /tmp/qkd-ping-test.log
	@docker compose down -v
endef

# Full test: test both initiation modes
test: build
	@echo "========================================="
	@echo "QKD strongSwan Integration Test Suite"
	@echo "Testing BOTH initiation modes"
	@echo "========================================="
	@echo ""
	@# Cleanup any existing initiation_mode setting
	@sed -i '/initiation_mode/d' configs/alice/strongswan.conf 2>/dev/null || true
	@sed -i '/initiation_mode/d' configs/bob/strongswan.conf 2>/dev/null || true
	@echo "========================================="
	@echo "TEST 1: CLIENT-INITIATED mode"
	@echo "========================================="
	@sed -i '/retry_count/a\            initiation_mode = client' configs/alice/strongswan.conf
	@sed -i '/retry_count/a\            initiation_mode = client' configs/bob/strongswan.conf
	$(call run_single_test)
	@if grep -q "0% packet loss" /tmp/qkd-ping-test.log; then \
		echo "  ✓ CLIENT mode: PASS"; \
		CLIENT_RESULT=0; \
	else \
		echo "  ✗ CLIENT mode: FAIL"; \
		CLIENT_RESULT=1; \
	fi; \
	rm -f /tmp/qkd-ping-test.log; \
	echo ""; \
	echo "========================================="; \
	echo "TEST 2: SERVER-INITIATED mode"; \
	echo "========================================="; \
	sed -i 's/initiation_mode = client/initiation_mode = server/' configs/alice/strongswan.conf; \
	sed -i 's/initiation_mode = client/initiation_mode = server/' configs/bob/strongswan.conf; \
	bash scripts/generate-pki.sh pki; \
	docker compose up -d; \
	sleep 20; \
	docker exec qkd-alice swanctl --load-all; \
	docker exec qkd-bob swanctl --load-all; \
	docker exec qkd-alice swanctl --initiate --child qkd-child; \
	sleep 2; \
	docker exec qkd-alice ping -c 4 10.1.0.20 > /tmp/qkd-ping-test.log 2>&1; \
	cat /tmp/qkd-ping-test.log; \
	docker compose down -v; \
	if grep -q "0% packet loss" /tmp/qkd-ping-test.log; then \
		echo "  ✓ SERVER mode: PASS"; \
		SERVER_RESULT=0; \
	else \
		echo "  ✗ SERVER mode: FAIL"; \
		SERVER_RESULT=1; \
	fi; \
	rm -f /tmp/qkd-ping-test.log; \
	sed -i '/initiation_mode/d' configs/alice/strongswan.conf; \
	sed -i '/initiation_mode/d' configs/bob/strongswan.conf; \
	echo ""; \
	echo "========================================="; \
	echo "FINAL SUMMARY"; \
	echo "========================================="; \
	if [ $$CLIENT_RESULT -eq 0 ] && [ $$SERVER_RESULT -eq 0 ]; then \
		echo "  ✓ All tests PASSED"; \
		exit 0; \
	else \
		echo "  ✗ Some tests FAILED"; \
		exit 1; \
	fi
