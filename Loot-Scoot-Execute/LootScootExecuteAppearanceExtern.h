#pragma once

#include <cstddef>
#include <string>

namespace Ogre
{
class Entity;
}

class AttachedEntity;

// Native members used by shader tinting and the finisher weapon probe.
class AppearanceBase
{
public:
    virtual ~AppearanceBase();
    virtual void func0x8();
    virtual void func0x10();
    virtual void func0x18();
    virtual void func0x20();
    virtual void func0x28();
    virtual void func0x30();
    virtual void setFlayed(bool flayed);
    virtual bool isFlayed();
    virtual int getBarefoot();
    virtual void updateVisbleForAttachedEntityies(bool visible);
    virtual void func0x58();
    virtual void func0x60();
    virtual void updateCharaterTexture();

    AttachedEntity* getAttachedEntity(const std::string& slot) const;

    void* slot;
    unsigned char _reserved10[0x40];
    unsigned char _reserved50[0x18];
    size_t _reserved68[7];
    unsigned char _reservedA0[0x18];
    size_t _reservedB8[4];
    Ogre::Entity* entity;
    Ogre::Entity* characterModel;
};

static_assert(offsetof(AppearanceBase, entity) == 0xD8, "AppearanceBase entity offset changed");
static_assert(offsetof(AppearanceBase, characterModel) == 0xE0, "AppearanceBase characterModel offset changed");
