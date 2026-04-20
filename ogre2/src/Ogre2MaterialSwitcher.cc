/*
 * Copyright (C) 2018 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "gz/common/Console.hh"
#include "gz/rendering/ogre2/Ogre2Includes.hh"
#include "gz/rendering/ogre2/Ogre2MaterialSwitcher.hh"
#include "gz/rendering/ogre2/Ogre2Scene.hh"
#include "gz/rendering/RenderTypes.hh"

// HLMS Unlit headers for color-coded selection rendering
#include <Hlms/Unlit/OgreHlmsUnlit.h>
#include <Hlms/Unlit/OgreHlmsUnlitDatablock.h>
#include <OgreHlmsManager.h>

using namespace gz;
using namespace rendering;


/////////////////////////////////////////////////
Ogre2MaterialSwitcher::Ogre2MaterialSwitcher(Ogre2ScenePtr _scene)
{
  this->currentColor = math::Color(0.0f, 0.0f, 0.1f);
  this->scene = _scene;
}

/////////////////////////////////////////////////
Ogre2MaterialSwitcher::~Ogre2MaterialSwitcher()
{
  // Destroy all cached selection datablocks
  Ogre::Root *root = Ogre::Root::getSingletonPtr();
  if (root)
  {
    Ogre::HlmsManager *hlmsManager = root->getHlmsManager();
    Ogre::HlmsUnlit *hlmsUnlit = static_cast<Ogre::HlmsUnlit*>(
        hlmsManager->getHlms(Ogre::HLMS_UNLIT));
    if (hlmsUnlit)
    {
      for (auto &pair : this->selectionDatablocks)
        hlmsUnlit->destroyDatablock(pair.second->getName());
    }
  }
}

////////////////////////////////////////////////
Ogre::HlmsUnlitDatablock *Ogre2MaterialSwitcher::GetOrCreateDatablock(
    const math::Color &_color, bool _overlay)
{
  // Build a unique datablock name from the color + overlay flag
  const uint32_t key = _color.AsRGBA();
  auto it = this->selectionDatablocks.find(
      _overlay ? (key | 0x80000000u) : key);
  if (it != this->selectionDatablocks.end())
    return it->second;

  Ogre::Root *root = Ogre::Root::getSingletonPtr();
  Ogre::HlmsManager *hlmsManager = root->getHlmsManager();
  Ogre::HlmsUnlit *hlmsUnlit = static_cast<Ogre::HlmsUnlit*>(
      hlmsManager->getHlms(Ogre::HLMS_UNLIT));

  // Build a unique name
  std::string dbName = "__sel_" + std::to_string(key)
      + (_overlay ? "_ov" : "");

  Ogre::HlmsMacroblock macroblock;
  macroblock.mDepthCheck = !_overlay;
  macroblock.mDepthWrite = !_overlay;
  Ogre::HlmsBlendblock blendblock;

  Ogre::HlmsUnlitDatablock *db = static_cast<Ogre::HlmsUnlitDatablock*>(
      hlmsUnlit->createDatablock(
          dbName, dbName, macroblock, blendblock, Ogre::HlmsParamVec()));
  db->setUseColour(true);
  db->setColour(Ogre::ColourValue(
      _color.R(), _color.G(), _color.B(), 1.0f));

  const uint32_t mapKey = _overlay ? (key | 0x80000000u) : key;
  this->selectionDatablocks[mapKey] = db;
  return db;
}

////////////////////////////////////////////////
void Ogre2MaterialSwitcher::workspacePreUpdate(
    Ogre::CompositorWorkspace *_workspace)
{
  // Switch each scene item to a solid-color HLMS Unlit datablock so the
  // 1×1 selection buffer can identify which entity is under the cursor.
  this->datablockMap.clear();
  auto itor = this->scene->OgreSceneManager()->getMovableObjectIterator(
      Ogre::ItemFactory::FACTORY_TYPE_NAME);

  while (itor.hasMoreElements())
  {
    this->NextColor();

    Ogre::MovableObject *object = itor.peekNext();
    Ogre::Item *item = static_cast<Ogre::Item *>(object);

    this->colorDict[this->currentColor.AsRGBA()] = item->getName();

    for (unsigned int i = 0; i < item->getNumSubItems(); ++i)
    {
      Ogre::SubItem *subItem = item->getSubItem(i);
      Ogre::HlmsDatablock *origDatablock = subItem->getDatablock();
      this->datablockMap[subItem] = origDatablock;

      bool isOverlay = false;
      if (origDatablock && origDatablock->getMacroblock())
        {
          isOverlay = (!origDatablock->getMacroblock()->mDepthWrite &&
                       !origDatablock->getMacroblock()->mDepthCheck);
        }

      Ogre::HlmsUnlitDatablock *selDb =
          GetOrCreateDatablock(this->currentColor, isOverlay);
      subItem->setDatablock(selDb);
    }
    itor.moveNext();
  }
}

/////////////////////////////////////////////////
void Ogre2MaterialSwitcher::workspacePosUpdate(
    Ogre::CompositorWorkspace * /*_workspace*/)
{
  // Restore each item's original datablock after the selection render.
  auto itor = this->scene->OgreSceneManager()->getMovableObjectIterator(
      Ogre::ItemFactory::FACTORY_TYPE_NAME);
  while (itor.hasMoreElements())
  {
    Ogre::MovableObject *object = itor.peekNext();
    Ogre::Item *item = static_cast<Ogre::Item *>(object);
    for (unsigned int i = 0; i < item->getNumSubItems(); ++i)
    {
      Ogre::SubItem *subItem = item->getSubItem(i);
      auto it = this->datablockMap.find(subItem);
      if (it != this->datablockMap.end())
        subItem->setDatablock(it->second);
    }
    itor.moveNext();
  }
}

/////////////////////////////////////////////////
std::string Ogre2MaterialSwitcher::EntityName(
    const math::Color &_color) const
{
  auto iter = this->colorDict.find(_color.AsRGBA());

  if (iter != this->colorDict.end())
    return (*iter).second;
  else
    return std::string();
}

/////////////////////////////////////////////////
void Ogre2MaterialSwitcher::NextColor()
{
  auto color = this->currentColor.AsARGB();
  color++;
  this->currentColor.SetFromARGB(color);
}

/////////////////////////////////////////////////
void Ogre2MaterialSwitcher::Reset()
{
  this->currentColor = math::Color(
      0.0, 0.0, 0.0);
  this->colorDict.clear();
}
