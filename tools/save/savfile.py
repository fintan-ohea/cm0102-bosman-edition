"""Read and write the container of a CM 01/02 saved game (.sav).

Format (zip.cpp of the original, cm0102.exe 3.9.68):
- Header of 0x416ec bytes at offset 0: int version, int unknown, int file count, then 1000 entries of
  0x10c bytes: int offset, int size (uncompressed), char name[260]. Read at 0x9326e0, written at 0x932b00.
  Version 1 and 3 mean "stored", version 2 and 4 mean "compressed" (0x9326e0 maps 1 to 3 and 2 to 4).
- Each member file follows the header. Its offset is in its entry. A compressed member is a byte run
  code: a byte below 0x81 is one literal byte, a byte b of 0x81 or more is followed by one byte that
  repeats b - 0x80 times. Decoder 0x932c30, encoder 0x932e10. The file has no checksum.

The tool keeps the header bytes and every member that it does not change, so only the changed members
and the offsets differ from the input file.
"""
import struct

HEADER_SIZE = 0x416EC
ENTRY_SIZE = 0x10C
MAX_ENTRIES = 1000


class Member:
    def __init__(self, index, offset, size, name):
        self.index, self.offset, self.size, self.name = index, offset, size, name
        self.raw = b""        # bytes as stored in the .sav
        self.data = None      # decoded bytes, set by Sav.read_member
        self.changed = False


def decode(raw, size):
    out = bytearray()
    i, n = 0, len(raw)
    while len(out) < size and i < n:
        b = raw[i]
        i += 1
        if b < 0x81:
            out.append(b)
        else:
            out += bytes([raw[i]]) * (b - 0x80)
            i += 1
    if len(out) < size:
        raise ValueError(f"member ends early: {len(out)} of {size} bytes")
    return bytes(out[:size]), i


def encode(data):
    """Same output as the encoder at 0x932e10: runs of up to 0x7f, a single byte below 0x81 stays literal."""
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        b = data[i]
        j = i + 1
        while j < n and data[j] == b and j - i < 0x7F:
            j += 1
        count = j - i
        if count < 2 and b < 0x81:
            out.append(b)
        else:
            out += bytes([count + 0x80, b])
        i = j
    return bytes(out)


class Sav:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.blob = f.read()
        self.version, self.unknown, count = struct.unpack_from("<3i", self.blob, 0)
        if self.version not in (1, 2, 3, 4) or not 0 <= count <= MAX_ENTRIES:
            raise ValueError(f"not a saved game: version {self.version}, {count} files")
        self.compressed = self.version in (2, 4)
        self.members = []
        for k in range(count):
            o = 12 + k * ENTRY_SIZE
            off, size = struct.unpack_from("<2I", self.blob, o)   # unsigned: a save may pass 2 GB
            name = self.blob[o + 8:o + ENTRY_SIZE].split(b"\0")[0].decode("latin-1")
            self.members.append(Member(k, off, size, name))
        order = sorted(self.members, key=lambda m: m.offset)
        for a, b in zip(order, order[1:] + [None]):
            end = b.offset if b else len(self.blob)
            a.raw = self.blob[a.offset:end]

    def member(self, name):
        for m in self.members:
            if m.name.lower() == name.lower():
                return m
        raise KeyError(name)

    def read_member(self, name):
        m = self.member(name)
        if m.data is None:
            if self.compressed:
                m.data, used = decode(m.raw, m.size)
                m.raw = m.raw[:used]
            else:
                m.data = m.raw[:m.size]
        return m.data

    def write_member(self, name, data):
        m = self.member(name)
        self.read_member(name)
        if len(data) != m.size:
            raise ValueError("the tool keeps member sizes unchanged")
        m.data, m.changed = bytes(data), True

    def save(self, path):
        header = bytearray(self.blob[:HEADER_SIZE])
        body = bytearray()
        for m in sorted(self.members, key=lambda m: m.offset):
            if m.changed:
                raw = encode(m.data) if self.compressed else m.data
            else:
                raw = m.raw
            offset = HEADER_SIZE + len(body)
            struct.pack_into("<I", header, 12 + m.index * ENTRY_SIZE, offset)
            body += raw
        with open(path, "xb") as f:   # never overwrite a file
            f.write(header + body)
