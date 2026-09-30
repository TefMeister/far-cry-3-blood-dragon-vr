"""Pair each Lua-registered name near the camera-offset block with its C handler, then disassemble handlers."""
import struct, sys
import pefile, capstone

PATH = r'E:/SteamLibrary/steamapps/common/Far Cry 3 Blood Dragon/bin/FC3.dll'
pe = pefile.PE(PATH)
data = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = False

def cstr(va):
    o = va - base
    if not (0 <= o < len(data)):
        return None
    e = data.find(b'\0', o, o + 80)
    s = data[o:e]
    return s.decode('ascii') if e > o and all(32 <= c < 127 for c in s) else None

REG_NAME, REG_FUNC = 0x1007cd9b, 0x1007cade
start, end = 0x10bb8000, 0x10bb8800
pairs, pending = [], None
for ins in md.disasm(data[start - base:end - base], start):
    if ins.mnemonic == 'push' and ins.op_str.startswith('0x'):
        v = int(ins.op_str, 16)
        s = cstr(v)
        if s:
            pending = s
        elif 0x10001000 <= v < 0x11000000 and pending:
            pairs.append((pending, v))
            pending = None
for n, f in pairs:
    print(f'{n:40} -> {f:08x}')

want = sys.argv[1:] or ['EnableCameraOffset', 'SetEffectiveCameraPositionOffset', 'SetDesiredCameraOffset']
for n, f in pairs:
    if n in want:
        print(f'\n== {n} @ {f:08x}')
        k = 0
        for ins in md.disasm(data[f - base:f - base + 400], f):
            print(f'   {ins.address:08x}  {ins.mnemonic:6} {ins.op_str}')
            k += 1
            if ins.mnemonic == 'ret' or k > 70:
                break
