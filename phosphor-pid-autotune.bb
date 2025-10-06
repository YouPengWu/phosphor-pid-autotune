# fans/phosphor-pid-autotune.bb (Meson-based)
# Builds and installs the autotune daemon, its systemd unit,
# and a fallback JSON config. Prefers EntityManager at runtime.

SUMMARY = "phosphor-pid-autotune"
DESCRIPTION = "Run base-duty/step experiments, identify FOPDT, and compute IMC PID gains."
LICENSE = "CLOSED"
LIC_FILES_CHKSUM = ""

# Source is shipped inside this layer (local files).
FILESEXTRAPATHS:prepend := "${THISDIR}/:"

# Pull the whole project tree (meson.build must be at repo root below).
SRC_URI = "file://phosphor-pid-autotune"
S = "${WORKDIR}/phosphor-pid-autotune"

inherit meson pkgconfig systemd

# Build-time deps:
# - sdbusplus: dbus bindings & helpers
# - systemd: headers for unit/daemon integration
# - nlohmann-json: JSON parsing
# - boost: asio, etc. (used by the codebase)
DEPENDS += "sdbusplus systemd nlohmann-json boost"

# Minimal runtime deps
RDEPENDS:${PN} += "systemd"

do_install:append() {
    # Install systemd unit (if not already installed by Meson)
    install -d ${D}${systemd_unitdir}/system
    if [ -f ${S}/phosphor-pid-autotune.service ]; then
        install -m 0644 ${S}/phosphor-pid-autotune.service \
            ${D}${systemd_unitdir}/system/
    fi

    # Install fallback JSON config (matches default path in main.cpp)
    install -d ${D}${datadir}/phosphor-pid-autotune/configs
    if [ -f ${S}/configs/autotune.json ]; then
        install -m 0644 ${S}/configs/autotune.json \
            ${D}${datadir}/phosphor-pid-autotune/configs/autotune.json
    fi
}

# Systemd integration
SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE:${PN} = "phosphor-pid-autotune.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

# Package contents
FILES:${PN} += " \
    ${bindir}/phosphor-pid-autotune \
    ${systemd_unitdir}/system/phosphor-pid-autotune.service \
    ${datadir}/phosphor-pid-autotune/configs/autotune.json \
"
