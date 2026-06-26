import struct


def create_magenta_bc3_dds(filename: str = "solid_magenta_bc3.dds"):
    dwMagic = b"DDS "
    dwSize = 124  # Header Size
    # Flags: DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE
    #        0x1       | 0x2         | 0x4        | 0x1000           | 0x80000 = 0x81007
    dwFlags = 0x81007
    dwHeight = 16
    dwWidth = 16
    dwPitchOrLinearSize = 256
    dwDepth = 0
    dwMipMapCount = 1
    dwReserved1 = (0,) * 11
    ddpfSize = 32
    ddpfFlags = 0x4
    ddpfFourCC = b"DXT5"
    ddpfRGBBitCount = 0
    ddpfRBitMask = 0
    ddpfGBitMask = 0
    ddpfBBitMask = 0
    ddpfABitMask = 0
    dwCaps = 0x1000  # DDSCAPS_TEXTURE
    dwCaps2 = 0
    dwCaps3 = 0
    dwCaps4 = 0
    dwReserved2 = 0

    dds_header = struct.pack(
        "<4sIIIIIII11I"  # Magic, basic sizes, and first reserved array
        "II4sIIIII"  # DDPIXELFORMAT details
        "IIIII",  # Caps and final reserved field
        dwMagic,
        dwSize,
        dwFlags,
        dwHeight,
        dwWidth,
        dwPitchOrLinearSize,
        dwDepth,
        dwMipMapCount,
        *dwReserved1,
        ddpfSize,
        ddpfFlags,
        ddpfFourCC,
        ddpfRGBBitCount,
        ddpfRBitMask,
        ddpfGBitMask,
        ddpfBBitMask,
        ddpfABitMask,
        dwCaps,
        dwCaps2,
        dwCaps3,
        dwCaps4,
        dwReserved2,
    )

    single_block_bytes = struct.pack("<4I", 65535, 0, 4162844703, 0)
    bc3_payload = single_block_bytes * 16
    full_dds_file = dds_header + bc3_payload
    with open(filename, "wb") as f:
        f.write(full_dds_file)


create_magenta_bc3_dds()
