#!/bin/bash
#
# HP Chromebook x360 11 G1 EE (Snappy) Audio Fix Installer
# 
# This script builds and installs the patched snd_soc_sof_da7219 kernel module
# to enable audio on Chromebooks with DA7219 + MAX98357A audio hardware.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_DIR="$SCRIPT_DIR/module"
CONFIG_DIR="$SCRIPT_DIR/config"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_requirements() {
    echo_info "Checking requirements..."
    
    # Check for root (needed for installation, not for building)
    if [ "$EUID" -eq 0 ]; then
        echo_warn "Running as root. This is OK for installation."
    fi
    
    # Check for build tools
    if ! command -v make &> /dev/null; then
        echo_error "make not found. Please install build-essential:"
        echo "    sudo apt install build-essential"
        exit 1
    fi
    
    if ! command -v gcc &> /dev/null; then
        echo_error "gcc not found. Please install build-essential:"
        echo "    sudo apt install build-essential"
        exit 1
    fi
    
    # Check for kernel headers
    KERNEL_VERSION=$(uname -r)
    if [ ! -d "/lib/modules/$KERNEL_VERSION/build" ]; then
        echo_error "Kernel headers not found for $KERNEL_VERSION"
        echo "    sudo apt install linux-headers-$KERNEL_VERSION"
        exit 1
    fi
    
    echo_info "All requirements satisfied."
}

build_module() {
    echo_info "Building kernel module..."
    
    cd "$MODULE_DIR"
    
    # Clean previous build
    make clean 2>/dev/null || true
    
    # Build
    if make; then
        echo_info "Module built successfully."
    else
        echo_error "Module build failed!"
        exit 1
    fi
    
    cd "$SCRIPT_DIR"
}

install_module() {
    echo_info "Installing kernel module..."

    KERNEL_VERSION=$(uname -r)

    sudo make -C "$MODULE_DIR" install

    echo_info "Module installed to /lib/modules/$KERNEL_VERSION/updates/"
}

install_config() {
    echo_info "Installing modprobe configuration..."
    
    # Backup existing config if present
    if [ -f /etc/modprobe.d/audio-hda.conf ]; then
        sudo cp /etc/modprobe.d/audio-hda.conf /etc/modprobe.d/audio-hda.conf.bak
        echo_info "Backed up existing config to audio-hda.conf.bak"
    fi
    
    # Install new config
    sudo cp "$CONFIG_DIR/audio-hda.conf" /etc/modprobe.d/
    
    echo_info "Configuration installed."
}

cleanup_old_fixes() {
    echo_info "Cleaning up any old fix attempts..."
    
    # Remove old class fix module if present
    sudo rm -f /etc/modules-load.d/pci-class-fix.conf 2>/dev/null || true
    sudo rm -f /etc/modprobe.d/audio-bxt-alias.conf 2>/dev/null || true
    
    # Remove old class fix kernel module
    KERNEL_VERSION=$(uname -r)
    sudo rm -f "/lib/modules/$KERNEL_VERSION/extra/pci_class_fix.ko" 2>/dev/null || true
    
    echo_info "Cleanup complete."
}

verify_installation() {
    echo_info "Verifying installation..."
    
    # Check module is in the right place
    KERNEL_VERSION=$(uname -r)
    if [ -f "/lib/modules/$KERNEL_VERSION/updates/snd-soc-sof_da7219.ko" ]; then
        echo_info "Module file: OK"
    else
        echo_error "Module file not found!"
        return 1
    fi
    
    # Check config
    if [ -f /etc/modprobe.d/audio-hda.conf ]; then
        if grep -q "dsp_driver=3" /etc/modprobe.d/audio-hda.conf; then
            echo_info "Modprobe config: OK"
        else
            echo_warn "Modprobe config may not have correct dsp_driver setting"
        fi
    else
        echo_error "Modprobe config not found!"
        return 1
    fi
    
    # Check module info
    if modinfo snd_soc_sof_da7219 | grep -q "updates"; then
        echo_info "Module will load from updates directory: OK"
    else
        echo_warn "Module may load from original location. Run 'sudo depmod -a' again."
    fi
    
    echo_info "Verification complete."
}

print_post_install() {
    echo ""
    echo "=========================================="
    echo -e "${GREEN}Installation Complete!${NC}"
    echo "=========================================="
    echo ""
    echo "Please reboot your system to apply changes:"
    echo ""
    echo "    sudo reboot"
    echo ""
    echo "After reboot, verify audio is working:"
    echo ""
    echo "    cat /proc/asound/cards"
    echo "    aplay -l"
    echo "    speaker-test -D plughw:0,0 -c 2 -t sine -l 1"
    echo ""
    echo "If PipeWire shows 'Dummy Output', run:"
    echo ""
    echo "    systemctl --user restart pipewire pipewire-pulse wireplumber"
    echo "    pactl set-card-profile alsa_card.pci-0000_00_0e.0-platform-bxt_da7219_mx98357a pro-audio"
    echo ""
}

uninstall() {
    echo_info "Uninstalling audio fix..."

    # Remove module
    sudo make -C "$MODULE_DIR" uninstall

    # Remove config
    sudo rm -f /etc/modprobe.d/audio-hda.conf

    # Restore backup if present
    if [ -f /etc/modprobe.d/audio-hda.conf.bak ]; then
        sudo mv /etc/modprobe.d/audio-hda.conf.bak /etc/modprobe.d/audio-hda.conf
        echo_info "Restored previous config from backup."
    fi

    echo_info "Uninstall complete. Please reboot."
}

# Main
case "${1:-install}" in
    install)
        echo "=========================================="
        echo "HP Chromebook Snappy Audio Fix Installer"
        echo "=========================================="
        echo ""
        check_requirements
        build_module
        cleanup_old_fixes
        install_module
        install_config
        verify_installation
        print_post_install
        ;;
    build)
        check_requirements
        build_module
        echo_info "Build complete. Module is at: $MODULE_DIR/snd-soc-sof_da7219.ko"
        ;;
    uninstall)
        uninstall
        ;;
    *)
        echo "Usage: $0 {install|build|uninstall}"
        echo ""
        echo "  install   - Build and install the audio fix (default)"
        echo "  build     - Build the module only"
        echo "  uninstall - Remove the audio fix"
        exit 1
        ;;
esac
