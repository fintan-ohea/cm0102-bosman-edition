"""Read a data folder's tables (index.dat entries with name, id, count, offset)."""
import struct, os

SIZES = {0: 581, 1: 581, 2: 58, 3: 198, 4: 290, 5: 78, 7: 43, 11: 101, 21: 56, 12: 107, 16: 107,
         13: 60, 14: 60, 15: 60, 17: 17, 18: 58, 19: 26, 20: 26, 6: 110, 9: 68, 10: 70, 22: 52}

def load(folder):
    idx = open(os.path.join(folder, "index.dat"), "rb").read()
    files, out = {}, {}
    for k in range(22):
        o = 8 + k * 67
        name = idx[o:o + 51].split(b"\0")[0].decode("latin-1")
        tid, cnt, off, fmt = struct.unpack_from("<4i", idx, o + 51)
        if name not in files:
            p = os.path.join(folder, name)
            files[name] = open(p, "rb").read() if os.path.exists(p) else b""
        size = SIZES[tid]
        b = files[name][off:off + cnt * size]
        out[tid] = [bytearray(b[i:i + size]) for i in range(0, len(b), size)]
    return out

def sname(r, a, b):
    return bytes(r[a:b]).split(b"\0")[0].decode("latin-1")

def i32(r, o): return struct.unpack_from("<i", r, o)[0]
def i16(r, o): return struct.unpack_from("<h", r, o)[0]
