from pathlib import Path
import ast,re,ctypes as C
src=Path(__file__).resolve().parents[1]/'HoverOutlineShaders.h'
s=src.read_text(encoding='utf-8')
def shader(name):
 # Stop at the terminating C++ statement, never at a semicolon inside a string.
 start=s.index('static const char '+name+'[]'); end=s.index('\n}',start) if name=='fragment' else s.index('static const char fragment',start)
 return ''.join(ast.literal_eval(x) for x in re.findall(r'"(?:[^"\\]|\\.)*"',s[start:end]))
dll=C.WinDLL('d3dcompiler_47.dll')
dll.D3DCompile.argtypes=[C.c_void_p,C.c_size_t,C.c_char_p,C.c_void_p,C.c_void_p,C.c_char_p,C.c_char_p,C.c_uint,C.c_uint,C.POINTER(C.c_void_p),C.POINTER(C.c_void_p)]
dll.D3DCompile.restype=C.c_long
class Macro(C.Structure):_fields_=[('name',C.c_char_p),('value',C.c_char_p)]
def blob(p):
 table=C.cast(p,C.POINTER(C.POINTER(C.c_void_p))).contents
 get=C.WINFUNCTYPE(C.c_void_p,C.c_void_p)(table[3]);size=C.WINFUNCTYPE(C.c_size_t,C.c_void_p)(table[4])
 return C.string_at(get(p),size(p))
def release(p):
 if p:
  table=C.cast(p,C.POINTER(C.POINTER(C.c_void_p))).contents;C.WINFUNCTYPE(C.c_ulong,C.c_void_p)(table[2])(p)
for variant in range(9):
 name='fragment' if variant==8 else 'vertex'; code=shader(name).encode();macros=(Macro*4)();n=0
 if variant<8:
  for bit,define in [(1,b'MASKED'),(2,b'COLLAPSE_SIDE'),(4,b'SKINNED')]:
   if variant&bit:macros[n]=Macro(define,b'1');n+=1
 out=C.c_void_p();err=C.c_void_p();hr=dll.D3DCompile(code,len(code),b'CharacterHighlightContour',macros,None,b'main_fs' if name=='fragment' else b'main_vs',b'ps_4_0' if name=='fragment' else b'vs_4_0',8|2048,0,C.byref(out),C.byref(err))
 try:
  detail=blob(err).decode(errors='replace') if err else ''
  assert hr>=0,(variant,detail)
  print('PASS shader',name,variant,'bytes',len(blob(out)))
 finally:release(out);release(err)
