#include "gog_profile.h"
#include <cstring>
namespace GogProfile {
namespace {
struct ByteWindow {uintptr_t steamRva;unsigned char bytes[32];};
#include "gog_profile_data.inc"
static const char imageDigest[]="2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681";
static bool Find(const AddressPair*rows,size_t count,uintptr_t key,uintptr_t*out){
 if(!out)return false;*out=0;
 size_t lo=0,hi=count;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;if(rows[mid].from<key)lo=mid+1;else hi=mid;}
 if(lo==count||rows[lo].from!=key)return false;
 *out=rows[lo].to;return true;
}
}
const char* ExecutableSha256(){return imageDigest;}
bool MatchesImage(uint32_t timestamp,uint32_t imageSize,const char*sha256){
 return timestamp==0x65D60519u&&imageSize==0x232A000u&&sha256&&std::strcmp(sha256,imageDigest)==0;
}
bool Translate(uintptr_t steamRva,uintptr_t*gogRva){return Find(addresses,sizeof(addresses)/sizeof(addresses[0]),steamRva,gogRva);}
bool Absolute(uintptr_t base,uintptr_t steamRva,uintptr_t*address){
 if(!address)return false;*address=0;uintptr_t actual=0;
 if(!base||!Translate(steamRva,&actual)||actual>(uintptr_t)-1-base)return false;
 *address=base+actual;return true;
}
bool CanonicalExact(uintptr_t gogRva,uintptr_t*steamRva){return Find(reverseAddresses,sizeof(reverseAddresses)/sizeof(reverseAddresses[0]),gogRva,steamRva);}
const unsigned char* ExpectedBytes(uintptr_t steamRva,size_t count){
 if(!count||count>32)return NULL;
 size_t lo=0,hi=sizeof(windows)/sizeof(windows[0]);
 while(lo<hi){size_t mid=lo+(hi-lo)/2;if(windows[mid].steamRva<steamRva)lo=mid+1;else hi=mid;}
 if(lo==sizeof(windows)/sizeof(windows[0])||windows[lo].steamRva!=steamRva)return NULL;
 return windows[lo].bytes;
}
bool VerifyWindow(Reader reader,void*user,uintptr_t steamRva,size_t count){
 const unsigned char*expected=ExpectedBytes(steamRva,count);uintptr_t actual=0;
 if(!reader||!expected||!Translate(steamRva,&actual))return false;
 unsigned char bytes[32];
 return reader(user,actual,bytes,count)&&std::memcmp(bytes,expected,count)==0;
}
size_t AddressCount(){return sizeof(addresses)/sizeof(addresses[0]);}
const AddressPair* AddressAt(size_t index){return index<AddressCount()?addresses+index:NULL;}
size_t WindowCount(){return sizeof(windows)/sizeof(windows[0]);}
uintptr_t WindowKeyAt(size_t index){return index<WindowCount()?windows[index].steamRva:0;}
}
