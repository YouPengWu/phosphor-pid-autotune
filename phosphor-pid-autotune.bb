# fans/phosphor-pid-autotune.bb (Meson-based)
# Builds and installs the autotune daemon, its systemd unit,
# and a fallback JSON config. Prefers EntityManager at runtime.

SUMMARY = "phosphor-pid-autotune"
DESCRIPTION = "Run base-duty/step experiments, identify FOPDT, and compute IMC PID gains."
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

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

# Systemd integration
SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE:${PN} = "phosphor-pid-autotune.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

