/*! \file   OpenDungeonsApplication.cpp
 *  \author Ogre team, andrewbuck, oln, StefanP.MUC
 *  \date   07 April 2011
 *  \brief  Class OpenDungeonsApplication containing everything to start the game
 */

#include <string>
#include <sstream>
#include <fstream>

#include <OgreConfigFile.h>
#include <sys/stat.h>
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#include <shlwapi.h>
#include <direct.h>
#endif

#include "Globals.h"
#include "Functions.h"
#include "ODFrameListener.h"
#include "GameMap.h"
#include "TextRenderer.h"
#include "RenderManager.h"
#include "MusicPlayer.h"
#include "SoundEffectsHelper.h"
#include "Gui.h"

#include "OpenDungeonsApplication.h"

template<> OpenDungeonsApplication*
        Ogre::Singleton<OpenDungeonsApplication>::ms_Singleton = 0;

/*! \brief Returns a reference to the singleton object
 *
 */
OpenDungeonsApplication& OpenDungeonsApplication::getSingleton()
{
    assert (ms_Singleton);
    return (*ms_Singleton);
}

/*! \brief Returns a pointer to the singleton object
 *
 */
OpenDungeonsApplication* OpenDungeonsApplication::getSingletonPtr()
{
    return ms_Singleton;
}

OpenDungeonsApplication::OpenDungeonsApplication() :
        mRoot(0)
{
    /* Provide a nice cross platform solution for locating the configuration
     * files. On windows files are searched for in the current working
     * directory, on OS X however you must provide the full path, the helper
     * function macBundlePath does this for us.
     */
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
    mResourcePath = macBundlePath() + "/Contents/Resources/";
//Actually this can be other things than linux as well
#elif OGRE_PLATFORM == OGRE_PLATFORM_LINUX
    // Get path of data
    // getenv return value should not be touched/freed.
    char* path = std::getenv("OPENDUNGEONS_DATA_PATH");
    if (path)
    {
        mResourcePath = path;
        if (*mResourcePath.end() != '/') //Make sure we have trailing slash
        {
            mResourcePath.append("/");
        }
    }
    else
    {
        mResourcePath = "";
    }
#else
    mResourcePath = "";
#endif
    mHomePath = getHomePath();

    if (!setup())
    {
        return;
    }

    mRoot->startRendering();
}

OpenDungeonsApplication::~OpenDungeonsApplication()
{
    if(mRoot)
        delete mRoot;
}

/*! \brief Sets up the application - returns false if the user chooses to abandon
 *  configuration.
 */
bool OpenDungeonsApplication::setup()
{
    Ogre::String pluginsPath = "";
    // only use plugins.cfg if not static
#ifndef OGRE_STATIC_LIB
    pluginsPath = mResourcePath + "plugins.cfg";
#endif

    mRoot = new Ogre::Root(pluginsPath, mHomePath + "ogre.cfg", mHomePath
            + "Ogre.log");

    setupResources();

    /* Show the configuration dialog and initialise the system
     * You can skip this and use root.restoreConfig() to load configuration
     * settings if you were sure there are valid ones saved in ogre.cfg
     */
    if(!mRoot->showConfigDialog())
        return false;

    mWindow = mRoot->initialise(true);

    new RenderManager(&gameMap);
    RenderManager::getSingletonPtr()->sceneManager
            = mRoot->createSceneManager(Ogre::ST_EXTERIOR_CLOSE);

    createCamera();
    createViewports();

    // Set default mipmap level (NB some APIs ignore this)
    Ogre::TextureManager::getSingleton().setDefaultNumMipmaps(5);

    // Load resources
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

    //instanciate all singleton helper classes
    new SoundEffectsHelper();
    new Gui();
    new TextRenderer();
    new MusicPlayer(mResourcePath + "music/");

    SoundEffectsHelper::getSingletonPtr()->initialiseSound(mResourcePath + "sounds/");
    //TODO: load main menu first, only start game if user clicks on new game
    Gui::getSingletonPtr()->loadGuiSheet(Gui::ingameMenu);
    TextRenderer::getSingleton().addTextBox("DebugMessages", MOTD.c_str(), 140,
            10, 50, 70, Ogre::ColourValue::Green);
    //TODO - move this to when the map is actually loaded
    MusicPlayer::getSingleton().start(0);

    createScene();

    //create the framelistener
    new ODFrameListener(mWindow, mCamera, true, true, false);
    ODFrameListener::getSingletonPtr()->showDebugOverlay(true);
    mRoot->addFrameListener(ODFrameListener::getSingletonPtr());

    return true;
}

/*! \brief Sets up the main camera
 */
