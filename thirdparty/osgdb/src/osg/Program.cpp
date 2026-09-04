/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 * Copyright (C) 2003-2005 3Dlabs Inc. Ltd.
 * Copyright (C) 2004-2005 Nathan Cournia
 * Copyright (C) 2008 Zebra Imaging
 * Copyright (C) 2010 VIRES Simulationstechnologie GmbH
 * Copyright (C) 2012 David Callu
 *
 * This application is open source and may be redistributed and/or modified
 * freely and without restriction, both in commercial and non commercial
 * applications, as long as this copyright notice is maintained.
 *
 * This application is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
*/

/* file:        src/osg/Program.cpp
 * author:      Mike Weiblen 2008-01-19
 *              Holger Helmich 2010-10-21
*/

#include <list>
#include <fstream>

#include <osg/Notify>
#include <osg/State>
#include <osg/Timer>
#include <osg/buffered_value>
#include <osg/ref_ptr>
#include <osg/Program>
#include <osg/Shader>
#include <osg/GLExtensions>
#include <osg/ContextData>

#include <string.h>

using namespace osg;

class GLProgramManager : public GLObjectManager
{
public:
    GLProgramManager(unsigned int contextID) : GLObjectManager("GLProgramManager", contextID) {}

    virtual void deleteGLObject(GLuint globj)
    {
        const GLExtensions* extensions = GLExtensions::Get(_contextID,true);
        if (extensions->isGlslSupported) extensions->glDeleteProgram( globj );
    }
};


///////////////////////////////////////////////////////////////////////////
// osg::Program::ProgramBinary
///////////////////////////////////////////////////////////////////////////

Program::ProgramBinary::ProgramBinary() : _format(0)
{
}

Program::ProgramBinary::ProgramBinary(const ProgramBinary& rhs, const osg::CopyOp& copyop) :
    osg::Object(rhs, copyop),
    _data(rhs._data), _format(rhs._format)
{
}

void Program::ProgramBinary::allocate(unsigned int size)
{
    _data.clear();
    _data.resize(size);
}

void Program::ProgramBinary::assign(unsigned int size, const unsigned char* data)
{
    allocate(size);
    if (data)
    {
        for(unsigned int i=0; i<size; ++i)
        {
            _data[i] = data[i];
        }
    }
}


///////////////////////////////////////////////////////////////////////////
// osg::Program
///////////////////////////////////////////////////////////////////////////

Program::Program() :
    _geometryVerticesOut(1), _geometryInputType(GL_TRIANGLES),
    _geometryOutputType(GL_TRIANGLE_STRIP), _feedbackmode(GL_SEPARATE_ATTRIBS)
{
}


Program::Program(const Program& rhs, const osg::CopyOp& copyop):
    osg::StateAttribute(rhs, copyop)
{

    if ((copyop.getCopyFlags()&osg::CopyOp::DEEP_COPY_STATEATTRIBUTES)!=0)
    {
        for( unsigned int shaderIndex=0; shaderIndex < rhs.getNumShaders(); ++shaderIndex )
        {
            addShader( new osg::Shader( *rhs.getShader( shaderIndex ), copyop ) );
        }
    }
    else
    {
        for( unsigned int shaderIndex=0; shaderIndex < rhs.getNumShaders(); ++shaderIndex )
        {
            addShader( const_cast<osg::Shader*>(rhs.getShader( shaderIndex )) );
        }
    }

    const osg::Program::AttribBindingList &abl = rhs.getAttribBindingList();
    for( osg::Program::AttribBindingList::const_iterator attribute = abl.begin(); attribute != abl.end(); ++attribute )
    {
        addBindAttribLocation( attribute->first, attribute->second );
    }

    const osg::Program::FragDataBindingList &fdl = rhs.getFragDataBindingList();
    for( osg::Program::FragDataBindingList::const_iterator fragdata = fdl.begin(); fragdata != fdl.end(); ++fragdata )
    {
        addBindFragDataLocation( fragdata->first, fragdata->second );
    }

    _geometryVerticesOut = rhs._geometryVerticesOut;
    _geometryInputType = rhs._geometryInputType;
    _geometryOutputType = rhs._geometryOutputType;

    _feedbackmode=rhs._feedbackmode;
    _feedbackout=rhs._feedbackout;
}


