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

#include <memory>
#include <gz/math/Color.hh>

#include "gz/common/Console.hh"
#include "gz/rendering/RenderTypes.hh"
#include "gz/rendering/ogre2/Ogre2Includes.hh"
#include "gz/rendering/ogre2/Ogre2MaterialSwitcher.hh"
#include "gz/rendering/ogre2/Ogre2RenderEngine.hh"
#include "gz/rendering/ogre2/Ogre2RenderTarget.hh"
#include "gz/rendering/ogre2/Ogre2Scene.hh"
#include "gz/rendering/ogre2/Ogre2SelectionBuffer.hh"

using namespace gz;
using namespace rendering;

struct gz::rendering::Ogre2SelectionBufferPrivate
{
  /// \brief This is a material listener and a CompositorWorkspaceListener.
  /// The material switcher is applied to only the selection camera
  /// and not applied globally to all targets. The class associates a
  /// color to an ogre entity
  public: std::unique_ptr<Ogre2MaterialSwitcher> materialSwitcher;

  /// \brief Ogre scene manager
  public: Ogre2ScenePtr scene;

  /// \brief Ogre scene manager
  public: Ogre::SceneManager *sceneMgr = nullptr;

  /// \brief Pointer to the camera that will be used as the reference
  /// for selection
  public: Ogre::Camera *camera = nullptr;

  /// \brief Selection buffer's render to texture camera
  public: Ogre::Camera *selectionCamera  = nullptr;

  /// \brief Ogre-next 2.3 GPU texture used as 1x1 render-to-texture target
  public: Ogre::TextureGpu *texture = nullptr;

  /// \brief Ogre's compositor workspace - the main interface to render
  /// into a render target or render texture.
  public: Ogre::CompositorWorkspace *ogreCompositorWorkspace = nullptr;

  /// \brief Render texture data buffer
  public: uint8_t *buffer = nullptr;

  /// \brief Width of the main camera viewport in pixels
  public: unsigned int viewportWidth = 0;

  /// \brief Height of the main camera viewport in pixels
  public: unsigned int viewportHeight = 0;
};

/////////////////////////////////////////////////
Ogre2SelectionBuffer::Ogre2SelectionBuffer(const std::string &_cameraName,
    Ogre2ScenePtr _scene, unsigned int _width, unsigned int _height)
    : dataPtr(new Ogre2SelectionBufferPrivate)
{
  this->dataPtr->scene = _scene;
  this->dataPtr->sceneMgr = _scene->OgreSceneManager();
  this->dataPtr->viewportWidth = _width;
  this->dataPtr->viewportHeight = _height;

  this->dataPtr->camera = this->dataPtr->sceneMgr->findCameraNoThrow(
      _cameraName);
  if (!this->dataPtr->camera)
  {
    ignerr << "No camera found. Unable to create Ogre 2 selection buffer "
           << std::endl;
    return;
  }

  this->dataPtr->selectionCamera =
      this->dataPtr->sceneMgr->createCamera(_cameraName + "_selection_buffer");

  this->dataPtr->materialSwitcher.reset(
      new Ogre2MaterialSwitcher(this->dataPtr->scene));
  this->CreateRTTBuffer();
}

/////////////////////////////////////////////////
Ogre2SelectionBuffer::~Ogre2SelectionBuffer()
{
  this->DeleteRTTBuffer();

  // remove selection buffer camera
  this->dataPtr->sceneMgr->destroyCamera(this->dataPtr->selectionCamera);
}

/////////////////////////////////////////////////
void Ogre2SelectionBuffer::SetDimensions(unsigned int _width,
    unsigned int _height)
{
  this->dataPtr->viewportWidth = _width;
  this->dataPtr->viewportHeight = _height;
}

/////////////////////////////////////////////////
void Ogre2SelectionBuffer::Update()
{
  if (!this->dataPtr->texture)
    return;

  this->dataPtr->materialSwitcher->Reset();

  // manual update - enable the selection workspace, render one frame, then disable it
  this->dataPtr->ogreCompositorWorkspace->setEnabled(true);
  auto engine = Ogre2RenderEngine::Instance();
  auto ogreRoot = engine->OgreRoot();
  ignerr << "SelectionBuffer: before renderOneFrame, workspace valid="
         << this->dataPtr->ogreCompositorWorkspace->isValid()
         << " enabled=" << this->dataPtr->ogreCompositorWorkspace->getEnabled()
         << std::endl;
  bool rendered = ogreRoot->renderOneFrame();
  this->dataPtr->ogreCompositorWorkspace->setEnabled(false);
  ignerr << "SelectionBuffer: renderOneFrame returned " << rendered << std::endl;

  // Download the 1×1 pixel from the GPU using AsyncTextureTicket
  Ogre::TextureGpuManager *texMgr =
      ogreRoot->getRenderSystem()->getTextureGpuManager();

  Ogre::AsyncTextureTicket *ticket = texMgr->createAsyncTextureTicket(
      1u, 1u, 1u, Ogre::TextureTypes::Type2D, this->dataPtr->texture->getPixelFormat());

  ticket->download(this->dataPtr->texture, 0u, true);

  Ogre::TextureBox box = ticket->map(0u);
  if (box.data && this->dataPtr->buffer)
  {
    memcpy(this->dataPtr->buffer, box.data,
        Ogre::PixelFormatGpuUtils::getBytesPerPixel(
            this->dataPtr->texture->getPixelFormat()));
  }
  ticket->unmap();
  texMgr->destroyAsyncTextureTicket(ticket);
}

