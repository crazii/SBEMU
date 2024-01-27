
# User instructions

## Available files

If you wish to use SBEMU and its dependencies in an existing DOS installation, you'll find the necessary
files in `SBEMU.zip`.

Alternatively, `SBEMU-FD13-USB.img.xz` provides SBEMU and is dependencies preconfigured inside a compressed
bootable FreeDOS image that you can write to a USB flash drive or an SD card.

<details>
<summary>Preparing a bootable USB drive</summary>

## Preparing a bootable USB drive

The USB image can be written to a USB drive or SD card using a tool like [balenaEtcher](https://etcher.balena.io/).

The advantage of using Etcher is that you don't have to decompress the `.xz` archive first.
It will decompress such files automatically, before writing the image to the target drive.
</details>
<details>
<summary>Booting the USB image in a virtual machine</summary>

## Booting the USB image in a virtual machine

You can run the image in a VM with QEMU as follows:

```shell
unxz SBEMU-FD13-USB.img.xz
qemu-system-i386 -drive file=SBEMU-FD13-USB.img,format=raw -device AC97
```

If you wish to test Intel HDA compatibility instead of ICHx AC'97 compatibility, replace `AC97` with `intel-hda` in the last command above.
On Linux, you can include the parameter `--enable-kvm` to run the VM with hardware-assisted virtualization.

If you prefer to use another hypervisor, such as VirtualBox or VMware, you may have to convert the raw image to a supported VM image format first:

```shell
unxz SBEMU-FD13-USB.img.xz
qemu-img convert -f raw -O vmdk SBEMU-FD13-USB.img SBEMU-FD13-USB.vmdk
```

**NOTE**: Although VMs can sometimes be useful during development, testing and debugging, you should not rely on those for actual hardware compatibility testing, since the sound cards that the hypervisors emulate are themselves merely approximations of actual hardware, and will not behave like the real thing in every single corner case.
Basically, you shouldn't test emulators on other emulators.
</details>
<details>
<summary>Where can I get some DOS games to test with?</summary>

## Where can I get some DOS games to test with?

There are multiple convenient distributions out there that contain DOS games that can be distributed freely and legally.
Specifically freeware, shareware, open source and free demo versions.

Here are a few links to such distributions:

- [The PC/DOS Mini](http://vieju.net/pcdosmini/), a compilation of 100+ DOS games ready to play for free
- [GAFFA DOS Shareware/Freeware Pack](https://archive.org/details/gaffa-dos-shareware-pack) (please [donate to the Internet Archive](https://archive.org/donate/), by the way!️ ❤️)
</details>
