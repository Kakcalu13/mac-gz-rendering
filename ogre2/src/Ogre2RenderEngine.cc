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

// Not Apple or Windows
#if !defined(__APPLE__) && !defined(_WIN32)
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <GL/glx.h>
#endif

#ifdef __APPLE__
# include <dispatch/dispatch.h>
# include <pthread.h>  // for pthread_main_np()
# include <OpenGL/CGLCurrent.h>  // CGLGetCurrentContext / CGLSetCurrentContext
#endif

#ifdef _WIN32
  // Ensure that Winsock2.h is included before Windows.h, which can get
  // pulled in by anybody (e.g., Boost).
  #include <Winsock2.h>
#endif
#include <gz/common/Console.hh>
#include <gz/common/Filesystem.hh>
#include <gz/common/Util.hh>

#include <gz/plugin/Register.hh>

#include "gz/rendering/RenderEngineManager.hh"
#include "gz/rendering/ogre2/Ogre2Includes.hh"
#include <OgreWindow.h>
#include "gz/rendering/ogre2/Ogre2RenderEngine.hh"
#include "gz/rendering/ogre2/Ogre2RenderTypes.hh"
#include "gz/rendering/ogre2/Ogre2Scene.hh"
#include "gz/rendering/ogre2/Ogre2Storage.hh"


class gz::rendering::Ogre2RenderEnginePrivate
{
#if !defined(__APPLE__) && !defined(_WIN32)
  public: XVisualInfo *dummyVisual = nullptr;
#endif

  /// \brief A list of supported fsaa levels
  public: std::vector<unsigned int> fsaaLevels;
};

using namespace gz;
using namespace rendering;

//////////////////////////////////////////////////
Ogre2RenderEnginePlugin::Ogre2RenderEnginePlugin()
{
}

//////////////////////////////////////////////////
std::string Ogre2RenderEnginePlugin::Name() const
{
  return Ogre2RenderEngine::Instance()->Name();
}

//////////////////////////////////////////////////
RenderEngine *Ogre2RenderEnginePlugin::Engine() const
{
  return Ogre2RenderEngine::Instance();
}

//////////////////////////////////////////////////
Ogre2RenderEngine::Ogre2RenderEngine() :
  dataPtr(new Ogre2RenderEnginePrivate)
{
  this->dummyDisplay = nullptr;
  this->dummyContext = 0;
  this->dummyWindowId = 0;

  std::string ogrePath = std::string(OGRE2_RESOURCE_PATH);
  this->ogrePaths.push_back(ogrePath);

#ifdef __APPLE__
  // on OSX the plugins may be placed in the parent lib directory
  if (ogrePath.rfind("OGRE") == ogrePath.size()-4u)
    this->ogrePaths.push_back(ogrePath.substr(0, ogrePath.size()-5));
#endif
}

