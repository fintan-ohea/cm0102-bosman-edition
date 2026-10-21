"""Saves above 2 GB (0.6.0): file positions of the save container as unsigned 32-bit values.

The save container (src/recomp_rt/win/season.c, tools/save/savfile.py) keeps the offset of each part in an
unsigned 32-bit word. The game's C library and the container take positions as signed 32-bit values, so in the
original a save that passes 2 GB is broken when it is written: every part that starts at 2 GB or later gets the
offset 0xffffffff, and the load stops at an error box. The heap of 0.6.0 is 2 GB, so a long game can reach that.

- ftell 0x946aae: the open of a new part (zopen 0x9326e0) takes its offset from ftell at the end of the file
  (0x932ab0 fseek, 0x932ab9 ftell, 0x932ad0 the row), and the tell of a stored part (ztell 0x933170, 0x93319f)
  uses ftell too. ftell gives -1 for any position that _lseek 0x94c552 returns with the top bit set (0x946ad6
  cmp eax, ebx / 0x946adb jl 0x946b38). The patch fails only on the error value of _lseek, -1.
- zseek 0x933080 tests the new position against the start and the end of the part with signed compares
  (0x933104 cmp eax, ebx / jge, 0x933108 cmp eax, edx / jl). The patch makes them unsigned, so a part that
  starts below 2 GB and ends above it can seek.

The runtime does the rest (src/recomp_rt/win/files.c): SetFilePointer takes the distance from the start of the
file as unsigned (the reads of zopen and zseek, fseek 0x946a22), and no file grows past 0xfffffffe bytes. Below
2 GB every position has the top bit clear, so the signed and the unsigned tests give the same result and a save
below 2 GB is the same as before.
"""

# address: (the instruction in the original, C code)
INSNS = {
    0x946ADB: ("jl 0x946b38", "if (eax == 0xffffffffu) goto L_946b38;"),
    0x933106: ("jge 0x933168", "if (!cc_b(F)) goto L_933168;"),
    0x93310A: ("jl 0x933168", "if (cc_b(F)) goto L_933168;"),
}
