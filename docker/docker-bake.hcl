// Docker Buildx Bake configuration for QKD strongSwan stack
// Usage: docker buildx bake -f docker-bake.hcl --builder qkd-builder

variable "STRONGSWAN_VERSION" {
  default = "6.0.0beta6"
}

variable "ETSI_API_VERSION" {
  default = "014"
}

variable "QKD_INITIATION_MODE" {
  default = "client"
}

group "default" {
  targets = ["alice", "bob", "kme"]
}

target "alice" {
  context = ".."
  dockerfile = "docker/Dockerfile.runtime"
  tags = ["qkd-alice:latest"]
  args = {
    STRONGSWAN_VERSION = "${STRONGSWAN_VERSION}"
    ETSI_API_VERSION = "${ETSI_API_VERSION}"
    QKD_INITIATION_MODE = "${QKD_INITIATION_MODE}"
  }
  no-cache = true
}

target "bob" {
  context = ".."
  dockerfile = "docker/Dockerfile.runtime"
  tags = ["qkd-bob:latest"]
  args = {
    STRONGSWAN_VERSION = "${STRONGSWAN_VERSION}"
    ETSI_API_VERSION = "${ETSI_API_VERSION}"
    QKD_INITIATION_MODE = "${QKD_INITIATION_MODE}"
  }
  no-cache = true
}

target "kme" {
  context = ".."
  dockerfile = "docker/Dockerfile.kme"
  tags = ["qkd-kme:latest"]
  no-cache = true
}