//////////////////////////////////////////////////
Ogre2RenderEngine::~Ogre2RenderEngine()
{
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::Destroy()
{
  BaseRenderEngine::Destroy();

  if (this->scenes)
  {
    this->scenes->RemoveAll();
  }

  delete this->ogreOverlaySystem;
  this->ogreOverlaySystem = nullptr;

  if (ogreRoot)
  {
    try
    {
      // TODO(anyone): do we need to catch segfault on delete?
      delete this->ogreRoot;
    }
    catch (...)
    {
    }
    this->ogreRoot = nullptr;
  }

  delete this->ogreLogManager;
  this->ogreLogManager = nullptr;

#if !defined(__APPLE__) && !defined(_WIN32)
  if (this->dummyDisplay)
  {
    Display *x11Display = static_cast<Display*>(this->dummyDisplay);
    GLXContext x11Context = static_cast<GLXContext>(this->dummyContext);
    glXDestroyContext(x11Display, x11Context);
    XDestroyWindow(x11Display, this->dummyWindowId);
    XCloseDisplay(x11Display);
    this->dummyDisplay = nullptr;
    XFree(this->dataPtr->dummyVisual);
    this->dataPtr->dummyVisual = nullptr;
  }
#endif
}

//////////////////////////////////////////////////
bool Ogre2RenderEngine::IsEnabled() const
{
  return BaseRenderEngine::IsEnabled();
}

//////////////////////////////////////////////////
std::string Ogre2RenderEngine::Name() const
{
  return "ogre2";
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::AddResourcePath(const std::string &_uri)
{
  if (_uri == "__default__" || _uri.empty())
    return;

  std::string path = common::findFilePath(_uri);

  if (path.empty())
  {
    ignerr << "URI doesn't exist[" << _uri << "]\n";
    return;
  }

  this->resourcePaths.push_back(path);

  try
  {
    if (!Ogre::ResourceGroupManager::getSingleton().resourceLocationExists(
          path, "General"))
    {
      Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
          path, "FileSystem", "General", true);

      Ogre::ResourceGroupManager::getSingleton().initialiseResourceGroup(
          "General", false);
      // Parse all material files in the path if any exist
      if (common::isDirectory(path))
      {
        std::vector<std::string> paths;

        common::DirIter endIter;
        for (common::DirIter dirIter(path); dirIter != endIter; ++dirIter)
        {
          paths.push_back(*dirIter);
        }
        std::sort(paths.begin(), paths.end());

        // Iterate over all the models in the current ign-rendering path
        for (auto dIter = paths.begin(); dIter != paths.end(); ++dIter)
        {
          std::string fullPath = *dIter;
          std::string matExtension = fullPath.substr(fullPath.size()-9);
          if (matExtension == ".material")
          {
            Ogre::DataStreamPtr stream =
              Ogre::ResourceGroupManager::getSingleton().openResource(
                  fullPath, "General");

            // There is a material file under there somewhere, read the thing in
            try
            {
              Ogre::MaterialManager::getSingleton().parseScript(
                  stream, "General");
              Ogre::MaterialPtr matPtr =
                Ogre::MaterialManager::getSingleton().getByName(
                    fullPath);

              if (!matPtr.isNull())
              {
                // is this necessary to do here? Someday try it without
                matPtr->compile();
                matPtr->load();
              }
            }
            catch(Ogre::Exception& e)
            {
              ignerr << "Unable to parse material file[" << fullPath << "]\n";
            }
            stream->close();
          }
        }
      }
    }
  }
  catch(Ogre::Exception &_e)
  {
    ignerr << "Unable to load Ogre Resources.\nMake sure the"
        "resources path in the world file is set correctly." << std::endl;
  }
}

//////////////////////////////////////////////////
Ogre::Root *Ogre2RenderEngine::OgreRoot() const
{
  return this->ogreRoot;
}

//////////////////////////////////////////////////
ScenePtr Ogre2RenderEngine::CreateSceneImpl(unsigned int _id,
    const std::string &_name)
{
  Ogre2ScenePtr scene = Ogre2ScenePtr(new Ogre2Scene(_id, _name));
  this->scenes->Add(scene);
  return scene;
}

//////////////////////////////////////////////////
SceneStorePtr Ogre2RenderEngine::Scenes() const
{
  return this->scenes;
}

//////////////////////////////////////////////////
bool Ogre2RenderEngine::LoadImpl(
    const std::map<std::string, std::string> &_params)
{
  // parse params
  auto it = _params.find("useCurrentGLContext");
  if (it != _params.end())
    std::istringstream(it->second) >> this->useCurrentGLContext;

  try
  {
    this->LoadAttempt();
    this->loaded = true;
    return true;
  }
  catch (Ogre::Exception &ex)
  {
    ignerr << ex.what() << std::endl;
    return false;
  }
  catch (...)
  {
    ignerr << "Failed to load render-engine" << std::endl;
    return false;
  }
}

//////////////////////////////////////////////////
bool Ogre2RenderEngine::InitImpl()
{
  try
  {
    this->InitAttempt();
    return true;
  }
  catch (...)
  {
    ignerr << "Failed to initialize render-engine" << std::endl;
    return false;
  }
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::LoadAttempt()
{
  this->CreateLogger();
  if (!this->useCurrentGLContext)
    this->CreateContext();
  this->CreateRoot();
  this->CreateOverlay();
  this->LoadPlugins();
  this->CreateRenderSystem();
  this->ogreRoot->initialise(false);
  this->CreateRenderWindow();
  this->CreateResources();
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::CreateLogger()
{
  // create log file path
  std::string logPath;
  common::env(IGN_HOMEDIR, logPath);
  logPath = common::joinPaths(logPath, ".ignition", "rendering");
  common::createDirectories(logPath);
  logPath = common::joinPaths(logPath, "ogre2.log");

  // create actual log
  this->ogreLogManager = new Ogre::LogManager();
  this->ogreLogManager->createLog(logPath, true, false, false);
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::CreateContext()
{
#if !defined(__APPLE__) && !defined(_WIN32)
  // create X11 display
  this->dummyDisplay = XOpenDisplay(0);
  Display *x11Display = static_cast<Display*>(this->dummyDisplay);

  if (!this->dummyDisplay)
  {
    ignerr << "Unable to open display: " << XDisplayName(0) << std::endl;
    return;
  }

  // create X11 visual
  int screenId = DefaultScreen(x11Display);

  int attributeList[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 16,
      GLX_STENCIL_SIZE, 8, None };

  this->dataPtr->dummyVisual =
      glXChooseVisual(x11Display, screenId, attributeList);

  if (!this->dataPtr->dummyVisual)
  {
    ignerr << "Unable to create glx visual" << std::endl;
    return;
  }

  // create X11 context
  this->dummyWindowId = XCreateSimpleWindow(x11Display,
      RootWindow(this->dummyDisplay, screenId), 0, 0, 1, 1, 0, 0, 0);

  this->dummyContext = glXCreateContext(x11Display, this->dataPtr->dummyVisual,
                                        nullptr, 1);

  GLXContext x11Context = static_cast<GLXContext>(this->dummyContext);

  if (!this->dummyContext)
  {
    ignerr << "Unable to create glx context" << std::endl;
    return;
  }

  // select X11 context
  glXMakeCurrent(x11Display, this->dummyWindowId, x11Context);
#endif
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::CreateRoot()
{
  try
  {
    this->ogreRoot = new Ogre::Root("", "", "");
  }
  catch (Ogre::Exception &ex)
  {
    ignerr << "Unable to create Ogre root" << std::endl;
  }
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::CreateOverlay()
{
  // Overlay component not built with this ogre-next installation; skip
  // this->ogreOverlaySystem = new Ogre::v1::OverlaySystem();
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::LoadPlugins()
{
  for (auto iter = this->ogrePaths.begin();
       iter != this->ogrePaths.end(); ++iter)
  {
    std::string path(*iter);
    if (!common::isDirectory(path))
      continue;

    std::vector<std::string> plugins;
    std::vector<std::string>::iterator piter;

#ifdef __APPLE__
    std::string extension = ".dylib";
#elif _WIN32
    std::string extension = ".dll";
#else
    std::string extension = ".so";
#endif
#ifdef __APPLE__
    // On macOS, prefer Metal over GL3Plus. GL3Plus uses NSWindow/NSOpenGLContext
    // which must be created on the main thread, but rendering runs on a
    // background thread. Metal does not have this restriction.
    std::string metalPlugin = common::joinPaths(path, "RenderSystem_Metal");
    plugins.push_back(metalPlugin);
#endif
    std::string p = common::joinPaths(path, "RenderSystem_GL3Plus");
    plugins.push_back(p);
    p = common::joinPaths(path, "Plugin_ParticleFX");
    plugins.push_back(p);

    for (piter = plugins.begin(); piter != plugins.end(); ++piter)
    {
      // check if plugin library exists
      std::string filename = *piter+extension;
      if (!common::exists(filename))
      {
        filename = filename + "." + std::string(OGRE2_VERSION);
        if (!common::exists(filename))
        {
          if ((*piter).find("RenderSystem") != std::string::npos)
          {
            ignerr << "Unable to find Ogre Plugin[" << *piter
                   << "]. Rendering will not be possible."
                   << "Make sure you have installed OGRE properly.\n";
          }
          continue;
        }
      }

      // load the plugin
      try
      {
        // Load the plugin into OGRE (ogre-next 2.3: loadPlugin requires optional flag and options)
        this->ogreRoot->loadPlugin(filename, false, nullptr);
        ignerr << "Loaded Ogre Plugin: " << filename << "\n";
      }
      catch(Ogre::Exception &e)
      {
        if ((*piter).find("RenderSystem") != std::string::npos)
        {
          ignerr << "Unable to load Ogre Plugin[" << *piter
                 << "]. Rendering will not be possible."
                 << "Make sure you have installed OGRE properly.\n";
        }
      }
      catch(const std::exception &e)
      {
        ignerr << "std::exception loading Ogre Plugin[" << *piter
               << "]: " << e.what() << "\n";
      }
      catch(...)
      {
        ignerr << "Unknown exception loading Ogre Plugin[" << *piter << "]\n";
      }
    }
  }
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::CreateRenderSystem()
{
  Ogre::RenderSystem *renderSys;
  const Ogre::RenderSystemList *rsList;

  rsList = &(this->ogreRoot->getAvailableRenderers());

  int c = 0;

  renderSys = nullptr;

  do
  {
    if (c == static_cast<int>(rsList->size()))
      break;

    renderSys = rsList->at(c);
    c++;
  }
  // cpplint has a false positive when extending a while call to multiple lines
  // (it thinks the while loop is empty), so we must put the whole while
  // statement on one line and add NOLINT at the end so that cpplint doesn't
  // complain about the line being too long
  while (renderSys && renderSys->getName().compare("OpenGL 3+ Rendering Subsystem") != 0); // NOLINT

#ifdef __APPLE__
  // On Apple Silicon (M2), Apple's OpenGL compatibility layer is incomplete
  // (e.g. glCopyBufferSubData is not implemented), so GL3Plus crashes during
  // VAO initialisation. Always prefer Metal on macOS; use GL3Plus only as a
  // last resort when Metal is unavailable.
  {
    Ogre::RenderSystem *metalSys = nullptr;
    for (int i = 0; i < static_cast<int>(rsList->size()); ++i)
    {
      if (rsList->at(i)->getName().compare("Metal Rendering Subsystem") == 0)
      {
        metalSys = rsList->at(i);
        break;
      }
    }
    if (metalSys)
      renderSys = metalSys;
    // else keep renderSys (GL3Plus or null) as found above
  }
#endif

  ignerr << "CreateRenderSystem: rsList size=" << rsList->size()
         << ", renderSys=" << (renderSys ? renderSys->getName() : "NULL") << "\n";

  if (renderSys == nullptr)
  {
    ignerr << "unable to find OpenGL rendering system. OGRE is probably "
            "installed incorrectly. Double check the OGRE cmake output, "
            "and make sure OpenGL is enabled." << std::endl;
    return;  // avoid null-pointer dereference below
  }

  // We operate in windowed mode
  try { renderSys->setConfigOption("Full Screen", "No"); }
  catch(Ogre::Exception &) {}

  // GL3Plus RTT mode: Metal does not use FBO and will ignore/reject this option
  if (renderSys->getName().find("Metal") == std::string::npos)
  {
    renderSys->setConfigOption("RTT Preferred Mode", "FBO");
  }

  // get all supported fsaa values
  Ogre::ConfigOptionMap configMap = renderSys->getConfigOptions();
  auto fsaaOoption = configMap.find("FSAA");

  if (fsaaOoption != configMap.end())
  {
    auto values = (*fsaaOoption).second.possibleValues;
    for (auto const &str : values)
    {
      int value = 0;
      try
      {
        value = std::stoi(str);
      }
      catch(...)
      {
        continue;
      }
      this->dataPtr->fsaaLevels.push_back(value);
    }
  }
  std::sort(this->dataPtr->fsaaLevels.begin(), this->dataPtr->fsaaLevels.end());

  // check if target fsaa is supported
  unsigned int fsaa = 0;
  unsigned int targetFSAA = 4;
  auto const it = std::find(this->dataPtr->fsaaLevels.begin(),
      this->dataPtr->fsaaLevels.end(), targetFSAA);
  if (it != this->dataPtr->fsaaLevels.end())
    fsaa = targetFSAA;

  renderSys->setConfigOption("FSAA", std::to_string(fsaa));

  this->ogreRoot->setRenderSystem(renderSys);
  ignmsg << "Selected Ogre render system: [" << renderSys->getName() << "]\n";
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::CreateResources()
{
  const char *env = std::getenv("IGN_RENDERING_RESOURCE_PATH");
  std::string resourcePath = (env) ? std::string(env) :
      IGN_RENDERING_RESOURCE_PATH;
  // install path
  std::string mediaPath = common::joinPaths(resourcePath, "ogre2", "media");
  if (!common::exists(mediaPath))
  {
    // src path
    mediaPath = common::joinPaths(resourcePath, "ogre2", "src", "media");
  }

  // register low level materials (ogre v1 materials)
  std::vector< std::pair<std::string, std::string> > archNames;
  std::string p = mediaPath;
  if (common::isDirectory(p))
  {
    archNames.push_back(
        std::make_pair(p, "General"));
    archNames.push_back(
        std::make_pair(p + "/materials/programs", "General"));
    archNames.push_back(
        std::make_pair(p + "/materials/scripts", "General"));

    for (auto aiter = archNames.begin(); aiter != archNames.end(); ++aiter)
    {
      try
      {
        Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
            aiter->first, "FileSystem", aiter->second);
      }
      catch(Ogre::Exception &/*_e*/)
      {
        ignerr << "Unable to load Ogre Resources. Make sure the resources "
            "path in the world file is set correctly." << std::endl;
      }
    }
  }

  // register PbsMaterial resources
  Ogre::String rootHlmsFolder = mediaPath;
  Ogre::String pbsCompositorFolder = common::joinPaths(
      rootHlmsFolder, "2.0", "scripts", "Compositors");
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
      pbsCompositorFolder, "FileSystem", "General");
  Ogre::String commonMaterialFolder = common::joinPaths(
      rootHlmsFolder, "2.0", "scripts", "materials", "Common");
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
      commonMaterialFolder, "FileSystem", "General");
  Ogre::String commonGLSLMaterialFolder = common::joinPaths(
      rootHlmsFolder, "2.0", "scripts", "materials", "Common", "GLSL");
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
      commonGLSLMaterialFolder, "FileSystem", "General");
#ifdef __APPLE__
  // On macOS with Metal render system, register Metal shader resources
  Ogre::String commonMetalMaterialFolder = common::joinPaths(
      rootHlmsFolder, "2.0", "scripts", "materials", "Common", "Metal");
  if (common::isDirectory(commonMetalMaterialFolder))
  {
    Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
        commonMetalMaterialFolder, "FileSystem", "General");
  }
#endif

  // The following code is taken from the registerHlms() function in ogre2
  // samples framework
  if (rootHlmsFolder.empty())
    rootHlmsFolder = "./";
  else if (*(rootHlmsFolder.end() - 1) != '/')
    rootHlmsFolder += "/";

  // At this point rootHlmsFolder should be a valid path to the Hlms data folder

  // For retrieval of the paths to the different folders needed
  Ogre::String mainFolderPath;
  Ogre::StringVector libraryFoldersPaths;
  Ogre::StringVector::const_iterator libraryFolderPathIt;
  Ogre::StringVector::const_iterator libraryFolderPathEn;

  Ogre::ArchiveManager &archiveManager = Ogre::ArchiveManager::getSingleton();

  {
    Ogre::HlmsUnlit *hlmsUnlit = 0;
    // Create & Register HlmsUnlit
    // Get the path to all the subdirectories used by HlmsUnlit
    Ogre::HlmsUnlit::getDefaultPaths(mainFolderPath, libraryFoldersPaths);
    Ogre::Archive *archiveUnlit = archiveManager.load(
        rootHlmsFolder + mainFolderPath, "FileSystem", true);
    Ogre::ArchiveVec archiveUnlitLibraryFolders;
    libraryFolderPathIt = libraryFoldersPaths.begin();
    libraryFolderPathEn = libraryFoldersPaths.end();
    while (libraryFolderPathIt != libraryFolderPathEn)
    {
      const std::string libPath = rootHlmsFolder + *libraryFolderPathIt;
      if (common::isDirectory(libPath))
      {
        Ogre::Archive *archiveLibrary =
            archiveManager.load(libPath, "FileSystem", true);
        archiveUnlitLibraryFolders.push_back(archiveLibrary);
      }
      ++libraryFolderPathIt;
    }

    // Create and register the unlit Hlms
    hlmsUnlit = OGRE_NEW Ogre::HlmsUnlit(archiveUnlit,
        &archiveUnlitLibraryFolders);
    Ogre::Root::getSingleton().getHlmsManager()->registerHlms(hlmsUnlit);

    // enable debug output to see generated Metal shader on compile error
    hlmsUnlit->setDebugOutputPath(true, true, "/tmp/hlms_unlit_");
  }

  {
    Ogre::HlmsPbs *hlmsPbs = 0;
    // Create & Register HlmsPbs
    // Do the same for HlmsPbs:
    Ogre::HlmsPbs::getDefaultPaths(mainFolderPath, libraryFoldersPaths);
    Ogre::Archive *archivePbs = archiveManager.load(
        rootHlmsFolder + mainFolderPath, "FileSystem", true);

    // Get the library archive(s), skipping any that don't exist in this
    // installation (e.g. Any/Main was introduced in ogre-next 2.3 but
    // gz-rendering provides its own platform-specific equivalents).
    Ogre::ArchiveVec archivePbsLibraryFolders;
    libraryFolderPathIt = libraryFoldersPaths.begin();
    libraryFolderPathEn = libraryFoldersPaths.end();
    while (libraryFolderPathIt != libraryFolderPathEn)
    {
      const std::string libPath = rootHlmsFolder + *libraryFolderPathIt;
      if (common::isDirectory(libPath))
      {
        Ogre::Archive *archiveLibrary =
            archiveManager.load(libPath, "FileSystem", true);
        archivePbsLibraryFolders.push_back(archiveLibrary);
      }
      ++libraryFolderPathIt;
    }

    // Create and register
    hlmsPbs = OGRE_NEW Ogre::HlmsPbs(archivePbs, &archivePbsLibraryFolders);
    Ogre::Root::getSingleton().getHlmsManager()->registerHlms(hlmsPbs);

    // disable writting debug output to disk
    hlmsPbs->setDebugOutputPath(false, false);
  }
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::CreateRenderWindow()
{
  // create dummy window
  auto *win = this->CreateOgreWindow(
      std::to_string(this->dummyWindowId), 1, 1, 1.0, 0);
  if (!win)
  {
    ignerr << "Failed to create dummy render window." << std::endl;
  }
}

//////////////////////////////////////////////////
/// \brief Legacy string-name wrapper kept for ABI compat.
std::string Ogre2RenderEngine::CreateRenderWindow(const std::string &_handle,
    const unsigned int _width, const unsigned int _height,
    const double _ratio, const unsigned int _antiAliasing)
{
  Ogre::Window *win = this->CreateOgreWindow(
      _handle, _width, _height, _ratio, _antiAliasing);
  if (!win)
    return std::string();
  return win->getTitle();
}

//////////////////////////////////////////////////
/// ogre-next 2.3: returns Ogre::Window* directly
Ogre::Window *Ogre2RenderEngine::CreateOgreWindow(const std::string &_handle,
    const unsigned int _width, const unsigned int _height,
    const double _ratio, const unsigned int _antiAliasing)
{
  Ogre::NameValuePairList params;

  // if use current gl then don't include window handle params
  if (!this->useCurrentGLContext)
  {
    // Mac and Windows *must* use externalWindow handle.
#if defined(__APPLE__) || defined(_MSC_VER)
    params["externalWindowHandle"] = _handle;
#else
    params["parentWindowHandle"] = _handle;
#endif
  }

  params["FSAA"] = std::to_string(_antiAliasing);
  params["stereoMode"] = "Frame Sequential";

#if defined(__APPLE__)
  // macAPI/cocoa params are only relevant for GL3Plus (Cocoa NSView).
  // Metal uses MTKView and does not use these parameters.
  {
    const Ogre::RenderSystem *rs = this->ogreRoot->getRenderSystem();
    if (rs && rs->getName().find("Metal") == std::string::npos)
    {
      params["macAPI"] = "cocoa";
      params["macAPICocoaUseNSView"] = "true";
    }
  }
#endif

  // Hide window if dimensions are less than or equal to one.
  params["border"] = "none";
  // On macOS/Metal, hide 1x1 dummy context windows — they have no content.
  if (_width <= 1 && _height <= 1)
    params["hidden"] = "true";

  std::ostringstream stream;
  stream << "OgreWindow(0)" << "_" << _handle;

  // Needed for retina displays
  params["contentScalingFactor"] = std::to_string(_ratio);

  // Ogre 2 PBS expects gamma correction
  params["gamma"] = "true";

  if (this->useCurrentGLContext)
  {
    params["externalGLControl"] = "true";
    params["currentGLContext"] = "true";
  }

#ifdef __APPLE__
  __block Ogre::Window *window = nullptr;
  int attempts = 0;
  while (window == nullptr && (attempts++) < 10)
  {
    __block std::exception_ptr eptr = nullptr;
    const std::string winName = stream.str();
    const unsigned int winWidth = _width;
    const unsigned int winHeight = _height;
    Ogre::NameValuePairList *winParams = &params;
    Ogre::Root *root = this->ogreRoot;
    // When using currentGLContext=true (GL3Plus render texture mode), Ogre
    // calls CGLGetCurrentContext() to wrap the existing context.  However,
    // GL3Plus still creates an NSWindow which must happen on the main thread.
    // Solution: capture the render-thread CGL context before dispatching, then
    // set it current on the main thread so Ogre wraps the right context.
    CGLContextObj savedCGL = this->useCurrentGLContext
        ? CGLGetCurrentContext() : nullptr;
    void (^createWin)(void) = ^{
      CGLContextObj prevCGL = nullptr;
      if (savedCGL)
      {
        prevCGL = CGLGetCurrentContext();
        CGLSetCurrentContext(savedCGL);
      }
      try
      {
        // ogre-next 2.3: createRenderWindow returns Ogre::Window*
        window = root->createRenderWindow(
            winName, winWidth, winHeight, false, winParams);
      }
      catch (...)
      {
        eptr = std::current_exception();
      }
      if (savedCGL)
        CGLSetCurrentContext(prevCGL);
    };
    if (pthread_main_np())
      createWin();
    else
      dispatch_sync(dispatch_get_main_queue(), createWin);
    if (eptr)
    {
      try { std::rethrow_exception(eptr); }
      catch (const std::exception &_e)
      {
        ignerr << " Unable to create the rendering window: " << _e.what()
               << std::endl;
        window = nullptr;
      }
    }
  }
#else
  Ogre::Window *window = nullptr;
  int attempts = 0;
  while (window == nullptr && (attempts++) < 10)
  {
    try
    {
      // ogre-next 2.3: createRenderWindow returns Ogre::Window*
      window = this->ogreRoot->createRenderWindow(
          stream.str(), _width, _height, false, &params);
    }
    catch(const std::exception &_e)
    {
      ignerr << " Unable to create the rendering window: " << _e.what()
             << std::endl;
      window = nullptr;
    }
  }
#endif

  if (attempts >= 10)
  {
    ignerr << "Unable to create the rendering window after [" << attempts
           << "] attempts." << std::endl;
    return nullptr;
  }

  if (window)
  {
    // ogre-next 2.3: _setVisible instead of setVisible
    window->_setVisible(true);
    window->reposition(0, 0);
  }
  return window;
}

//////////////////////////////////////////////////
void Ogre2RenderEngine::InitAttempt()
{
  this->initialized = false;

  // init the resources
  Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups(false);

  this->scenes = Ogre2SceneStorePtr(new Ogre2SceneStore);
}

/////////////////////////////////////////////////
std::vector<unsigned int> Ogre2RenderEngine::FSAALevels() const
{
  return this->dataPtr->fsaaLevels;
}

/////////////////////////////////////////////////
Ogre::v1::OverlaySystem *Ogre2RenderEngine::OverlaySystem() const
{
  return this->ogreOverlaySystem;
}

// Register this plugin
IGNITION_ADD_PLUGIN(Ogre2RenderEnginePlugin,
                    rendering::RenderEnginePlugin)
