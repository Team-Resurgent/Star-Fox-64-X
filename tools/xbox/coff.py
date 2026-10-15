"""A small reader for the i386 COFF objects clang writes (i686-pc-windows-gnu).

Enough to lay an object's data out at an address and apply its relocations
the way a linker would: sections (name, alignment, raw bytes, relocations),
and the symbol table (name, value, section, storage class).
"""
import struct

IMAGE_SCN_CNT_CODE = 0x00000020
IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040
IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080
IMAGE_SCN_LNK_INFO = 0x00000200
IMAGE_SCN_LNK_REMOVE = 0x00000800
IMAGE_SCN_LNK_COMDAT = 0x00001000
IMAGE_SCN_MEM_DISCARDABLE = 0x02000000
IMAGE_SCN_ALIGN_MASK = 0x00F00000

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3
IMAGE_SYM_CLASS_LABEL = 6
IMAGE_SYM_UNDEFINED = 0
IMAGE_SYM_ABSOLUTE = -1
IMAGE_SYM_DEBUG = -2

IMAGE_REL_I386_ABSOLUTE = 0x0000
IMAGE_REL_I386_DIR32 = 0x0006
IMAGE_REL_I386_DIR32NB = 0x0007
IMAGE_REL_I386_REL32 = 0x0014


class Section(object):
    def __init__(self, index, name, flags, data, relocs):
        self.index = index          # 1-based, as symbols name it
        self.name = name
        self.flags = flags
        self.data = data            # bytes, or None for uninitialised
        self.size = 0
        self.relocs = relocs        # [(offset, symbol index, type)]

    @property
    def align(self):
        a = (self.flags & IMAGE_SCN_ALIGN_MASK) >> 20
        return 1 << (a - 1) if a else 16

    @property
    def is_bss(self):
        return bool(self.flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA)

    @property
    def is_code(self):
        return bool(self.flags & IMAGE_SCN_CNT_CODE)

    @property
    def is_dropped(self):
        return bool(self.flags & (IMAGE_SCN_LNK_REMOVE | IMAGE_SCN_LNK_INFO)) or \
            self.name.startswith(".debug") or self.name in (".drectve", ".llvm_addrsig")


class Symbol(object):
    def __init__(self, index, name, value, section, typ, sclass):
        self.index = index
        self.name = name
        self.value = value
        self.section = section      # 1-based section number, 0 undefined, -1 absolute
        self.type = typ
        self.sclass = sclass

    @property
    def is_global_def(self):
        return self.sclass == IMAGE_SYM_CLASS_EXTERNAL and self.section > 0


class CoffObject(object):
    def __init__(self, path):
        self.path = path
        d = open(path, "rb").read()
        self.d = d
        machine, nsec, _ts, symptr, nsym, opthdr, _ch = struct.unpack_from("<HHIIIHH", d, 0)
        if machine != 0x14C:
            raise ValueError("%s: not an i386 COFF object (machine %04x)" % (path, machine))
        self.strtab = symptr + nsym * 18
        self.sections = []
        off = 20 + opthdr
        for i in range(nsec):
            b = off + i * 40
            name = self._name(d[b:b + 8])
            vsize, vaddr, size, ptr, relptr, _lp, nrel, _nl, flags = \
                struct.unpack_from("<IIIIIIHHI", d, b + 8)
            relocs = []
            if nrel == 0xFFFF and (flags & 0x01000000):     # IMAGE_SCN_LNK_NRELOC_OVFL
                nrel = struct.unpack_from("<I", d, relptr)[0]
                relptr += 10
                nrel -= 1
            for r in range(nrel):
                va, si, typ = struct.unpack_from("<IIH", d, relptr + r * 10)
                relocs.append((va, si, typ))
            sec = Section(i + 1, name, flags, None if (flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA) else d[ptr:ptr + size], relocs)
            sec.size = size
            self.sections.append(sec)
        self.symbols = {}
        self.symlist = []
        i = 0
        while i < nsym:
            b = symptr + i * 18
            name = self._symname(d[b:b + 8])
            value, secnum, typ, sclass, naux = struct.unpack_from("<IhHBB", d, b + 8)
            s = Symbol(i, name, value, secnum, typ, sclass)
            self.symbols[i] = s
            self.symlist.append(s)
            i += 1 + naux

    def _str(self, off):
        end = self.d.index(b"\0", self.strtab + off)
        return self.d[self.strtab + off:end].decode("ascii", "replace")

    def _name(self, raw):
        n = raw.rstrip(b"\0").decode("ascii", "replace")
        if n.startswith("/") and n[1:].isdigit():
            return self._str(int(n[1:]))
        return n

    def _symname(self, raw):
        if raw[:4] == b"\0\0\0\0":
            return self._str(struct.unpack_from("<I", raw, 4)[0])
        return raw.rstrip(b"\0").decode("ascii", "replace")

    def section(self, number):
        return self.sections[number - 1]
