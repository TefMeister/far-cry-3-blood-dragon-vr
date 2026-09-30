"""Find who references Blood Dragon's camera-offset function names in FC3.dll, and what sits next to them."""
import re, struct, sys
import pefile, capstone

PATH = r'E:/SteamLibrary/steamapps/common/Far Cry 3 Blood Dragon/bin/FC3.dll'
NAMES = [b'EnableCameraOffset', b'DisableCameraOffset', b'IsCameraOffsetEnabled', b'SetDesiredCameraOffset',
         b'SetEffectiveCameraOffset', b'SetEffectiveCameraPositionOffset', b'GetDesiredCamera']

pe = pefile.PE(PATH)
data = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
text = next(s for s in pe.sections if s.Name.rstrip(b'\0') == b'.text')
t0, t1 = text.VirtualAddress, text.VirtualAddress + text.Misc_VirtualSize
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

def find_all(needle):
    return [m.start() for m in re.finditer(re.escape(needle), data)]

def refs_to(va):
    """Every 4-byte little-endian occurrence of va in the image (code or data)."""
    return find_all(struct.pack('<I', va))

def disasm_around(rva, before=40, after=40):
    start = max(t0, rva - before)
    code = data[start:rva + after]
    out = []
    for ins in md.disasm(code, base + start):
        out.append(f'   {ins.address:08x}  {ins.mnemonic:6} {ins.op_str}')
    return out

for name in NAMES:
    hits = [o for o in find_all(name + b'\0') if o == 0 or data[o - 1] == 0]
    print(f'== {name.decode()}: string at', [hex(base + h) for h in hits])
    for h in hits:
        for r in refs_to(base + h):
            where = 'code' if t0 <= r < t1 else 'data'
            print(f'  ref from {where} {base + r:08x}')
            if where == 'code':
                print('\n'.join(disasm_around(r - 1, 30, 40)))
            else:
                words = struct.unpack('<8I', data[r - 16:r + 16])
                print('   table words around it:', ' '.join(f'{w:08x}' for w in words))
