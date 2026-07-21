#!/bin/bash
# macOS installer for the Clawdmeter daemon (native Swift + CoreBluetooth + launchd).
# Builds a signed binary that owns its own Bluetooth (TCC) identity — no Python,
# no venv, no .app wrapper.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVICE_LABEL="com.user.claude-usage-daemon"
PLIST_SRC="$SCRIPT_DIR/daemon/$SERVICE_LABEL.plist"
PLIST_DST="$HOME/Library/LaunchAgents/$SERVICE_LABEL.plist"
SWIFT_PKG="$SCRIPT_DIR/daemon/ClawdmeterDaemon"
BIN_BUILT="$SWIFT_PKG/.build/release/ClawdmeterDaemon"
DAEMON_BIN="$SCRIPT_DIR/daemon/clawdmeter-daemon"   # stable install path (survives swift clean)
BUNDLE_ID="software.playful.clawdmeter-daemon"
LOG_DIR="$HOME/Library/Logs"
LOG_OUT="$LOG_DIR/claude-usage-daemon.out.log"
LOG_ERR="$LOG_DIR/claude-usage-daemon.err.log"

echo "=== Clawdmeter macOS install (Swift daemon) ==="
echo ""

echo "[1/5] Checking prerequisites..."
for cmd in swift codesign; do
    command -v "$cmd" >/dev/null || { echo "Error: $cmd is required (ships with Xcode Command Line Tools: xcode-select --install)"; exit 1; }
done
if ! security find-generic-password -s "Claude Code-credentials" -a "$USER" -w >/dev/null 2>&1; then
    echo "Warning: Claude Code OAuth token not found in Keychain (service 'Claude Code-credentials')."
    echo "  Sign in via Claude Code first, then re-run this installer."
    echo "  Continuing anyway — the daemon will retry on each poll."
fi
if ! command -v blueutil >/dev/null && [ ! -x /opt/homebrew/bin/blueutil ] && [ ! -x /usr/local/bin/blueutil ]; then
    echo "Note: blueutil not found. It's optional — only used to auto-forget a stale"
    echo "  Bluetooth bond after a firmware reflash. Install with: brew install blueutil"
fi
echo "  OK"
echo ""

# The binary carries its Bluetooth usage string + bundle id in an embedded
# Info.plist (see Package.swift linker flags); the ad-hoc signature gives it a
# stable cdhash so the granted permission sticks across launches.
echo "[2/5] Building + signing the Swift daemon..."
swift build -c release --package-path "$SWIFT_PKG"
cp "$BIN_BUILT" "$DAEMON_BIN"
codesign --force --sign - --identifier "$BUNDLE_ID" "$DAEMON_BIN"
echo "  Built & signed: $DAEMON_BIN"
echo ""

echo "[3/5] Rendering launchd plist..."
mkdir -p "$HOME/Library/LaunchAgents" "$LOG_DIR"
sed \
    -e "s|__DAEMON_BIN__|${DAEMON_BIN}|g" \
    -e "s|__REPO_DIR__|${SCRIPT_DIR}|g" \
    -e "s|__LOG_OUT__|${LOG_OUT}|g" \
    -e "s|__LOG_ERR__|${LOG_ERR}|g" \
    -e "s|__HOME__|${HOME}|g" \
    "$PLIST_SRC" > "$PLIST_DST"
echo "  Installed: $PLIST_DST"
echo ""

echo "[4/5] Loading launchd service..."
launchctl unload "$PLIST_DST" 2>/dev/null || true
launchctl load -w "$PLIST_DST"
echo "  Loaded."
echo ""

echo "[5/5] Bluetooth permission..."
echo "  The daemon is now running under launchd. The FIRST time it touches"
echo "  Bluetooth, macOS shows a permission prompt — click Allow. If you miss it"
echo "  (or it's denied), enable it manually at:"
echo "    System Settings → Privacy & Security → Bluetooth → Clawdmeter"
echo "  No terminal needs to stay open; the grant persists across reboots."
echo ""

echo "=== Done ==="
echo ""
echo "First-time Bluetooth pairing (after firmware is flashed):"
echo "  1. Power on the device."
echo "  2. Open System Settings → Bluetooth."
echo "  3. Click 'Connect' next to 'Clawdmeter'."
echo "  4. The daemon will discover it within ~30 s and start polling."
echo ""
echo "Useful commands:"
echo "  launchctl list | grep claude-usage     # check it's running"
echo "  tail -F $LOG_OUT                       # live logs"
echo "  launchctl unload $PLIST_DST            # stop"
echo "  launchctl load -w $PLIST_DST           # start"
