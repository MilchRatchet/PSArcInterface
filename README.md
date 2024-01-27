# PSArcInterface

PSArcInterface is a library for interfacing PlayStation archive (PSArc) files. These archives were commonly used by PS3 games and sometimes by PS Vita and PS4 games. This project contains a C++20 library and a commandline executable.

The library is currently still incomplete with the following features confirmed missing:
- zlib de-/compression
- big endian host machine support

## Confirmed supported games

Archives from the following games have been tested:
- Ratchet and Clank 1 (NPEA00385) - Unpacking
- Ratchet and Clank 2 (NPEA00386) - Unpacking
- Ratchet and Clank 3 (NPEA00387) - Unpacking

# PSArc-cl

PSArc-cl is a commandline executable that allows packing and unpacking of Playstation archive files using LibPSArc. Builds are available for download for Windows and Linux platforms:

- [Release Builds](https://github.com/MilchRatchet/PSArcInterface/releases)
- [CI Builds (Unstable)](https://github.com/MilchRatchet/PSArcInterface/releases/tag/unstable)

## Usage

To decompress an archive, put the PSArc-cl executable into the same directory as the archive and start PSArc-cl. The content of the archive will then automatically be extracted.

Alternatively, PSArc-cl can be used through a commandline:
```
USAGE: psarc-cl.exe mode input-path output-path

MODE:
  pack      Pack all files in a directory into a PSArc file.
  unpack    Unpack all files in a PSArc file into a directory.
```

# LibPSArc

LibPSArc is a C++20 library that implements an interface to a Playstation archive file. Currently, the API is not stable and may change over time. LibPSArc installs as a CMake package that contains the following components.

| Component name | Description   |
|----------|-------|
| Headers | The LibPSArc headers, available through the `LibPSArc::Headers` target. |
| Static | The LibPSArc static library, available through the `LibPSArc::Static` target. |
| Shared | The LibPSArc shared library, available through the `LibPSArc::Shared` target. |

## Build and Install

```
mkdir build
cd build
cmake ../ -G Ninja -DCMAKE_INSTALL_PREFIX={Installation directory}
ninja install
```
