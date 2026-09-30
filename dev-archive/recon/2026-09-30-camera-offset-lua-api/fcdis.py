"""Disassemble FC3.dll at given VAs until ret (or a limit)."""
import sys
import pefile, capstone

pe = pefile.PE(r'E:/SteamLibrary/steamapps/common/Far Cry 3 Blood Dragon/bin/FC3.dll')
data = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
limit = 120
for arg in sys.argv[1:]:
    if arg.startswith('n='):
        limit = int(arg[2:])
        continue
    va = int(arg, 16)
    print(f'== {va:08x}')
    k = 0
    for ins in md.disasm(data[va - base:va - base + 4000], va):
        print(f'   {ins.address:08x}  {ins.mnemonic:6} {ins.op_str}')
        k += 1
        if ins.mnemonic.startswith('ret') or k >= limit:
            break