/////////////////////////////////////////////////
void Ogre2SelectionBuffer::DeleteRTTBuffer()
{
  if (this->dataPtr->ogreCompositorWorkspace)
  {
    this->dataPtr->ogreCompositorWorkspace->removeListener(
        this->dataPtr->materialSwitcher.get());
  }

  if (this->dataPtr->texture)
  {
    auto engine = Ogre2RenderEngine::Instance();
    Ogre::TextureGpuManager *texMgr =
        engine->OgreRoot()->getRenderSystem()->getTextureGpuManager();
    texMgr->destroyTexture(this->dataPtr->texture);
    this->dataPtr->texture = nullptr;
  }

  if (this->dataPtr->buffer)
  {
    delete [] this->dataPtr->buffer;
    this->dataPtr->buffer = nullptr;
  }
}

/////////////////////////////////////////////////
void Ogre2SelectionBuffer::CreateRTTBuffer()
{
  ignerr << "SelectionBuffer: CreateRTTBuffer start" << std::endl;

  auto engine = Ogre2RenderEngine::Instance();
  auto ogreRoot = engine->OgreRoot();
  Ogre::TextureGpuManager *texMgr =
      ogreRoot->getRenderSystem()->getTextureGpuManager();

  ignerr << "SelectionBuffer: creating texture" << std::endl;
  this->dataPtr->texture = texMgr->createTexture(
      "SelectionPassTex",
      Ogre::GpuPageOutStrategy::Discard,
      Ogre::TextureFlags::RenderToTexture,
      Ogre::TextureTypes::Type2D);

  ignerr << "SelectionBuffer: texture created, setting up" << std::endl;
  this->dataPtr->texture->setResolution(1u, 1u);
  this->dataPtr->texture->setNumMipmaps(1u);
  this->dataPtr->texture->setPixelFormat(Ogre::PFG_RGBA8_UNORM);
  ignerr << "SelectionBuffer: scheduling transition to Resident" << std::endl;
  this->dataPtr->texture->scheduleTransitionTo(Ogre::GpuResidency::Resident);
  ignerr << "SelectionBuffer: transition scheduled" << std::endl;

  Ogre::CompositorManager2 *ogreCompMgr = ogreRoot->getCompositorManager2();

  const Ogre::String workspaceName = "SelectionBufferWorkspace" +
      this->dataPtr->camera->getName();

  ignerr << "SelectionBuffer: calling createBasicWorkspaceDef: "
         << workspaceName << std::endl;
  ogreCompMgr->createBasicWorkspaceDef(workspaceName,
      Ogre::ColourValue(1.0f, 0.0f, 1.0f, 1.0f));  // magenta background for debug
  ignerr << "SelectionBuffer: createBasicWorkspaceDef done" << std::endl;

  ignerr << "SelectionBuffer: calling addWorkspace" << std::endl;
  this->dataPtr->ogreCompositorWorkspace =
      ogreCompMgr->addWorkspace(this->dataPtr->scene->OgreSceneManager(),
      this->dataPtr->texture,
      this->dataPtr->selectionCamera, workspaceName, false);
  ignerr << "SelectionBuffer: addWorkspace done, ptr="
         << this->dataPtr->ogreCompositorWorkspace << std::endl;

  ignerr << "SelectionBuffer: calling addListener" << std::endl;
  this->dataPtr->ogreCompositorWorkspace->addListener(
      this->dataPtr->materialSwitcher.get());
  ignerr << "SelectionBuffer: addListener done" << std::endl;

  this->dataPtr->buffer = new uint8_t[4u];
  memset(this->dataPtr->buffer, 0, 4u);
  ignerr << "SelectionBuffer: CreateRTTBuffer complete" << std::endl;
}

