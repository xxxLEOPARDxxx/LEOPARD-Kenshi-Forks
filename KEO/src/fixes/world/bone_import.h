#pragma once
namespace bone_import {
typedef void(*Original)(void*,const void*);
typedef void*(*Allocate)(void*,const void*);
typedef void*(*Link)(void*,void**,bool,void*,void*);
void Add(void*self,const void*assignment,Original original,Allocate allocate,Link link,bool enabled,bool owner);
}
void InstallBoneImport(int*,int*);
