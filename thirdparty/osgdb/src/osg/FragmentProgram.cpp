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
#include <osg/Notify>
#include <osg/GLExtensions>
#include <osg/FragmentProgram>
#include <osg/State>
#include <osg/Timer>
#include <osg/ContextData>

using namespace osg;

class GLFragmentProgramManager : public GLObjectManager
{
public:
    GLFragmentProgramManager(unsigned int contextID) : GLObjectManager("GLFragmentProgramManager",contextID) {}

    virtual void deleteGLObject(GLuint globj)
    {
        const GLExtensions* extensions = GLExtensions::Get(_contextID,true);
        if (extensions->isGlslSupported) extensions->glDeletePrograms(1, &globj );
    }
};

FragmentProgram::FragmentProgram()
{
}


FragmentProgram::FragmentProgram(const FragmentProgram& vp,const CopyOp& copyop):
    osg::StateAttribute(vp,copyop)
{
    _fragmentProgram = vp._fragmentProgram;

    for( LocalParamList::const_iterator itr = vp._programLocalParameters.begin();
        itr != vp._programLocalParameters.end(); ++itr )
    {
        _programLocalParameters[itr->first] = itr->second;
    }

    for( MatrixList::const_iterator mitr = vp._matrixList.begin();
        mitr != vp._matrixList.end(); ++mitr )
    {
        _matrixList[mitr->first] = mitr->second;
    }
}


// virtual
FragmentProgram::~FragmentProgram()
{
    dirtyFragmentProgramObject();
}

void FragmentProgram::dirtyFragmentProgramObject()
{

}

void FragmentProgram::apply(State& state) const
{

}

void FragmentProgram::resizeGLObjectBuffers(unsigned int maxSize)
{
    _fragmentProgramIDList.resize(maxSize);
}

void FragmentProgram::releaseGLObjects(State* state) const
{
   
}
