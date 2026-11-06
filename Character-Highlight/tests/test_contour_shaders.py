from pathlib import Path
import ast,re,ctypes as C
src=Path(__file__).resolve().parents[1]/'HoverOutlineShaders.h'
s=src.read_text(encoding='utf-8')
ORDER=['vertex','fragment','maskTrivial']
def shader(name):
 # Stop at the next shader block or the closing brace, never inside a string.
 start=s.index('static const char '+name+'[]')
 ends=[s.index('static const char '+n+'[]',start+1) for n in ORDER if n!=name and ('static const char '+n+'[]') in s[start+1:]]
 end=min(ends) if ends else s.index('\n}',start)
 return ''.join(ast.literal_eval(x) for x in re.findall(r'"(?:[^"\\]|\\.)*"',s[start:end]))
dll=C.WinDLL('d3dcompiler_47.dll')
dll.D3DCompile.argtypes=[C.c_void_p,C.c_size_t,C.c_char_p,C.c_void_p,C.c_void_p,C.c_char_p,C.c_char_p,C.c_uint,C.c_uint,C.POINTER(C.c_void_p),C.POINTER(C.c_void_p)]
dll.D3DCompile.restype=C.c_long
def blob(p):
 table=C.cast(p,C.POINTER(C.POINTER(C.c_void_p))).contents
 get=C.WINFUNCTYPE(C.c_void_p,C.c_void_p)(table[3]);size=C.WINFUNCTYPE(C.c_size_t,C.c_void_p)(table[4])
 return C.string_at(get(p),size(p))
def release(p):
 if p:
  table=C.cast(p,C.POINTER(C.POINTER(C.c_void_p))).contents;C.WINFUNCTYPE(C.c_ulong,C.c_void_p)(table[2])(p)
for name in ORDER:
 code=shader(name).encode()
 out=C.c_void_p();err=C.c_void_p();hr=dll.D3DCompile(code,len(code),b'CharacterHighlightEdge',None,None,b'main_vs' if name=='vertex' else b'main_fs',b'vs_4_0' if name=='vertex' else b'ps_4_0',8|2048,0,C.byref(out),C.byref(err))
 try:
  detail=blob(err).decode(errors='replace') if err else ''
  assert hr>=0,(name,detail)
  print('PASS edge shader',name,'bytes',len(blob(out)))
 finally:release(out);release(err)
