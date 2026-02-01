# Troubleshooting Guide

This guide covers common issues and their solutions when using the HP Chromebook Snappy audio fix.

## Table of Contents

1. [No Sound Card Detected](#no-sound-card-detected)
2. [Sound Card Present But No Audio](#sound-card-present-but-no-audio)
3. [PipeWire Shows Dummy Output](#pipewire-shows-dummy-output)
4. [Module Build Failures](#module-build-failures)
5. [Audio After Kernel Updates](#audio-after-kernel-updates)
6. [Debugging Commands](#debugging-commands)

---

## No Sound Card Detected

After installation and reboot, if `cat /proc/asound/cards` shows "no soundcards", check the following:

### 1. Verify the Module Loaded

```bash
lsmod | grep sof_da7219
```

**Expected output:**
```
snd_soc_sof_da7219     20480  1
```

**If module is not loaded:**
```bash
sudo modprobe snd_soc_sof_da7219
dmesg | tail -20
```

### 2. Verify dsp_driver Setting

```bash
cat /sys/module/snd_intel_dspcfg/parameters/dsp_driver
```

**Expected output:** `3`

**If not 3:**
```bash
cat /etc/modprobe.d/audio-hda.conf
# Should contain: options snd_intel_dspcfg dsp_driver=3
```

### 3. Check if SOF is Bound to the Device

```bash
ls -la /sys/bus/pci/devices/0000:00:0e.0/driver
```

**Expected output:** Link to `sof-audio-pci-intel-apl`

**If no driver bound:**
```bash
dmesg | grep -iE "sof|audio|0e.0"
```

### 4. Check for Module Conflicts

```bash
lsmod | grep -E "snd_hda_intel|snd_soc_avs"
```

If `snd_soc_avs` is loaded, blacklist it:
```bash
echo "blacklist snd_soc_avs" | sudo tee /etc/modprobe.d/blacklist-avs.conf
sudo reboot
```

---

## Sound Card Present But No Audio

If the sound card is detected but no audio plays:

### 1. Check ALSA Mixer Settings

```bash
# List controls
amixer -c 0 contents | grep -E "Volume|Switch" | head -20

# Unmute and set volume
amixer -c 0 set 'Headphone' unmute
amixer -c 0 set 'Headphone' 80%
```

### 2. Test Directly with ALSA

```bash
# Test speakers (device 0)
speaker-test -D plughw:0,0 -c 2 -t sine -l 1

# Test headphones (device 1)
speaker-test -D plughw:0,1 -c 2 -t sine -l 1
```

### 3. Check for IPC Errors

```bash
dmesg | grep -i "ipc.*error"
```

If you see IPC errors, the DSP firmware may have issues. Try:
```bash
sudo modprobe -r snd_sof_pci_intel_apl
sudo modprobe snd_sof_pci_intel_apl
```

---

## PipeWire Shows Dummy Output

If PipeWire/PulseAudio shows "Dummy Output" instead of the sound card:

### 1. Restart PipeWire

```bash
systemctl --user restart pipewire pipewire-pulse wireplumber
```

### 2. Check Card Profile

```bash
pactl list cards
```

Look for `Active Profile: off`. If so:

```bash
pactl set-card-profile alsa_card.pci-0000_00_0e.0-platform-bxt_da7219_mx98357a pro-audio
```

### 3. Check for WirePlumber Errors

```bash
journalctl --user -u wireplumber -n 50 --no-pager
```

### 4. Verify Sinks Exist

```bash
pactl list sinks short
```

If only "auto_null" appears:
```bash
wpctl status
```

---

## Module Build Failures

### Missing Kernel Headers

```
fatal error: linux/module.h: No such file or directory
```

**Solution:**
```bash
sudo apt install linux-headers-$(uname -r)
```

### Missing Build Tools

```
make: command not found
```

**Solution:**
```bash
sudo apt install build-essential
```

### Symbol Version Mismatch

```
disagrees about version of symbol module_layout
```

**Solution:** Rebuild for your current kernel:
```bash
cd module
make clean
make
sudo make install
sudo depmod -a
```

---

## Audio After Kernel Updates

After kernel updates, the module needs to be rebuilt:

### Automatic DKMS Setup (Recommended)

Create a DKMS configuration:

```bash
# Create DKMS directory
sudo mkdir -p /usr/src/sof-da7219-1.0

# Copy source files
sudo cp module/* /usr/src/sof-da7219-1.0/

# Create DKMS config
sudo tee /usr/src/sof-da7219-1.0/dkms.conf << EOF
PACKAGE_NAME="sof-da7219"
PACKAGE_VERSION="1.0"
BUILT_MODULE_NAME[0]="snd-soc-sof_da7219"
DEST_MODULE_LOCATION[0]="/updates"
AUTOINSTALL="yes"
MAKE[0]="make KDIR=/lib/modules/\${kernelver}/build"
CLEAN="make clean"
EOF

# Register with DKMS
sudo dkms add -m sof-da7219 -v 1.0
sudo dkms build -m sof-da7219 -v 1.0
sudo dkms install -m sof-da7219 -v 1.0
```

### Manual Rebuild

After each kernel update:
```bash
cd /path/to/chromebook-snappy-audio-fix/module
make clean
make
sudo make install
sudo depmod -a
sudo reboot
```

---

## Debugging Commands

### Full System Audio Status

```bash
#!/bin/bash
echo "=== ALSA Cards ==="
cat /proc/asound/cards

echo -e "\n=== ALSA Devices ==="
aplay -l

echo -e "\n=== Loaded Audio Modules ==="
lsmod | grep -E "snd|sof" | sort

echo -e "\n=== DSP Driver Setting ==="
cat /sys/module/snd_intel_dspcfg/parameters/dsp_driver

echo -e "\n=== PCI Audio Device ==="
lspci -vnn -s 00:0e.0 | head -10

echo -e "\n=== Driver Bound ==="
ls -la /sys/bus/pci/devices/0000:00:0e.0/driver 2>&1

echo -e "\n=== Recent Audio Messages ==="
dmesg | grep -iE "sof|da7219|audio|snd" | tail -20

echo -e "\n=== PipeWire Status ==="
pactl info 2>/dev/null | head -5
pactl list sinks short 2>/dev/null
```

### Collect Debug Information

```bash
# Save debug info to file
{
    echo "Date: $(date)"
    echo "Kernel: $(uname -a)"
    echo ""
    cat /proc/asound/cards
    echo ""
    aplay -l
    echo ""
    lsmod | grep -E "snd|sof"
    echo ""
    dmesg | grep -iE "sof|da7219|audio" | tail -50
} > audio-debug.txt
```

---

## Getting Help

If you're still having issues:

1. Run the debug script above and save the output
2. Check dmesg for specific error messages
3. Open an issue on the GitHub repository with:
   - Your kernel version (`uname -a`)
   - Contents of audio-debug.txt
   - Steps you've already tried

## Related Resources

- [ALSA Project](https://www.alsa-project.org/)
- [PipeWire Documentation](https://pipewire.pages.freedesktop.org/pipewire/)
- [SOF Project](https://thesofproject.github.io/)
- [Arch Wiki - Sound](https://wiki.archlinux.org/title/Sound_system)
