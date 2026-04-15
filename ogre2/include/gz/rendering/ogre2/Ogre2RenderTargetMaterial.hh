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
#ifndef GZ_RENDERING_OGRE2_OGRE2RENDERTARGETMATERIAL_HH_
#define GZ_RENDERING_OGRE2_OGRE2RENDERTARGETMATERIAL_HH_

#include <vector>

#include "gz/rendering/config.hh"
#include "gz/rendering/ogre2/Ogre2Includes.hh"
#include "gz/rendering/ogre2/Export.hh"

namespace ignition
{
  namespace rendering
  {
    inline namespace IGNITION_RENDERING_VERSION_NAMESPACE {
    //
    /// \brief Causes all objects in a scene to be rendered with the same
    /// material when rendered by a given CompositorWorkspace.
    ///
    /// On construction it registers as a CompositorWorkspaceListener on the
    /// provided workspace and sets the material scheme on every scene pass node
    /// to a value that is unlikely to exist. When the workspace is about to
    /// render it adds itself as an Ogre::MaterialManager::Listener so that
    /// handleSchemeNotFound redirects every material look-up to the supplied
    /// override material.
    /// \internal
    class IGNITION_RENDERING_OGRE2_VISIBLE  Ogre2RenderTargetMaterial :
      public Ogre::CompositorWorkspaceListener,
      public Ogre::MaterialManager::Listener
    {
      /// \brief constructor
      /// \param[in] _scene the scene manager responsible for rendering
      /// \param[in] _workspace the CompositorWorkspace this should apply to
      /// \param[in] _material the material to apply to all renderables
      public: Ogre2RenderTargetMaterial(Ogre::SceneManager *_scene,
          Ogre::CompositorWorkspace *_workspace, Ogre::Material *_material);

      /// \brief destructor
      public: ~Ogre2RenderTargetMaterial();

      // Documentation inherited – adds MaterialManager listener before render.
      public: virtual void workspacePreUpdate(
          Ogre::CompositorWorkspace *_workspace) override;

      // Documentation inherited – removes MaterialManager listener after render.
      public: virtual void workspacePosUpdate(
          Ogre::CompositorWorkspace *_workspace) override;

      /// \brief Ogre callback that assigns the override material to all
      /// renderables when the requested scheme is not found.
      public: virtual Ogre::Technique *handleSchemeNotFound(
                  uint16_t _schemeIndex, const Ogre::String &_schemeName,
                  Ogre::Material *_originalMaterial, uint16_t _lodIndex,
                  const Ogre::Renderable *_rend) override;

      /// \brief scene manager responsible for rendering
      private: Ogre::SceneManager *scene = nullptr;

      /// \brief compositor workspace that should see a uniform material
      private: Ogre::CompositorWorkspace *workspace = nullptr;

      /// \brief material that should be applied to all objects
      private: Ogre::Material *material = nullptr;

      /// \brief name of the material scheme used by this applicator
      private: Ogre::String schemeName;
    };
    }
  }
}

#endif