/////////////////////////////////////////////////
Ogre::Item *Ogre2SelectionBuffer::OnSelectionClick(const int _x, const int _y)
{
  if (!this->dataPtr->texture)
    return nullptr;

  if (!this->dataPtr->camera)
    return nullptr;

  // Use the stored viewport dimensions instead of getLastViewport(), which
  // returns 0x0 in ogre-next 2.3 because the viewport's mCurrentTarget is
  // only valid during a render pass, not between frames.
  const unsigned int targetWidth = this->dataPtr->viewportWidth;
  const unsigned int targetHeight = this->dataPtr->viewportHeight;

  if (targetWidth == 0 || targetHeight == 0)
  {
    ignerr << "SelectionBuffer: viewport dimensions not set (0x0)" << std::endl;
    return nullptr;
  }

  ignerr << "SelectionBuffer: click (" << _x << "," << _y
         << ") viewport " << targetWidth << "x" << targetHeight << std::endl;
  if (_x < 0 || _y < 0 || _x >= static_cast<int>(targetWidth)
      || _y >= static_cast<int>(targetHeight))
  {
    ignerr << "SelectionBuffer: click out of bounds" << std::endl;
    return nullptr;
  }

  // 1x1 selection buffer, adapted from rviz
  // http://docs.ros.org/indigo/api/rviz/html/c++/selection__manager_8cpp.html
  unsigned int width = 1;
  unsigned int height = 1;
  float x1 = static_cast<float>(_x) /
      static_cast<float>(targetWidth - 1) - 0.5f;
  float y1 = static_cast<float>(_y) /
      static_cast<float>(targetHeight - 1) - 0.5f;
  float x2 = static_cast<float>(_x+width) /
      static_cast<float>(targetWidth - 1) - 0.5f;
  float y2 = static_cast<float>(_y+height) /
      static_cast<float>(targetHeight - 1) - 0.5f;
  Ogre::Matrix4 scaleMatrix = Ogre::Matrix4::IDENTITY;
  Ogre::Matrix4 transMatrix = Ogre::Matrix4::IDENTITY;
  scaleMatrix[0][0] = 1.0 / (x2-x1);
  scaleMatrix[1][1] = 1.0 / (y2-y1);
  transMatrix[0][3] -= x1+x2;
  transMatrix[1][3] += y1+y2;
  // DEBUG: skip pixel-zoom projection to check if selection camera sees any geometry
  // this->dataPtr->selectionCamera->setCustomProjectionMatrix(true,
  //     scaleMatrix * transMatrix * this->dataPtr->camera->getProjectionMatrix());
  this->dataPtr->selectionCamera->setCustomProjectionMatrix(true,
    scaleMatrix * transMatrix * this->dataPtr->camera->getProjectionMatrix());
  this->dataPtr->selectionCamera->setPosition(
      this->dataPtr->camera->getDerivedPosition());
  this->dataPtr->selectionCamera->setOrientation(
      this->dataPtr->camera->getDerivedOrientation());
  ignerr << "SelectionBuffer: selectionCam pos=("
         << this->dataPtr->camera->getDerivedPosition().x << ","
         << this->dataPtr->camera->getDerivedPosition().y << ","
         << this->dataPtr->camera->getDerivedPosition().z << ")"
         << " dir=("
         << this->dataPtr->camera->getDerivedDirection().x << ","
         << this->dataPtr->camera->getDerivedDirection().y << ","
         << this->dataPtr->camera->getDerivedDirection().z << ")"
         << std::endl;
  // Copy full camera state
  this->dataPtr->selectionCamera->setNearClipDistance(
      this->dataPtr->camera->getNearClipDistance());
  this->dataPtr->selectionCamera->setFarClipDistance(
      this->dataPtr->camera->getFarClipDistance());
  this->dataPtr->selectionCamera->setFOVy(
      this->dataPtr->camera->getFOVy());

  // update render texture
  this->Update();

   if (!this->dataPtr->buffer)
    {
      ignerr << "Selection buffer is null." << std::endl;
      return nullptr;
    }

    const uint8_t r = this->dataPtr->buffer[0];
    const uint8_t g = this->dataPtr->buffer[1];
    const uint8_t b = this->dataPtr->buffer[2];
    const uint8_t a = this->dataPtr->buffer[3];

    ignerr << "SelectionBuffer: raw buffer bytes: "
           << (int)r << " "
           << (int)g << " "
           << (int)b << " "
           << (int)a << std::endl;

    math::Color cv(
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        1.0f);

    ignerr << "SelectionBuffer: decoded color r=" << cv.R()
           << " g=" << cv.G() << " b=" << cv.B() << std::endl;

    const std::string &entName =
      this->dataPtr->materialSwitcher->EntityName(cv);
  ignerr << "SelectionBuffer: entity name: '" << entName << "'" << std::endl;

  if (entName.empty())
  {
    return 0;
  }
  else
  {
    auto collection = this->dataPtr->sceneMgr->findMovableObjects(
        Ogre::ItemFactory::FACTORY_TYPE_NAME, entName);
    if (collection.empty())
      return nullptr;
    else
      return dynamic_cast<Ogre::Item *>(collection[0]);
  }
}
