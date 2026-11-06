#pragma once
class Character;
namespace Ogre { class ColourValue; }
namespace HoverOutline {
void BeginFrame();
void Draw(Character* target,const Ogre::ColourValue& colour,float width,float brightness);
void EndFrame();
void Remove(Character* target);
void Clear();
void Shutdown();
void SetDebug(bool enabled);
void SetEdgePass(bool enabled);
void PortraitBegin(Character* target);
void PortraitEnd();
}
