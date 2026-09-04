/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/
#include <osg/GLExtensions>
#include <osg/GL>
#include <osg/Notify>
#include <osg/Math>
#include <osg/buffered_value>
#include <osg/os_utils>
#include <osg/ApplicationUsage>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

#include <string>
#include <set>
#include <sstream>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif // WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif // NOMINMAX
    #include <windows.h>
#elif defined(__APPLE__)
    // The NS*Symbol* stuff found in <mach-o/dyld.h> is deprecated.
    // Since 10.3 (Panther) OS X has provided the dlopen/dlsym/dlclose
    // family of functions under <dlfcn.h>. Since 10.4 (Tiger), Apple claimed
    // the dlfcn family was significantly faster than the NS*Symbol* family.
    // Since 'deprecated' needs to be taken very seriously with the
    // coming of 10.5 (Leopard), it makes sense to use the dlfcn family when possible.
    #include <AvailabilityMacros.h>
    #if !defined(MAC_OS_X_VERSION_10_3) || (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_3)
        #define USE_APPLE_LEGACY_NSSYMBOL
        #include <mach-o/dyld.h>
    #else
        #include <dlfcn.h>
    #endif
#elif defined(__EMSCRIPTEN__)
    // Emscripten ships EGL, which we use to get OpenGL function addresses.
    #include <EGL/egl.h>
#else
    #include <dlfcn.h>
#endif

using namespace osg;

typedef std::set<std::string>  ExtensionSet;
struct GLExtensions::ExtensionData : public Referenced
{
    osg::buffered_object<ExtensionSet> glExtensionSetList;
    osg::buffered_object<std::string> glRendererList;
    osg::buffered_value<int> glInitializedList;
};

ref_ptr<GLExtensions::ExtensionData> s_extensionData(new GLExtensions::ExtensionData);

static ApplicationUsageProxy GLEXtension_e0(ApplicationUsage::ENVIRONMENTAL_VARIABLE, "OSG_GL_EXTENSION_DISABLE <value>", "Use space deliminarted list of GL extensions to disable associated GL extensions");
static ApplicationUsageProxy GLEXtension_e1(ApplicationUsage::ENVIRONMENTAL_VARIABLE, "OSG_MAX_TEXTURE_SIZE <value>", "Clamp the maximum GL texture size to specified value.");

float osg::getGLVersionNumber()
{
    char* versionstring = nullptr;
    if (!versionstring) return 0.0;

    return (findAsciiToFloat(versionstring));
}

bool osg::isExtensionInExtensionString(const char *extension, const char *extensionString)
{
    const char *startOfWord = extensionString;
    const char *endOfWord;
    while ((endOfWord = strchr(startOfWord,' ')) != 0)
    {
        if (strncmp(extension, startOfWord, endOfWord - startOfWord) == 0)
            return true;
        startOfWord = endOfWord+1;
    }
    if (*startOfWord && strcmp(extension, startOfWord) == 0)
        return true;

   return false;
}

bool osg::isGLExtensionSupported(unsigned int contextID, const char *extension)
{
    return osg::isGLExtensionOrVersionSupported(contextID, extension, FLT_MAX);
}

bool osg::isGLExtensionSupported(unsigned int contextID, const char *extension1, const char *extension2)
{
    return osg::isGLExtensionOrVersionSupported(contextID, extension1, FLT_MAX) ||
           osg::isGLExtensionOrVersionSupported(contextID, extension2, FLT_MAX);
}

bool osg::isGLExtensionOrVersionSupported(unsigned int contextID, const char *extension, float requiredGLVersion)
{
    return false;
}

void osg::setGLExtensionDisableString(const std::string& disableString)
{
    getGLExtensionDisableString() = disableString;
}

std::string& osg::getGLExtensionDisableString()
{
    static std::string s_GLExtensionDisableString(getEnvVar("OSG_GL_EXTENSION_DISABLE"));

    return s_GLExtensionDisableString;
}

OSG_INIT_SINGLETON_PROXY(GLExtensionDisableStringInitializationProxy, osg::getGLExtensionDisableString())


void* osg::getGLExtensionFuncPtr(const char *funcName)
{
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////
// Static array of percontext osg::GLExtensions instances

typedef osg::buffered_object< osg::ref_ptr<GLExtensions> > BufferedExtensions;
static BufferedExtensions s_extensions;

GLExtensions* GLExtensions::Get(unsigned int in_contextID, bool createIfNotInitalized)
{
    if (!s_extensions[in_contextID] && createIfNotInitalized)
            s_extensions[in_contextID] = new GLExtensions(in_contextID);

    return s_extensions[in_contextID].get();
}

void GLExtensions::Set(unsigned int in_contextID, GLExtensions* extensions)
{
    s_extensions[in_contextID] = extensions;
}

///////////////////////////////////////////////////////////////////////////
// Extension function pointers for OpenGL v2.x


GLExtensions::GLExtensions(unsigned int in_contextID):
    contextID(in_contextID),
    _extensionData(s_extensionData)
{
    

}

GLExtensions::~GLExtensions()
{

}

///////////////////////////////////////////////////////////////////////////
// C++-friendly convenience methods

GLuint GLExtensions::getCurrentProgram() const
{
    return 0;
}


bool GLExtensions::getProgramInfoLog( GLuint program, std::string& result ) const
{
    return false;
}


bool GLExtensions::getShaderInfoLog( GLuint shader, std::string& result ) const
{
    return false;
}


bool GLExtensions::getAttribLocation( const char* attribName, GLuint& location ) const
{
    return false;
}


bool GLExtensions::getFragDataLocation( const char* fragDataName, GLuint& location ) const
{
    return false;
}

