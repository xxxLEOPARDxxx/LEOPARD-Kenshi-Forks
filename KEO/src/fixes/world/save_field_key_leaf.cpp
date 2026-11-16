#include "fixes/world/save_field_key_leaf.h"
#include <stdint.h>
#include <cstring>
namespace save_field_key_leaf {
struct Code{
 unsigned char bytes[256];unsigned n,count;bool ok;
 struct Fix{unsigned offset;bool fast;}fix[24];
 Code():n(0),count(0),ok(true){}
 void B(unsigned char v){if(n<sizeof(bytes))bytes[n]=v;else ok=false;++n;}
 void D(uint32_t v){for(unsigned i=0;i<4;++i)B((unsigned char)(v>>(i*8)));}
 void Q(uintptr_t v){for(unsigned i=0;i<8;++i)B((unsigned char)(v>>(i*8)));}
 void Op(const unsigned char*p,unsigned size){for(unsigned i=0;i<size;++i)B(p[i]);}
 void J(unsigned char condition,bool fast=false){B(0x0f);B(condition);Fixup(fast);}
 void Fixup(bool fast){if(count<24){fix[count].offset=n;fix[count++].fast=fast;}else ok=false;D(0);}
};
#define OP(...) do{const unsigned char instruction[]={__VA_ARGS__};c.Op(instruction,sizeof(instruction));}while(0)
unsigned Build(unsigned char*out,size_t capacity,const volatile bool*enabled,void*original,bool provenFrame){
 static_assert(sizeof(void*)==8&&sizeof(bool)==1,"Win64 live byte flag ABI");
 if(!out||!enabled||!original)return 0;Code c;
 OP(0x48,0xb8);c.Q((uintptr_t)enabled);OP(0x80,0x38,0);c.J(0x84);
 if(!provenFrame){
 OP(0x48,0x85,0xc9);c.J(0x84); // nonnull destination
 OP(0x48,0x85,0xd2);c.J(0x84); // nonnull source
 OP(0x48,0x85,0xf6);c.J(0x84); // nonnull native RSI filter
 }
 OP(0x49,0x83,0xf8,63);c.J(0x87);
 OP(0x48,0x83,0x7e,0x20,0);c.J(0x85); // empty native filter
 if(!provenFrame){
 OP(0x48,0x83,0x79,0x10,0);c.J(0x85);
 OP(0x48,0x83,0x79,0x18,15);c.J(0x85);
 OP(0x80,0x39,0);c.J(0x85); // fresh SSO destination
 OP(0x49,0x89,0xc9,0x49,0x83,0xc1,40);c.J(0x82);
 OP(0x48,0x89,0xd0,0x4c,0x01,0xc0);c.J(0x82);
 OP(0x48,0x83,0xc0,1);c.J(0x82);
 OP(0x4c,0x39,0xca);c.J(0x83,true); // source >= destination end
 OP(0x48,0x39,0xc8);c.J(0x86,true); // source end <= destination
 c.B(0xe9);c.Fixup(false);
 }
 const unsigned fast=c.n;OP(0x48,0x89,0xc8,0xc3);
 const unsigned fallback=c.n;OP(0xff,0x25,0,0,0,0);c.Q((uintptr_t)original);
 if(!c.ok||c.n>capacity)return 0;
 for(unsigned i=0;i<c.count;++i){const int rel=(int)(c.fix[i].fast?fast:fallback)-(int)(c.fix[i].offset+4);memcpy(c.bytes+c.fix[i].offset,&rel,4);}
 memcpy(out,c.bytes,c.n);return c.n;
}
#undef OP
}
