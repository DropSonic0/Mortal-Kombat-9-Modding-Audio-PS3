import struct

with open('tmp/SND_MOV_MK063_SPA.XXX', 'rb') as f:
    f.seek(0x14)
    name_count = struct.unpack('>I', f.read(4))[0]
    print(f"Name Count: {name_count}")

    # Names start at 0x18
    for i in range(name_count):
        slen = struct.unpack('>I', f.read(4))[0]
        if slen > 1000: # try LE
            f.seek(-4, 1)
            slen = struct.unpack('<I', f.read(4))[0]
        name = f.read(slen).decode('ascii').strip('\0')
        flags = f.read(4) # flags?
        print(f"Name {i}: {name} (flags: {flags.hex()})")

    # What's next?
    pos = f.tell()
    print(f"Current Pos: {pos} (0x{pos:x})")
    f.seek(pos)
    import_count = struct.unpack('>I', f.read(4))[0]
    print(f"Import Count: {import_count}")
