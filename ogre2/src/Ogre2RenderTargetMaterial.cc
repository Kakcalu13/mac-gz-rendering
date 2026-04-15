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

#include "gz/rendering/ogre2/Ogre2RenderTargetMaterial.hh"

using namespace gz::rendering;


//////////////////////////////////////////////////
Ogre2RenderTargetMaterial::Ogre2RenderTargetMaterial(
    Ogre::SceneManager *_scene, Ogre::CompositorWorkspace *_workspace,
    Ogre::Material *_material):
  scene(_scene), workspace(_workspace), material(_material)
{
  // Pick a name that's unlikely to collide with a real material scheme
  this->schemeName = "__ignition__rendering__Ogre2RenderTargetMaterial";

  // Set the material scheme on every scene pass in this workspace so that
  // handleSchemeNotFound is invoked for every renderable.
  if (this->workspace)
  {
    auto nodeSeq = this->workspace->getNodeSequence();
    for (auto *node : nodeSeq)
    {
      for (auto *pass : node->_getPasses())
      {
        auto *passDef = const_cast<Ogre::CompositorPassDef *>(
            pass->getDefinition());
        if (passDef->getType() == Ogre::PASS_SCENE)
        {
          auto *sceneDef =
              static_cast<Ogre::CompositorPassSceneDef *>(passDef);
          sceneDef->mMaterialScheme = this->schemeName;
        }
      }
    }
    this->workspace->addListener(this);
  }
}

//////////////////////////////////////////////////
Ogre2RenderTargetMaterial::~Ogre2RenderTargetMaterial()
{
  if (this->workspace)
    this->workspace->removeListener(this);
}

//////////////////////////////////////////////////
void Ogre2RenderTargetMaterial::workspacePreUpdate(
    Ogre::CompositorWorkspace * /*_workspace*/)
{
  Ogre::MaterialManager::getSingleton().addListener(this);
}

//////////////////////////////////////////////////
void Ogre2RenderTargetMaterial::workspacePosUpdate(
    Ogre::CompositorWorkspace * /*_workspace*/)
{
  Ogre::MaterialManager::getSingleton().removeListener(this);
}

//////////////////////////////////////////////////
/// \brief Ogre callback that assigns material to new renderables
Ogre::Technique *Ogre2RenderTargetMaterial::handleSchemeNotFound(
    uint16_t /*_schemeIndex*/, const Ogre::String &_schemeName,
    Ogre::Material * /*_originalMaterial*/, uint16_t /*_lodIndex*/,
    const Ogre::Renderable * /*_rend*/)
{
  if (_schemeName == this->schemeName)
  {
    // not using getBestTechnique() because it leads to infinite recursion here
    return this->material->getSupportedTechnique(0);
  }
  return nullptr;
}