void OpenDungeonsApplication::createCamera()
{
    Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->sceneManager;
    mCamera = mSceneMgr->createCamera("PlayerCam");
    mCamera->setNearClipDistance(.05);
    mCamera->setFarClipDistance(300.0);
    mCamera->setAutoTracking(false, mSceneMgr->getRootSceneNode()
            ->createChildSceneNode("CameraTarget"), Ogre::Vector3(0, 0, 0));
}

void OpenDungeonsApplication::createScene()
{
    // Turn on shadows
    //mSceneMgr->setShadowTechnique(SHADOWTYPE_TEXTURE_MODULATIVE);	// Quality 1
    //mSceneMgr->setShadowTechnique(SHADOWTYPE_STENCIL_MODULATIVE);	// Quality 2
    //mSceneMgr->setShadowTechnique(SHADOWTYPE_STENCIL_ADDITIVE);	// Quality 3

    /* TODO: move level loading to a better place
     *       (own class to exclude from global skope?)
     *       and generalize it for the future when we have more levels
     */
    // Read in the default game map
    std::string levelPath = mResourcePath + "levels_git/Test.level";
    {
        //Check if the level from git exists. If not, use the standard one.
        std::ifstream file(levelPath.c_str(), std::ios_base::in);
        if (!file.is_open())
        {
            levelPath = mResourcePath + "levels/Test.level";
        }
    }

    gameMap.levelFileName = "Test";
    readGameMapFromFile(levelPath);

    // Create ogre entities for the tiles, rooms, and creatures
    gameMap.createAllEntities();

    Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->sceneManager;
    // Create the main scene lights
    mSceneMgr->setAmbientLight(Ogre::ColourValue(0.3, 0.36, 0.28));

    // Create the scene node that the camera attaches to
    Ogre::SceneNode* node = mSceneMgr->getRootSceneNode()
            ->createChildSceneNode("CamNode1", Ogre::Vector3(1, -1, 16));
    node->pitch(Ogre::Degree(25), Ogre::Node::TS_WORLD);
    node->roll(Ogre::Degree(30), Ogre::Node::TS_WORLD);
    node->attachObject(mCamera);

    // Create the single tile selection mesh
    Ogre::Entity* ent = mSceneMgr->createEntity("SquareSelector", "SquareSelector.mesh");
    node = mSceneMgr->getRootSceneNode()->createChildSceneNode(
            "SquareSelectorNode");
    node->translate(Ogre::Vector3(0, 0, 0));
    node->scale(Ogre::Vector3(BLENDER_UNITS_PER_OGRE_UNIT,
            BLENDER_UNITS_PER_OGRE_UNIT, BLENDER_UNITS_PER_OGRE_UNIT));
#if OGRE_VERSION < ((1 << 16) | (6 << 8) | 0)
    ent->setNormaliseNormals(true);
#endif
    node->attachObject(ent);
    Ogre::SceneNode *node2 = node->createChildSceneNode("Hand_node");
    node2->setPosition(0.0 / BLENDER_UNITS_PER_OGRE_UNIT, 0.0
            / BLENDER_UNITS_PER_OGRE_UNIT, 3.0 / BLENDER_UNITS_PER_OGRE_UNIT);
    node2->scale(Ogre::Vector3(1.0 / BLENDER_UNITS_PER_OGRE_UNIT, 1.0
            / BLENDER_UNITS_PER_OGRE_UNIT, 1.0 / BLENDER_UNITS_PER_OGRE_UNIT));

    // Create the light which follows the single tile selection mesh
    Ogre::Light* light = mSceneMgr->createLight("MouseLight");
    light->setType(Ogre::Light::LT_POINT);
    light->setDiffuseColour(Ogre::ColourValue(.5, .7, .6));
    light->setSpecularColour(Ogre::ColourValue(.5, .4, .4));
    light->setPosition(0, 0, 5);
    light->setAttenuation(20, 0.15, 0.15, 0.017);
    node->attachObject(light);
}

/*! \brief setup the viewports
 *
 */
void OpenDungeonsApplication::createViewports()
{
    // Create one viewport, entire window
    Ogre::Viewport* vp = mWindow->addViewport(mCamera);
    vp->setBackgroundColour(Ogre::ColourValue(0, 0, 0));

    // Alter the camera aspect ratio to match the viewport
    mCamera->setAspectRatio(Ogre::Real(vp->getActualWidth()) / Ogre::Real(
            vp->getActualHeight()));
}

/*  \brief Method which will define the source of resources
 * (other than current folder)
 */
