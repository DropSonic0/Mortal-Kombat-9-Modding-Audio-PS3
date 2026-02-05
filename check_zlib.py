import zlib

with open('tmp/SND_MOV_MK063_SPA.XXX', 'rb') as f:
    f.seek(0x5f15)
    data = f.read()
    try:
        decompressed = zlib.decompress(data)
        print(f"Decompressed {len(decompressed)} bytes from 0x5f15")
        if b'FSB' in decompressed:
            print("Found FSB in decompressed data!")
    except Exception as e:
        print(f"Failed at 0x5f15: {e}")