Program::~Program()
{
    // inform any attached Shaders that we're going away
    for( unsigned int i=0; i < _shaderList.size(); ++i )
    {
        _shaderList[i]->removeProgramRef( this );
    }
}


int Program::compare(const osg::StateAttribute& sa) const
{
    return 0; // passed all the above comparison macros, must be equal.
}


void Program::compileGLObjects( osg::State& state ) const
{
  
}

void Program::setThreadSafeRefUnref(bool threadSafe)
{
    StateAttribute::setThreadSafeRefUnref(threadSafe);

    for( unsigned int i=0; i < _shaderList.size(); ++i )
    {
        if (_shaderList[i].valid()) _shaderList[i]->setThreadSafeRefUnref(threadSafe);
    }
}

void Program::dirtyProgram()
{
    
}


void Program::resizeGLObjectBuffers(unsigned int maxSize)
{
    for( unsigned int i=0; i < _shaderList.size(); ++i )
    {
        if (_shaderList[i].valid()) _shaderList[i]->resizeGLObjectBuffers(maxSize);
    }

    _pcpList.resize(maxSize);
}

void Program::releaseGLObjects(osg::State* state) const
{
    for( unsigned int i=0; i < _shaderList.size(); ++i )
    {
        if (_shaderList[i].valid()) _shaderList[i]->releaseGLObjects(state);
    }

    if (!state) _pcpList.setAllElementsTo(0);
    else
    {
        unsigned int contextID = state->getContextID();
        _pcpList[contextID] = 0;
    }
}

bool Program::addShader( Shader* shader )
{
    if( !shader ) return false;

    // Shader can only be added once to a Program
    for( unsigned int i=0; i < _shaderList.size(); ++i )
    {
        if( shader == _shaderList[i].get() ) return false;
    }

    // Add shader to PCPs
    for( unsigned int cxt=0; cxt < _pcpList.size(); ++cxt )
    {
        if( _pcpList[cxt].valid() ) _pcpList[cxt]->addShaderToAttach( shader );
    }

    shader->addProgramRef( this );
    _shaderList.push_back( shader );
    dirtyProgram();
    return true;
}


bool Program::removeShader( Shader* shader )
{
    if( !shader ) return false;

    // Shader must exist to be removed.
    for( ShaderList::iterator itr = _shaderList.begin();
         itr != _shaderList.end();
         ++itr)
    {
        if( shader == itr->get() )
        {
            // Remove shader from PCPs
            for( unsigned int cxt=0; cxt < _pcpList.size(); ++cxt )
            {
                if( _pcpList[cxt].valid() ) _pcpList[cxt]->addShaderToDetach( shader );
            }

            shader->removeProgramRef( this );
            _shaderList.erase(itr);

            dirtyProgram();
            return true;
        }
    }

    return false;
}


void Program::setParameter( GLenum pname, GLint value )
{
    switch( pname )
    {
        case GL_GEOMETRY_VERTICES_OUT:
        case GL_GEOMETRY_VERTICES_OUT_EXT:
            _geometryVerticesOut = value;
            dirtyProgram();
            break;
        case GL_GEOMETRY_INPUT_TYPE:
        case GL_GEOMETRY_INPUT_TYPE_EXT:
            _geometryInputType = value;
            dirtyProgram();    // needed?
            break;
        case GL_GEOMETRY_OUTPUT_TYPE:
        case GL_GEOMETRY_OUTPUT_TYPE_EXT:
            _geometryOutputType = value;
            //dirtyProgram();    // needed?
            break;
        case GL_PATCH_VERTICES:
            OSG_WARN << "Program::setParameter invalid param " << GL_PATCH_VERTICES << ", use osg::PatchParameter when setting GL_PATCH_VERTICES."<<std::endl;
            break;
        default:
            OSG_WARN << "Program::setParameter invalid param " << pname << std::endl;
            break;
    }
}

