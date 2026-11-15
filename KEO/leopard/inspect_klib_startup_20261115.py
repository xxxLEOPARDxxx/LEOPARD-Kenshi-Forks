"""Bounded offline disassembly of the already installed SDK, no execution."""
from pathlib import Path
import sys, struct, hashlib, importlib.util
sys.path.insert(0,r'D:/DEV/Kenshi/tools/_native_python')
import capstone
ROOT=Path(__file__).resolve().parents[1];GAME=ROOT.parents[1]
spec=importlib.util.spec_from_file_location('pe_reader',ROOT/'tools/kenshilib/check_imports.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
pe=mod.PE(GAME/'KenshiLib.dll')
assert hashlib.sha256(pe.data).hexdigest()=='5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b'
iat={};rva,size=pe.directories[1]
for n in range(size//20):
 fields=struct.unpack_from('<5I',pe.data,pe.offset(rva)+20*n)
 if not any(fields):break
 oft,_,_,name,first=fields;dll=pe.string(name)
 for j in range(32768):
  thunk=struct.unpack_from('<Q',pe.data,pe.offset(oft or first)+8*j)[0]
  if not thunk:break
  iat[first+8*j]=dll+'!'+('#'+str(thunk&0xffff) if thunk>>63 else pe.string(thunk+2))
bounds=[];rva,size=pe.directories[3]
for n in range(size//12):bounds.append(struct.unpack_from('<III',pe.data,pe.offset(rva)+12*n)[:2])
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
lines=[]
for arg in sys.argv[1:]:
 target=int(arg,16);start,end=next((s,e) for s,e in bounds if s<=target<e)
 assert end-start<8192
 lines.append('FUNCTION %X..%X'%(start,end))
 for ins in md.disasm(pe.data[pe.offset(start):pe.offset(start)+end-start],start):
  labels=[]
  for op in ins.operands:
   if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:
    ref=ins.address+ins.size+op.mem.disp
    if ref in iat:labels.append(iat[ref])
  lines.append('%08X: %-8s %s%s'%(ins.address,ins.mnemonic,ins.op_str,' ; '+' | '.join(labels) if labels else ''))
out=ROOT/'build/section_diagnostics_lazy_test/klib-startup-disassembly.txt';out.parent.mkdir(exist_ok=True,parents=True);out.write_text('\n'.join(lines)+'\n');print(str(out)+' ('+str(len(lines))+' lines)')
