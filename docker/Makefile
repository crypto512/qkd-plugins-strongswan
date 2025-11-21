# QKD strongSwan Docker Stack Makefile
BUILDER_NAME := qkd-builder

.PHONY: builder build build-no-cache up down clean test

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

# Full test: generate certs, build, start, initiate tunnel, test connectivity
test: build
	@echo "========================================="
	@echo "QKD strongSwan Integration Test Suite"
	@echo "========================================="
	@echo ""
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
	@echo "========================================="
	@echo "Tunnel Status (Alice):"
	@echo "========================================="
	@docker exec qkd-alice swanctl --list-sas
	@echo ""
	@echo "========================================="
	@echo "Tunnel Status (Bob):"
	@echo "========================================="
	@docker exec qkd-bob swanctl --list-sas
	@echo ""
	@echo "========================================="
	@echo "Ping Test Results:"
	@echo "========================================="
	@cat /tmp/qkd-ping-test.log
	@echo ""
	@echo "========================================="
	@echo "Test Summary:"
	@echo "========================================="
	@if grep -q "0% packet loss" /tmp/qkd-ping-test.log; then \
		echo "  ✓ Tunnel Status: ESTABLISHED"; \
		echo "  ✓ Connectivity: SUCCESS (0% packet loss)"; \
		echo "  ✓ Overall: PASS"; \
		echo ""; \
		echo "All tests passed! QKD tunnel is operational."; \
		TEST_RESULT=0; \
	else \
		echo "  ✗ Tunnel Status: UNKNOWN"; \
		echo "  ✗ Connectivity: FAILED"; \
		echo "  ✗ Overall: FAIL"; \
		echo ""; \
		echo "Tests failed. Check logs above for details."; \
		TEST_RESULT=1; \
	fi; \
	echo "========================================="; \
	echo ""; \
	echo "Cleaning up..."; \
	docker compose down -v; \
	rm -f /tmp/qkd-ping-test.log; \
	exit $$TEST_RESULT