GLint Program::getParameter( GLenum pname ) const
{
    switch( pname )
    {
        case GL_GEOMETRY_VERTICES_OUT:
        case GL_GEOMETRY_VERTICES_OUT_EXT:
            return _geometryVerticesOut;
        case GL_GEOMETRY_INPUT_TYPE:
        case GL_GEOMETRY_INPUT_TYPE_EXT:
            return _geometryInputType;
        case GL_GEOMETRY_OUTPUT_TYPE:
        case GL_GEOMETRY_OUTPUT_TYPE_EXT:
            return _geometryOutputType;
    }
    OSG_WARN << "getParameter invalid param " << pname << std::endl;
    return 0;
}

void Program::addBindAttribLocation( const std::string& name, GLuint index )
{
    _attribBindingList[name] = index;
    dirtyProgram();
}

void Program::removeBindAttribLocation( const std::string& name )
{
    _attribBindingList.erase(name);
    dirtyProgram();
}

void Program::addBindFragDataLocation( const std::string& name, GLuint index )
{
    _fragDataBindingList[name] = index;
    dirtyProgram();
}

void Program::removeBindFragDataLocation( const std::string& name )
{
    _fragDataBindingList.erase(name);
    dirtyProgram();
}

void Program::addBindUniformBlock(const std::string& name, GLuint index)
{
    _uniformBlockBindingList[name] = index;
    dirtyProgram(); // XXX
}

void Program::removeBindUniformBlock(const std::string& name)
{
    _uniformBlockBindingList.erase(name);
    dirtyProgram(); // XXX
}



#include <iostream>
void Program::apply( osg::State& state ) const
{
    
}


Program::ProgramObjects::ProgramObjects(const osg::Program* program, unsigned int contextID):
    _contextID(contextID),
    _program(program)
{
}


Program::PerContextProgram* Program::ProgramObjects::getPCP(const std::string& defineStr) const
{
    for(PerContextPrograms::const_iterator itr = _perContextPrograms.begin();
        itr != _perContextPrograms.end();
        ++itr)
    {
        if ((*itr)->getDefineString()==defineStr)
        {
            // OSG_NOTICE<<"Returning PCP "<<itr->get()<<" DefineString = "<<(*itr)->getDefineString()<<std::endl;
            return itr->get();
        }
    }
    return 0;
}

Program::PerContextProgram* Program::ProgramObjects::createPerContextProgram(const std::string& defineStr)
{
    Program::PerContextProgram* pcp = new PerContextProgram( _program, _contextID );
    _perContextPrograms.push_back( pcp );
    pcp->setDefineString(defineStr);
    // OSG_NOTICE<<"Creating PCP "<<pcp<<" PCP DefineString = ["<<pcp->getDefineString()<<"]"<<std::endl;
    return pcp;
}

void Program::ProgramObjects::requestLink()
{
    for(PerContextPrograms::iterator itr = _perContextPrograms.begin();
        itr != _perContextPrograms.end();
        ++itr)
    {
        (*itr)->requestLink();
    }
}

void Program::ProgramObjects::addShaderToAttach(Shader* shader)
{
    for(PerContextPrograms::iterator itr = _perContextPrograms.begin();
        itr != _perContextPrograms.end();
        ++itr)
    {
        (*itr)->addShaderToAttach(shader);
    }
}

void Program::ProgramObjects::addShaderToDetach(Shader* shader)
{
    for(PerContextPrograms::iterator itr = _perContextPrograms.begin();
        itr != _perContextPrograms.end();
        ++itr)
    {
        (*itr)->addShaderToDetach(shader);
    }
}


