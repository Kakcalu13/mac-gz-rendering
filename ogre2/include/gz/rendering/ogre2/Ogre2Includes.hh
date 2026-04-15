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
#ifndef GZ_RENDERING_OGRE2_OGRE2INCLUDES_HH_
#define GZ_RENDERING_OGRE2_OGRE2INCLUDES_HH_

// This disables warning messages for OGRE
#ifndef _MSC_VER
  #pragma GCC system_header
#else
  #pragma warning(push, 0)
#endif

// This prevents some deprecation #warning messages on OSX 10.9
#pragma clang diagnostic ignored "-W#warnings"

// ogre-next 2.3: core headers (Ogre.h no longer pulls everything in)
#include <Ogre.h>
#include <OgreBillboard.h>
#include <OgreMovableObject.h>
#include <OgreRenderable.h>
#include <OgrePlugin.h>
#include <OgreDataStream.h>
#include <OgreLogManager.h>
#include <OgreWindowEventUtilities.h>
#include <OgreSceneQuery.h>
#include <OgreRoot.h>
#include <OgreItem.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreVector3.h>
#include <OgreManualObject.h>
#include <OgreMaterialManager.h>
#include <OgreMatrix4.h>
#include <OgreColourValue.h>
#include <OgreQuaternion.h>
#include <OgreMesh2.h>
#include <OgreSubMesh2.h>
#include <OgreMeshManager2.h>
#include <OgreHardwareBufferManager.h>
#include <OgreCamera.h>
#include <OgreNode.h>
#include <OgreSimpleRenderable.h>
#include <OgreFrameListener.h>
#include <OgreRenderObjectListener.h>
#include <OgreOldSkeletonManager.h>
#include <Animation/OgreSkeletonInstance.h>
#include <OgreTextureUnitState.h>
#include <OgreGpuProgramManager.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreShadowCameraSetupPSSM.h>

// ogre-next 2.3: new texture GPU management
#include <OgreTextureGpu.h>
#include <OgreTextureGpuManager.h>
#include <OgreTextureBox.h>
#include <OgrePixelFormatGpu.h>
#include <OgrePixelFormatGpuUtils.h>
#include <OgreStagingTexture.h>
#include <OgreAsyncTextureTicket.h>
#include <OgrePlatformInformation.h>
#include <OgreWindow.h>
#include <Vao/OgreVaoManager.h>

#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreDepthBuffer.h>
// OgreRectangle2D: ogre-next 2.3 renamed this to OgreRectangle2D2
#include <OgreRectangle2D2.h>

#include <OgreHlmsManager.h>
// OgreHlmsTextureManager was removed in ogre-next 2.2+; use TextureGpuManager instead
#include <Hlms/Unlit/OgreHlmsUnlit.h>
#include <Hlms/Pbs/OgreHlmsPbs.h>

#include <Compositor/OgreCompositorManager2.h>
#include <Compositor/OgreCompositorNode.h>
#include <Compositor/OgreCompositorNodeDef.h>
#include <Compositor/OgreCompositorShadowNode.h>
#include <Compositor/OgreCompositorWorkspace.h>
#include <Compositor/OgreCompositorWorkspaceListener.h>
#include <Compositor/Pass/PassClear/OgreCompositorPassClearDef.h>
#include <Compositor/Pass/PassClear/OgreCompositorPassClear.h>
#include <Compositor/Pass/PassQuad/OgreCompositorPassQuadDef.h>
#include <Compositor/Pass/PassScene/OgreCompositorPassSceneDef.h>
#include <OgreRenderPassDescriptor.h>

// Overlay component not built with this ogre-next installation; omit overlay headers

// TODO(anyone): enable when ogre 2.1 fully supports paging
// and terrain components
// #include <Paging/OgrePageManager.h>
// #include <Paging/OgrePagedWorld.h>
// #include <Terrain/OgreTerrainPaging.h>
// #include <Terrain/OgreTerrainMaterialGeneratorA.h>
// #include <Terrain/OgreTerrain.h>
// #include <Terrain/OgreTerrainGroup.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
