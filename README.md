# HP Chromebook x360 11 G1 EE (Snappy) Audio Fix for Linux

This repository contains a patched kernel module and configuration files to enable audio on the HP Chromebook x360 11 G1 EE (codename "snappy") when running Linux with MrChromebox UEFI firmware.

## The Problem

When running Linux on this Chromebook with MrChromebox custom UEFI firmware, the audio hardware is detected but doesn't work. The Sound Open Firmware (SOF) driver loads successfully, but the machine driver (`snd_soc_sof_da7219`) doesn't recognize the platform device `bxt_da7219_mx98357a` created by SOF, leaving the system with no sound.

### Hardware Details

| Component | Details |
|-----------|---------|
| Board codename | snappy (HP Chromebook x360 11 G1 EE) |
| CPU | Intel Apollo Lake N3350 |
| Audio DSP | Intel 8086:5a98 |
| Headphone codec | Dialog DA7219 (I2C) |
| Speaker amplifier | Maxim MAX98357A (I2C) |
| SSP Configuration | SSP1 → DA7219, SSP5 → MAX98357A |

### Root Cause

The upstream `snd_soc_sof_da7219` machine driver only includes platform aliases for newer platforms (GLK, CML, JSL, ADL, RPL, MTL) but not for BXT/APL Chromebooks. When SOF creates the platform device `bxt_da7219_mx98357a`, no driver claims it.

## The Solution

This repository provides:

1. **Patched `snd_soc_sof_da7219` kernel module** - Adds support for `bxt_da7219_mx98357a` with correct SSP port configuration (SSP1 for codec, SSP5 for amp)
2. **Modprobe configuration** - Sets `dsp_driver=3` to force SOF driver selection
3. **Installation script** - Automates the build and installation process

## Requirements

- Linux kernel 6.x (tested on 6.14.0-37-generic)
- Ubuntu 24.04 / Linux Mint 22.x or compatible distribution
- Build tools: `build-essential`, `linux-headers-$(uname -r)`
- Internet connection (to download kernel source headers during build)

## Quick Installation

```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/chromebook-snappy-audio-fix.git
cd chromebook-snappy-audio-fix

# Install build dependencies
sudo apt install build-essential linux-headers-$(uname -r)

# Build and install
./install.sh

# Reboot
sudo reboot
```

## Manual Installation

### Step 1: Build the patched module

```bash
cd module
make
```

### Step 2: Install the module

```bash
sudo mkdir -p /lib/modules/$(uname -r)/updates
sudo cp snd-soc-sof_da7219.ko /lib/modules/$(uname -r)/updates/
sudo depmod -a
```

### Step 3: Configure modprobe

```bash
sudo cp ../config/audio-hda.conf /etc/modprobe.d/
```

### Step 4: Reboot

```bash
sudo reboot
```

## Post-Installation

After rebooting, verify audio is working:

```bash
# Check sound card is detected
cat /proc/asound/cards

# Should show:
#  0 [sofbxtda7219max]: sof-bxtda7219ma - sof-bxtda7219max
#                       Google-Snappy-rev7

# List playback devices
aplay -l

# Should show Speakers, Headset, and HDMI outputs

# Test speakers
speaker-test -D plughw:0,0 -c 2 -t sine -l 1
```

### PipeWire/PulseAudio Configuration

If PipeWire shows "Dummy Output" after installation:

```bash
# Restart PipeWire
systemctl --user restart pipewire pipewire-pulse wireplumber

# Set the card profile to pro-audio
pactl set-card-profile alsa_card.pci-0000_00_0e.0-platform-bxt_da7219_mx98357a pro-audio

# Check sinks
pactl list sinks short
```

## What's Included

```
chromebook-snappy-audio-fix/
├── README.md                 # This file
├── LICENSE                   # MIT License
├── install.sh               # Automated installation script
├── config/
│   └── audio-hda.conf       # Modprobe configuration (dsp_driver=3)
├── module/
│   ├── Makefile             # Build configuration
│   ├── sof_da7219.c         # Patched machine driver source
│   ├── sof_board_helpers.h  # Required header
│   ├── sof_maxim_common.h   # Required header
│   ├── sof_hdmi_common.h    # Required header
│   ├── sof_realtek_common.h # Required header
│   ├── sof_nuvoton_common.h # Required header
│   ├── sof_cirrus_common.h  # Required header
│   └── da7219.h             # DA7219 codec header
└── docs/
    └── TROUBLESHOOTING.md   # Detailed troubleshooting guide
```

## Technical Details

### Patch Summary

The patch adds the following to `sof_da7219.c`:

1. **New board flag**: `SOF_DA7219_BXT_BOARD` (BIT 4)

2. **New platform ID entry**:
```c
{
    .name = "bxt_da7219_mx98357a",
    .driver_data = (kernel_ulong_t)(SOF_DA7219_BXT_BOARD |
                    SOF_SSP_PORT_CODEC(1) |
                    SOF_SSP_PORT_AMP(5)),
},
```

3. **Board-specific handling**:
```c
if (board_quirk & SOF_DA7219_BXT_BOARD) {
    ctx->dmic_be_num = 1;
    ctx->link_order_overwrite = GLK_LINK_ORDER;
    // ... card name setup
}
```

### Why SSP1 and SSP5?

The NHLT (Non-HD Audio Link Table) from the ACPI firmware reports `ssp_mask 0x22`, which corresponds to SSP1 and SSP5. The DA7219 headphone codec is connected to SSP1, and the MAX98357A speaker amplifier is connected to SSP5.

## Troubleshooting

See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) for detailed troubleshooting steps.

### Common Issues

**No sound card detected after reboot:**
```bash
# Check if module loaded
lsmod | grep sof_da7219

# Check dmesg for errors
dmesg | grep -iE "sof|da7219"

# Verify dsp_driver setting
cat /sys/module/snd_intel_dspcfg/parameters/dsp_driver
# Should be 3
```

**Sound card detected but no audio:**
```bash
# Unmute and set volume
amixer -c 0 set 'Headphone' unmute
amixer -c 0 set 'Headphone' 80%

# Check PipeWire profile
pactl list cards | grep "Active Profile"
```

## Kernel Version Compatibility

This module was developed and tested on kernel 6.14.0-37-generic. For other kernel versions:

- The source files may need updates if the kernel API changes
- You must rebuild the module after kernel updates:
  ```bash
  cd module
  make clean
  make
  sudo make install
  sudo depmod -a
  ```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

The kernel module source code is derived from the Linux kernel and is dual-licensed under GPL-2.0 and BSD-3-Clause.

## Acknowledgments

- The Linux kernel SOF (Sound Open Firmware) team
- MrChromebox for the UEFI firmware
- The ChromeOS audio team for the original hardware support

## Related Links

- [MrChromebox Firmware](https://mrchromebox.tech/)
- [Sound Open Firmware Project](https://thesofproject.github.io/)
- [Arch Wiki - Chromebook](https://wiki.archlinux.org/title/Chrome_OS_devices)