bool Program::ProgramObjects::getGlProgramInfoLog(std::string& log) const
{
    bool result = false;
    for(PerContextPrograms::const_iterator itr = _perContextPrograms.begin();
        itr != _perContextPrograms.end();
        ++itr)
    {
        result = (*itr)->getInfoLog( log ) | result;
    }
    return result;
}

Program::PerContextProgram* Program::getPCP(State& state) const
{
    unsigned int contextID = state.getContextID();
    std::string defineStr;
    state.getDefineString(defineStr, getShaderPragmas());

    if( ! _pcpList[contextID].valid() )
    {
        _pcpList[contextID] = new ProgramObjects( this, contextID );
    }

    Program::PerContextProgram* pcp = _pcpList[contextID]->getPCP(defineStr);
    if (pcp) return pcp;

    pcp = _pcpList[contextID]->createPerContextProgram(defineStr);

    // attach all PCSs to this new PCP
    for( unsigned int i=0; i < _shaderList.size(); ++i )
    {
        pcp->addShaderToAttach( _shaderList[i].get() );
    }

    return pcp;
}


bool Program::isFixedFunction() const
{
#ifdef OSG_GL_FIXED_FUNCTION_AVAILABLE
    // A Program object having no attached Shaders is a special case:
    // it indicates that programmable shading is to be disabled,
    // and thus use GL 1.x "fixed functionality" rendering.
    return _shaderList.empty();
#else
    return false;
#endif
}


bool Program::getGlProgramInfoLog(unsigned int contextID, std::string& log) const
{
    if (contextID<_pcpList.size()) return (_pcpList[ contextID ])->getGlProgramInfoLog( log );
    else return false;
}

#if 0
const Program::ActiveUniformMap& Program::getActiveUniforms(unsigned int contextID) const
{
    return getPCP( contextID )->getActiveUniforms();
}

const Program::ActiveVarInfoMap& Program::getActiveAttribs(unsigned int contextID) const
{
    return getPCP( contextID )->getActiveAttribs();
}

const Program::UniformBlockMap& Program::getUniformBlocks(unsigned contextID) const
{
    return getPCP( contextID )->getUniformBlocks();
}
#endif

///////////////////////////////////////////////////////////////////////////
// osg::Program::PerContextProgram
// PCP is an OSG abstraction of the per-context glProgram
///////////////////////////////////////////////////////////////////////////

Program::PerContextProgram::PerContextProgram(const Program* program, unsigned int contextID, GLuint programHandle ) :
        osg::Referenced(),
        _glProgramHandle(programHandle),
        _loadedBinary(false),
        _contextID( contextID ),
        _ownsProgramHandle(false)
{
    _program = program;
    if (_glProgramHandle == 0)
    {
        _extensions = GLExtensions::Get( _contextID, true );
        _glProgramHandle = _extensions->glCreateProgram();

        if (_glProgramHandle)
        {
            _ownsProgramHandle = true;
        }
        else
        {
            OSG_WARN << "Unable to create osg::Program \"" << _program->getName() << "\"" << " contextID=" << _contextID <<  std::endl;
        }
    }
    requestLink();
}

Program::PerContextProgram::~PerContextProgram()
{
    if (_ownsProgramHandle)
    {
        osg::get<GLProgramManager>(_contextID)->scheduleGLObjectForDeletion(_glProgramHandle);
    }
}


void Program::PerContextProgram::requestLink()
{
    _needsLink = true;
    _isLinked = false;
}


void Program::PerContextProgram::linkProgram(osg::State& state)
{

}

bool Program::PerContextProgram::validateProgram()
{
    return false;
}

bool Program::PerContextProgram::getInfoLog( std::string& infoLog ) const
{
     return false;
}

Program::ProgramBinary* Program::PerContextProgram::compileProgramBinary(osg::State& state)
{
    return 0;
}

void Program::PerContextProgram::useProgram() const
{
}
