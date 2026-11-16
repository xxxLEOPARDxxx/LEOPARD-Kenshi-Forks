#ifndef KEO_MORPH_UPLOAD_BATCH_H
#define KEO_MORPH_UPLOAD_BATCH_H
#include <new>
#include <cstddef>
#include <stdexcept>
#include <ogre/OgreMesh.h>
#include <ogre/OgreSubMesh.h>
#include <ogre/OgreVertexIndexData.h>
#include <ogre/OgreHardwareVertexBuffer.h>
namespace morph_upload_batch {
struct Access:Ogre::HardwareBuffer {
 using Ogre::HardwareBuffer::mUseShadowBuffer;
 using Ogre::HardwareBuffer::mShadowBuffer;
 using Ogre::HardwareBuffer::mSuppressHardwareUpdate;
 static bool Suppressed(Ogre::HardwareBuffer*p){bool Ogre::HardwareBuffer::*m=static_cast<bool Ogre::HardwareBuffer::*>(&Access::mSuppressHardwareUpdate);return p->*m;}
 static void Clear(Ogre::HardwareBuffer*p){bool Ogre::HardwareBuffer::*m=static_cast<bool Ogre::HardwareBuffer::*>(&Access::mSuppressHardwareUpdate);p->*m=false;}
 static Ogre::HardwareBuffer*Shadow(Ogre::HardwareBuffer*p){Ogre::HardwareBuffer* Ogre::HardwareBuffer::*m=static_cast<Ogre::HardwareBuffer* Ogre::HardwareBuffer::*>(&Access::mShadowBuffer);return p->*m;}
};
static_assert(offsetof(Access,mUseShadowBuffer)==0x2d,"native buffer shadow flag");
static_assert(offsetof(Access,mShadowBuffer)==0x30,"native shadow pointer");
static_assert(offsetof(Access,mSuppressHardwareUpdate)==0x39,"native suppression flag");
static_assert(sizeof(Ogre::HardwareVertexBufferSharedPtr)==16,"native buffer lease");
// Explicit lifetime: enclosing native scope uses SEH finally, not /EHa.
struct Batch {
 __declspec(align(8)) unsigned char storage[sizeof(Ogre::HardwareVertexBufferSharedPtr)];
 bool live;
 Ogre::HardwareVertexBufferSharedPtr&Buffer(){return *reinterpret_cast<Ogre::HardwareVertexBufferSharedPtr*>(storage);}
};
static void Init(Batch*b){b->live=false;}
static void Begin(Batch*b,Ogre::Mesh*source,Ogre::Mesh*target){
 if(b->live||!source||!target||source==target||!source->getNumSubMeshes()||!target->getNumSubMeshes())return;
 Ogre::SubMesh*ss=source->getSubMesh(0),*ts=target->getSubMesh(0);
 if(!ss||!ts)return;
 Ogre::VertexData*s=ss->vertexData,*t=ts->vertexData;
 if(!s||!t||s==t||!s->vertexDeclaration||!t->vertexDeclaration||!s->vertexBufferBinding||!t->vertexBufferBinding)return;
 const Ogre::VertexElement*se=s->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
 const Ogre::VertexElement*te=t->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
 if(!se||!te||!s->vertexBufferBinding->isBufferBound(se->getSource())||!t->vertexBufferBinding->isBufferBound(te->getSource()))return;
 const Ogre::HardwareVertexBufferSharedPtr&sb=s->vertexBufferBinding->getBuffer(se->getSource());
 const Ogre::HardwareVertexBufferSharedPtr&tb=t->vertexBufferBinding->getBuffer(te->getSource());
 if(sb.isNull()||tb.isNull()||sb.get()==tb.get()||!tb->hasShadowBuffer()||tb->isSystemMemory()||tb->isLocked()||Access::Suppressed(tb.get()))return;
 Ogre::HardwareBuffer*shadow=Access::Shadow(tb.get());
 if(!shadow||shadow==sb.get()||shadow==Access::Shadow(sb.get()))return;
 new(b->storage) Ogre::HardwareVertexBufferSharedPtr(tb);b->live=true;
 // suppress(true) only sets a flag; it does not allocate, lock or upload.
 tb->suppressHardwareUpdate(true);
}
static void Flush(Ogre::HardwareVertexBuffer*p){
 if(p->isLocked()){Access::Clear(p);throw std::runtime_error("Morph body upload: native buffer remains locked");}
 p->suppressHardwareUpdate(false);
}
static void End(Batch*b){
 if(!b->live)return;
 b->live=false;Ogre::HardwareVertexBuffer*p=b->Buffer().get();
 __try{
  // Do not force-unlock a buffer abandoned by a failed native blend.
  Flush(p);
 }__finally{b->Buffer().~HardwareVertexBufferSharedPtr();}
}
static bool EndSeh(Batch*b){__try{End(b);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool EndNoThrow(Batch*b){try{return EndSeh(b);}catch(...){return false;}}
}
#endif
