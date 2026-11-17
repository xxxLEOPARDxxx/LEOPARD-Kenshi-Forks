#pragma once
// Only patches this isolated native-oracle process. Never the game process.
struct FixtureHooks {
 code_patch_batch::Edit edits[2];unsigned char*pages[2];code_patch_threads::Set threads;bool active;
 FixtureHooks():active(false){memset(edits,0,sizeof(edits));memset(pages,0,sizeof(pages));memset(&threads,0,sizeof(threads));}
 int Arm(HMODULE module){
  const uintptr_t targets[]={(uintptr_t)&TrackAssignment,(uintptr_t)&CountRelease};
  const unsigned sites[]={0x1cabff,0x1ba2bc},nativeTargets[]={0x369330,0x1a1340};
  for(unsigned i=0;i<2;++i){unsigned char*site=(unsigned char*)module+sites[i];pages[i]=AllocateNearPages((unsigned __int64)(uintptr_t)site,4096);C(pages[i]!=NULL);pages[i][0]=0xff;pages[i][1]=0x25;memset(pages[i]+2,0,4);memcpy(pages[i]+6,&targets[i],8);DWORD old=0;C(VirtualProtect(pages[i],4096,PAGE_EXECUTE_READ,&old));C(FlushInstructionCache(GetCurrentProcess(),pages[i],14));edits[i].address=site;edits[i].size=5;memcpy(edits[i].before,site,5);C(site[0]==0xe8);int relative;memcpy(&relative,site+1,4);C(site+5+relative==(unsigned char*)module+nativeTargets[i]);memcpy(edits[i].after,site,5);const __int64 delta=(__int64)(uintptr_t)pages[i]-(__int64)(uintptr_t)(site+5);C(delta>=-2147483647LL-1&&delta<=2147483647LL);relative=(int)delta;memcpy(edits[i].after+1,&relative,4);}
  bool possible=false;C(code_patch_batch::Apply(edits,2,&threads,&possible));active=true;return 0;
 }
 int Restore(){
  C(active);for(unsigned i=0;i<2;++i){unsigned char old[5];memcpy(old,edits[i].before,5);memcpy(edits[i].before,edits[i].after,5);memcpy(edits[i].after,old,5);}bool possible=false;C(code_patch_batch::Apply(edits,2,&threads,&possible));active=false;for(unsigned i=0;i<2;++i){C(VirtualFree(pages[i],0,MEM_RELEASE));pages[i]=NULL;}return 0;
 }
};
