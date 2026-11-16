#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "fixes/world/equipment_animation_lookup.h"
using namespace equipment_animation_lookup;
struct Node{Node*left,*parent,*right;unsigned key,padding;void*state;unsigned char color,nil;char tail[6];};
struct Table{void*prefix;Node*head;uint64_t size;};
struct Pair{unsigned key,padding;void*state;};
static_assert(sizeof(Node)==48&&offsetof(Node,key)==0x18&&offsetof(Node,state)==0x20&&offsetof(Node,nil)==0x29,"native integer tree");
static Node storage,fresh;static unsigned originals=0,allocations=0,insertions=0;static bool validPair=true,validHint=true,reenter=false,throwAllocate=false,throwInsert=false;static void*fallback=NULL;
static void**__fastcall OriginalCall(void*,const unsigned*){++originals;return &fallback;}
static void*__fastcall AllocateCall(void*map,const void*value){++allocations;const Pair*p=(const Pair*)value;validPair=validPair&&p->state==NULL;if(throwAllocate)throw std::runtime_error("allocate");storage.key=p->key;if(reenter){Table*t=(Table*)map;memset(&fresh,0,sizeof(fresh));fresh.left=fresh.parent=fresh.right=&fresh;fresh.nil=1;t->head=&fresh;t->size=0;}return &storage;}
static void*__fastcall InsertCall(void*map,void**out,void*hint,void*node){++insertions;validHint=validHint&&hint==((Table*)map)->head;if(throwInsert)throw std::runtime_error("native insertion cleanup");*out=node;return out;}
static void**Call(Table*t,const unsigned*k,bool enabled=true,bool owner=true){
#ifdef KEO_ANIMATION_BOUNDARY_BASELINE
 return OriginalCall(t,k);
#else
 return Append(t,k,OriginalCall,AllocateCall,InsertCall,enabled,owner);
#endif
}
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){Node head={0},last={0};head.left=head.parent=head.right=&head;head.nil=1;Table table={NULL,&head,0};unsigned key=10;originals=allocations=insertions=0;C(Call(&table,&key)==&storage.state);C(originals==0&&allocations==1&&insertions==1&&validPair&&validHint);
 table.size=1;last.key=9;head.right=&last;C(Call(&table,&key)==&storage.state);last.key=10;originals=0;C(Call(&table,&key)==&fallback&&originals==1);last.key=11;originals=0;C(Call(&table,&key)==&fallback&&originals==1);
 last.key=0xfffffffe;key=0xffffffff;C(Call(&table,&key)==&storage.state);last.key=0xffffffff;key=0;originals=0;C(Call(&table,&key)==&fallback&&originals==1);
 table.size=0;head.right=&head;originals=0;C(Call(&table,&key,false,true)==&fallback&&originals==1);originals=0;C(Call(&table,&key,true,false)==&fallback&&originals==1);
 // Disabled/nonowner must not inspect native map storage.
 originals=0;C(Call((Table*)1,&key,false,true)==&fallback&&originals==1);originals=0;C(Call((Table*)1,&key,true,false)==&fallback&&originals==1);
 originals=0;C(Append(NULL,&key,OriginalCall,AllocateCall,InsertCall,true,true)==&fallback&&originals==1);originals=0;C(Append(&table,NULL,OriginalCall,AllocateCall,InsertCall,true,true)==&fallback&&originals==1);
 originals=0;C(Append(&table,&key,OriginalCall,NULL,InsertCall,true,true)==&fallback&&originals==1);originals=0;C(Append(&table,&key,OriginalCall,AllocateCall,NULL,true,true)==&fallback&&originals==1);
 table.size=0;head.right=&head;key=100;reenter=true;validHint=true;C(Call(&table,&key)==&storage.state&&validHint&&table.head==&fresh);reenter=false;
 table.head=&head;head.right=&head;throwAllocate=true;bool caught=false;const unsigned before=insertions;try{Call(&table,&key);}catch(const std::runtime_error&){caught=true;}throwAllocate=false;C(caught&&insertions==before);
 throwInsert=true;caught=false;originals=0;try{Call(&table,&key);}catch(const std::runtime_error&){caught=true;}throwInsert=false;C(caught&&originals==0);
 table.head=NULL;originals=0;C(Call(&table,&key)==&fallback&&originals==1);table.head=&head;head.nil=0;originals=0;C(Call(&table,&key)==&fallback&&originals==1);head.nil=1;table.size=0;head.right=&last;originals=0;C(Call(&table,&key)==&fallback&&originals==1);
 table.size=1;head.right=NULL;originals=0;C(Call(&table,&key)==&fallback&&originals==1);head.right=&head;originals=0;C(Call(&table,&key)==&fallback&&originals==1);head.right=&last;last.nil=1;originals=0;C(Call(&table,&key)==&fallback&&originals==1);last.nil=0;
 printf("PASS %u current-end production helper boundary checks\n",checks);return 0;}
