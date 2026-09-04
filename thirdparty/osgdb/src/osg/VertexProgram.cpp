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
#include <osg/VertexProgram>
#include <osg/State>
#include <osg/Timer>
#include <osg/ContextData>

using namespace osg;

class GLVertexProgramManager : public GLObjectManager
{
public:
    GLVertexProgramManager(unsigned int contextID) : GLObjectManager("GLVertexProgramManager",contextID) {}

    virtual void deleteGLObject(GLuint globj)
    {
        const GLExtensions* extensions = GLExtensions::Get(_contextID,true);
        if (extensions->isGlslSupported) extensions->glDeletePrograms(1, &globj );
    }
};

VertexProgram::VertexProgram()
{
}


VertexProgram::VertexProgram(const VertexProgram& vp,const CopyOp& copyop):
    osg::StateAttribute(vp,copyop)
{
    _vertexProgram = vp._vertexProgram;

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
VertexProgram::~VertexProgram()
{
    dirtyVertexProgramObject();
}

void VertexProgram::dirtyVertexProgramObject()
{

}

void VertexProgram::apply(State& state) const
{

}

void VertexProgram::resizeGLObjectBuffers(unsigned int maxSize)
{
    _vertexProgramIDList.resize(maxSize);
}

void VertexProgram::releaseGLObjects(State* state) const
{
   
}