void OpenDungeonsApplication::setupResources()
{
    // Load resource paths from config file
    Ogre::ConfigFile cf;
    cf.load(mResourcePath + "resources.cfg");

    // Go through all sections & settings in the file
    Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();

    Ogre::String secName, typeName, archName;
    while (seci.hasMoreElements())
    {
        secName = seci.peekNextKey();
        Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
        Ogre::ConfigFile::SettingsMultiMap::iterator i;
        for (i = settings->begin(); i != settings->end(); ++i)
        {
            typeName = i->first;
            archName = mResourcePath + i->second;
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
            // OS X does not set the working directory relative to the app,
            // In order to make things portable on OS X we need to provide
            // the loading with it's own bundle path location
            ResourceGroupManager::getSingleton().addResourceLocation(
                    String(macBundlePath() + "/" + archName), typeName, secName, true);
#else
            Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                    archName, typeName, secName, true);
#endif
        }
    }
}

std::string OpenDungeonsApplication::getHomePath()
{
    std::string homePath;

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
    homePath = "./";
#elif OGRE_PLATFORM == OGRE_PLATFORM_LINUX

    /* If variable is not set, assume we are in a build dir and
     * use the current dir for config files.
     */
    char* useHomeDir = std::getenv("OPENDUNGEONS_DATA_PATH");
    if (useHomeDir)
    {
        //On linux and similar, use home dir
        //http://linux.die.net/man/3/getpwuid
        char* path = std::getenv("HOME");
        if (path)
        {
            homePath = path;
        }
        else //In the unlikely event that home is not defined, use current  working dir
        {
            homePath = ".";
        }
        homePath.append("/.OpenDungeons");

        //Create directory if it doesn't exist
        struct stat statbuf;
        int status = stat(homePath.c_str(), &statbuf);
        if (status != 0)
        {
            int dirCreateStatus;
            dirCreateStatus = mkdir(homePath.c_str(), S_IRWXU | S_IRWXG
                    | S_IROTH | S_IXOTH);

        }

        homePath.append("/");
    }
    else
    {
        homePath = "./";
    }
#else
    homePath = "./";
#endif

    return homePath;
}

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
#include <CoreFoundation/CoreFoundation.h>

// This function will locate the path to our application on OS X,
// unlike windows you can not rely on the curent working directory
// for locating your configuration files and resources.
std::string OpenDungeonsApplication::macBundlePath()
{
    char path[1024];
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    assert(mainBundle);

    CFURLRef mainBundleURL = CFBundleCopyBundleURL(mainBundle);
    assert(mainBundleURL);

    CFStringRef cfStringRef = CFURLCopyFileSystemPath( mainBundleURL, kCFURLPOSIXPathStyle);
    assert(cfStringRef);

    CFStringGetCString(cfStringRef, path, 1024, kCFStringEncodingASCII);

    CFRelease(mainBundleURL);
    CFRelease(cfStringRef);

    return std::string(path);
}
#endif

const unsigned int OpenDungeonsApplication::PORT_NUMBER = 31222;
const double OpenDungeonsApplication::BLENDER_UNITS_PER_OGRE_UNIT = 10.0;
const double OpenDungeonsApplication::DEFAULT_FRAMES_PER_SECOND = 60.0;
const std::string OpenDungeonsApplication::VERSION = "0.4.7";
const std::string OpenDungeonsApplication::POINTER_INFO_STRING = "pointerInfo";
const std::string OpenDungeonsApplication::HELP_MESSAGE = "\
The console is a way of interacting with the underlying game engine directly.\
Commands given to the the console are made up of two parts: a \'command name\' and one or more \'arguments\'.\
For information on how to use a particular command, type help followed by the command name.\
\n\nThe following commands are avaliable:\
\n\thelp keys - shows the keyboard controls\
\n\tlist - print out lists of creatures, classes, etc\n\thelp - displays this help screen\n\tsave - saves the current level to a file\
\n\tload - loads a level from a file\
\n\tquit - exit the program\
\n\tmaxmessages - Sets or displays the max number of chat messages to display\
\n\tmaxtime - Sets or displays the max time for chat messages to be displayed\
\n\ttermwidth - set the terminal width\
\n\taddcreature - load a creature into the file.\
\n\taddclass - Define a creature class\
\n\taddtiles - adds a rectangular region of tiles\
\n\tnewmap - Creates a new rectangular map\
\n\trefreshmesh - Reloads the meshes for all the objects in the game\
\n\tmovespeed - sets the camera movement speed\
\n\trotatespeed - sets the camera rotation speed\
\n\tfps - sets the maximum framerate\
\n\tturnspersecond - sets the number of turns the AI will carry out per second\
\n\tmousespeed - sets the mouse speed\
\n\tambientlight - set the ambient light color\
\n\tconnect - connect to a server\
\n\thost - host a server\
\n\tchat - send a message to other people in the game\
\n\tnearclip - sets the near clipping distance\
\n\tfarclip - sets the far clipping distance\
\n\tvisdebug - turns on visual debugging for a creature\
\n\taddcolor - adds another player color\
\n\tsetcolor - changes the value of one of the player's color\
\n\tdisconnect - stops a running server or client and returns to the map editor\
\n\taithreads - sets the maximum number of creature AI threads on the server";
