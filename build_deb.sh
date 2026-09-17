#!/usr/bin/env bash
# Build script for the semitan-height-measure .deb package.
# See docs/superpowers/specs/2026-05-07-multiple-lidar-deb-packaging-design.md
set -euo pipefail

readonly PKG_NAME="semitan-height-measure"
readonly PKG_DESC="Multiple lidar acquisition service for tramway pavement height measurement"
readonly PKG_MAINTAINER="Quentin Bossard <quentin.bossard@stimio.fr>"
readonly PKG_DEPENDS="libc6, libstdc++6"

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="${REPO_ROOT}/build"
readonly STAGE_DIR="${BUILD_DIR}/pkg"
readonly SERVICES_DIR="${REPO_ROOT}/services/multiple_lidar"

log() { printf '[build_deb] %s\n' "$*"; }
fail() { printf '[build_deb] ERROR: %s\n' "$*" >&2; exit 1; }

check_tool() {
    command -v "$1" >/dev/null 2>&1 || fail "required tool '$1' not found in PATH"
}

detect_arch() {
    dpkg --print-architecture
}

detect_version() {
    local v
    if ! v=$(git describe --tags --always --dirty 2>/dev/null) || [ -z "${v}" ]; then
        v="0.0.0-$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
    fi
    # Debian policy: Version must start with a digit. If git describe falls
    # back to a bare SHA (no tag), prepend a synthetic 0.0.0- prefix.
    case "${v}" in
        [0-9]*) ;;
        *)      v="0.0.0-${v}" ;;
    esac
    printf '%s' "${v}"
}

build_binary() {
    log "Configuring CMake (if needed)..."
    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
        cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}"
    fi

    log "Building target: multiple_lidar"
    cmake --build "${BUILD_DIR}" --target multiple_lidar -j"$(nproc)"

    [ -x "${BUILD_DIR}/multiple_lidar" ] || fail "build/multiple_lidar not produced"
    log "Built: ${BUILD_DIR}/multiple_lidar"
}

build_deb() {
    local arch="$1"
    local version="$2"
    local out="${BUILD_DIR}/${PKG_NAME}_${version}_${arch}.deb"

    log "Building .deb: ${out}"
    dpkg-deb --build --root-owner-group "${STAGE_DIR}" "${out}"

    log "----- dpkg-deb --info -----"
    dpkg-deb --info "${out}"
    log "----- dpkg-deb --contents -----"
    dpkg-deb --contents "${out}"

    if command -v lintian >/dev/null 2>&1; then
        log "----- lintian (informational) -----"
        lintian "${out}" || true
    fi

    log "Done. Package: ${out}"
    log "Install with: sudo dpkg -i ${out}"
}

write_maintainer_scripts() {
    log "Writing DEBIAN/postinst"
    cat > "${STAGE_DIR}/DEBIAN/postinst" <<'EOF'
#!/bin/bash
set -e
if [ "$1" = "configure" ]; then
    systemctl daemon-reload || true
fi
exit 0
EOF
    chmod 0755 "${STAGE_DIR}/DEBIAN/postinst"

    log "Writing DEBIAN/postrm"
    cat > "${STAGE_DIR}/DEBIAN/postrm" <<'EOF'
#!/bin/bash
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    systemctl stop 'multiple_lidar_barre_*.target' 2>/dev/null || true
    systemctl daemon-reload || true
fi
exit 0
EOF
    chmod 0755 "${STAGE_DIR}/DEBIAN/postrm"
}

write_control() {
    local arch="$1"
    local version="$2"
    log "Writing DEBIAN/control"
    cat > "${STAGE_DIR}/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${version}
Section: misc
Priority: optional
Architecture: ${arch}
Depends: ${PKG_DEPENDS}
Maintainer: ${PKG_MAINTAINER}
Description: ${PKG_DESC}
 Provides the multiple_lidar binary and systemd unit files to drive
 several YDLidar devices in parallel for tramway pavement height
 measurement on the Stimio bench.
EOF
    chmod 0644 "${STAGE_DIR}/DEBIAN/control"
}

stage_files() {
    log "Cleaning stage dir: ${STAGE_DIR}"
    rm -rf "${STAGE_DIR}"

    log "Creating package layout"
    mkdir -p "${STAGE_DIR}/DEBIAN"
    mkdir -p "${STAGE_DIR}/usr/bin"
    mkdir -p "${STAGE_DIR}/usr/share/doc/${PKG_NAME}"
    mkdir -p "${STAGE_DIR}/etc/systemd/system"

    log "Copying binary"
    install -m 0755 "${BUILD_DIR}/multiple_lidar" "${STAGE_DIR}/usr/bin/multiple_lidar"

    log "Copying systemd unit files"
    shopt -s nullglob
    local unit
    local count=0
    for unit in "${SERVICES_DIR}"/*.service "${SERVICES_DIR}"/*.target; do
        install -m 0644 "${unit}" "${STAGE_DIR}/etc/systemd/system/$(basename "${unit}")"
        count=$((count + 1))
    done
    shopt -u nullglob
    log "Installed ${count} systemd unit file(s)"
    [ "${count}" -gt 0 ] || fail "no unit files were copied"

    log "Copying readme"
    if [ -f "${SERVICES_DIR}/readme.md" ]; then
        install -m 0644 "${SERVICES_DIR}/readme.md" "${STAGE_DIR}/usr/share/doc/${PKG_NAME}/readme.md"
    else
        fail "missing ${SERVICES_DIR}/readme.md"
    fi
}

preflight() {
    log "Checking required tools..."
    check_tool cmake
    check_tool make
    check_tool g++
    check_tool dpkg-deb
    check_tool dpkg
    check_tool git

    [ -d "${SERVICES_DIR}" ] || fail "expected ${SERVICES_DIR} (run from repo root)"

    shopt -s nullglob
    local services=( "${SERVICES_DIR}"/*.service )
    shopt -u nullglob
    [ "${#services[@]}" -gt 0 ] || fail "no *.service files found in ${SERVICES_DIR}"
}

main() {
    preflight
    local arch version
    arch=$(detect_arch)
    version=$(detect_version)
    log "Architecture: ${arch}"
    log "Version:      ${version}"

    build_binary
    stage_files
    write_control "${arch}" "${version}"
    write_maintainer_scripts
    build_deb "${arch}" "${version}"
}

main "$@"
