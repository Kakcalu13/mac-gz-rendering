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

#ifndef GZ_RENDERING_OGRE2_OGRE2MATERIALSWITCHER_HH_
#define GZ_RENDERING_OGRE2_OGRE2MATERIALSWITCHER_HH_

#include <cstdint>
#include <map>
#include <string>

#include <gz/math/Color.hh>
#include "gz/rendering/config.hh"
#include "gz/rendering/ogre2/Export.hh"
#include "gz/rendering/ogre2/Ogre2Includes.hh"
#include "gz/rendering/ogre2/Ogre2RenderTypes.hh"

// Forward-declare HLMS Unlit types to avoid pulling in heavy Ogre headers here
namespace Ogre { class HlmsUnlitDatablock; }

namespace ignition
{
  namespace rendering
  {
    inline namespace IGNITION_RENDERING_VERSION_NAMESPACE {
    //
    // forward declarations
    class Ogre2SelectionBuffer;

    /// \brief Helper class to assign unique colors to renderables
    class IGNITION_RENDERING_OGRE2_VISIBLE Ogre2MaterialSwitcher :
      public Ogre::CompositorWorkspaceListener
    {
      /// \brief Constructor
      public: explicit Ogre2MaterialSwitcher(Ogre2ScenePtr _scene);

      /// \brief Destructor
      public: ~Ogre2MaterialSwitcher();

      /// \brief Get the entity with a specific color
      /// \param[in] _color The entity's color.
      public: std::string EntityName(
              const gz::math::Color &_color) const;

      /// \brief Reset the color value incrementor
      public: void Reset();

      // Documentation inherited – switches all item materials to unique colors
      // before the workspace renders.
      public: virtual void workspacePreUpdate(
                  Ogre::CompositorWorkspace *_workspace) override;

      // Documentation inherited – restores original item materials
      // after the workspace finishes rendering.
      public: virtual void workspacePosUpdate(
                  Ogre::CompositorWorkspace *_workspace) override;

      /// \brief Current unique color value
      private: gz::math::Color currentColor;

      /// \brief Color dictionary that maps the unique color value to
      /// renderable name
      private: std::map<unsigned int, std::string> colorDict;

      /// \brief A map of ogre sub item pointer to their original hlms material
      private: std::map<Ogre::SubItem *, Ogre::HlmsDatablock *> datablockMap;

      /// \brief Get or create a cached HLMS Unlit datablock for a given color.
      /// \param[in] _color  The solid color to encode.
      /// \param[in] _overlay  If true, depth check/write are disabled (for
      ///                      items that live in the overlay render queue).
      private: Ogre::HlmsUnlitDatablock *GetOrCreateDatablock(
                   const gz::math::Color &_color, bool _overlay);

      /// \brief Cache of HLMS Unlit datablocks keyed by color RGBA + overlay
      /// flag (high bit set for overlay). Populated lazily; destroyed in dtor.
      private: std::map<uint32_t, Ogre::HlmsUnlitDatablock *> selectionDatablocks;

      /// \brief Increment unique color value that will be assigned to the
      /// next renderable
      private: void NextColor();

      /// \brief Selection Buffer class that make use of this class for
      /// selecting entitiies
      public: friend class Ogre2SelectionBuffer;

      /// \brief Plain material technique
      private: Ogre2ScenePtr scene;
    };
    }
  }
}
#endif
