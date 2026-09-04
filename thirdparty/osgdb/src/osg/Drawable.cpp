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
#include <stdio.h>
#include <math.h>
#include <float.h>

#include <osg/Drawable>
#include <osg/State>
#include <osg/Notify>
#include <osg/Node>
#include <osg/GLExtensions>
#include <osg/Timer>
#include <osg/TriangleFunctor>
#include <osg/ContextData>
#include <osg/io_utils>

#include <algorithm>
#include <map>
#include <list>


using namespace osg;

static unsigned int s_minimumNumberOfDisplayListsToRetainInCache = 0;
void Drawable::setMinimumNumberOfDisplayListsToRetainInCache(unsigned int minimum)
{
    s_minimumNumberOfDisplayListsToRetainInCache = minimum;
}

unsigned int Drawable::getMinimumNumberOfDisplayListsToRetainInCache()
{
    return s_minimumNumberOfDisplayListsToRetainInCache;
}

class DisplayListManager : public GraphicsObjectManager
{
public:
    DisplayListManager(unsigned int contextID):
        GraphicsObjectManager("DisplayListManager", contextID),
        _numberDrawablesReusedLastInLastFrame(0),
        _numberNewDrawablesInLastFrame(0),
        _numberDeletedDrawablesInLastFrame(0)
    {
    }

    virtual void flushDeletedGLObjects(double, double& availableTime)
    {
    }

    virtual void flushAllDeletedGLObjects()
    {
    
    }

    virtual void deleteAllGLObjects()
    {
    }

    virtual void discardAllGLObjects()
    {
    }

    void deleteDisplayList(GLuint globj, unsigned int sizeHint)
    {
    }

    GLuint generateDisplayList(unsigned int sizeHint)
    {
        return 0;
    }

protected:

    int _numberDrawablesReusedLastInLastFrame;
    int _numberNewDrawablesInLastFrame;
    int _numberDeletedDrawablesInLastFrame;

    typedef std::multimap<unsigned int,GLuint> DisplayListMap;
    DisplayListMap _displayListMap;

};

GLuint Drawable::generateDisplayList(unsigned int contextID, unsigned int sizeHint)
{
    return osg::get<DisplayListManager>(contextID)->generateDisplayList(sizeHint);
}

void Drawable::deleteDisplayList(unsigned int contextID,GLuint globj, unsigned int sizeHint)
{
    osg::get<DisplayListManager>(contextID)->deleteDisplayList(globj, sizeHint);
}


Drawable::Drawable()
{
    _supportsDisplayList = false;
    _useDisplayList = false;

    _supportsVertexBufferObjects = true;
    _useVertexBufferObjects = true;

    _useVertexArrayObject = false;
}

Drawable::Drawable(const Drawable& drawable,const CopyOp& copyop):
    Node(drawable,copyop),
    _initialBoundingBox(drawable._initialBoundingBox),
    _computeBoundingBoxCallback(drawable._computeBoundingBoxCallback),
    _boundingBox(drawable._boundingBox),
    _shape(copyop(drawable._shape.get())),
    _supportsDisplayList(drawable._supportsDisplayList),
    _useDisplayList(drawable._useDisplayList),
    _supportsVertexBufferObjects(drawable._supportsVertexBufferObjects),
    _useVertexBufferObjects(drawable._useVertexBufferObjects),
    _useVertexArrayObject(drawable._useVertexArrayObject),
    _drawCallback(drawable._drawCallback),
    _createVertexArrayStateCallback(drawable._createVertexArrayStateCallback)
{
}

Drawable::~Drawable()
{
    // clean up display lists if assigned, for the display lists size  we can't use glGLObjectSizeHint() as it's a virtual function, so have to default to a 0 size hint.
    #ifdef OSG_GL_DISPLAYLISTS_AVAILABLE
    for(unsigned int i=0;i<_globjList.size();++i)
    {
        if (_globjList[i] != 0)
        {
            Drawable::deleteDisplayList(i,_globjList[i], 0); // we don't know getGLObjectSizeHint()
            _globjList[i] = 0;
        }
    }
    #endif

    // clean up VertexArrayState
    for(unsigned int i=0; i<_vertexArrayStateList.size(); ++i)
    {
        VertexArrayState* vas = _vertexArrayStateList[i].get();
        if (vas)
        {
            vas->release();
            _vertexArrayStateList[i] = 0;
        }
    }
}

osg::MatrixList Drawable::getWorldMatrices(const osg::Node* haltTraversalAtNode) const
{
    osg::MatrixList matrices;
    for(ParentList::const_iterator itr = _parents.begin();
        itr != _parents.end();
        ++itr)
    {
        osg::MatrixList localMatrices = (*itr)->getWorldMatrices(haltTraversalAtNode);
        matrices.insert(matrices.end(), localMatrices.begin(), localMatrices.end());
    }
    return matrices;
}

void Drawable::computeDataVariance()
{
    if (getDataVariance() != UNSPECIFIED) return;

    bool dynamic = false;

    if (getUpdateCallback() ||
        getEventCallback() ||
        getCullCallback())
    {
        dynamic = true;
    }

    setDataVariance(dynamic ? DYNAMIC : STATIC);
}

void Drawable::setThreadSafeRefUnref(bool threadSafe)
{
    Object::setThreadSafeRefUnref(threadSafe);

    if (_stateset.valid()) _stateset->setThreadSafeRefUnref(threadSafe);
    if (_drawCallback.valid()) _drawCallback->setThreadSafeRefUnref(threadSafe);
}

void Drawable::resizeGLObjectBuffers(unsigned int maxSize)
{
    if (_stateset.valid()) _stateset->resizeGLObjectBuffers(maxSize);
    if (_drawCallback.valid()) _drawCallback->resizeGLObjectBuffers(maxSize);

    _globjList.resize(maxSize);

    _vertexArrayStateList.resize(maxSize);
}

void Drawable::releaseGLObjects(State* state) const
{
    if (_stateset.valid()) _stateset->releaseGLObjects(state);

    if (_drawCallback.valid()) _drawCallback->releaseGLObjects(state);

    if (state)
    {
        // get the contextID (user defined ID of 0 upwards) for the
        // current OpenGL context.
        unsigned int contextID = state->getContextID();

        VertexArrayState* vas = contextID <_vertexArrayStateList.size() ? _vertexArrayStateList[contextID].get() : 0;
        if (vas)
        {
            vas->release();
            _vertexArrayStateList[contextID] = 0;
        }
    }
    else
    {
        for(unsigned int i=0; i<_vertexArrayStateList.size(); ++i)
        {
            VertexArrayState* vas = _vertexArrayStateList[i].get();
            if (vas)
            {
                vas->release();
                _vertexArrayStateList[i] = 0;
            }
        }
    }
}

void Drawable::setSupportsDisplayList(bool flag)
{
    // if value unchanged simply return.
    if (_supportsDisplayList==flag) return;
    _supportsDisplayList=false;
}

void Drawable::setUseDisplayList(bool flag)
{
    // if value unchanged simply return.
    if (_useDisplayList==flag) return;
   _useDisplayList = false;
}


void Drawable::setUseVertexArrayObject(bool flag)
{
    _useVertexArrayObject = flag;
}



void Drawable::setUseVertexBufferObjects(bool flag)
{
    // _useVertexBufferObjects = true;

    // OSG_NOTICE<<"Drawable::setUseVertexBufferObjects("<<flag<<")"<<std::endl;

    // if value unchanged simply return.
    if (_useVertexBufferObjects==flag) return;

    // if was previously set to true, remove display list.
    if (_useVertexBufferObjects)
    {
        dirtyGLObjects();
    }

    _useVertexBufferObjects = flag;
}

void Drawable::dirtyGLObjects()
{
    for(unsigned int i=0; i<_vertexArrayStateList.size(); ++i)
    {
        VertexArrayState* vas = _vertexArrayStateList[i].get();
        if (vas) vas->dirty();
    }
}


struct ComputeBound : public PrimitiveFunctor
{
        ComputeBound()
        {
            _vertices2f = 0;
            _vertices3f = 0;
            _vertices4f = 0;
            _vertices2d = 0;
            _vertices3d = 0;
            _vertices4d = 0;
        }

        virtual void setVertexArray(unsigned int,const Vec2* vertices) { _vertices2f = vertices; }
        virtual void setVertexArray(unsigned int,const Vec3* vertices) { _vertices3f = vertices; }
        virtual void setVertexArray(unsigned int,const Vec4* vertices) { _vertices4f = vertices; }

        virtual void setVertexArray(unsigned int,const Vec2d* vertices) { _vertices2d  = vertices; }
        virtual void setVertexArray(unsigned int,const Vec3d* vertices) { _vertices3d  = vertices; }
        virtual void setVertexArray(unsigned int,const Vec4d* vertices) { _vertices4d = vertices; }

        template<typename T>
        void _drawArrays(T* vert, T* end)
        {
            for(;vert<end;++vert)
            {
                vertex(*vert);
            }
        }


        template<typename T, typename I>
        void _drawElements(T* vert, I* indices, I* end)
        {
            for(;indices<end;++indices)
            {
                vertex(vert[*indices]);
            }
        }

        virtual void drawArrays(GLenum,GLint first,GLsizei count)
        {
            if      (_vertices3f) _drawArrays(_vertices3f+first, _vertices3f+(first+count));
            else if (_vertices2f) _drawArrays(_vertices2f+first, _vertices2f+(first+count));
            else if (_vertices4f) _drawArrays(_vertices4f+first, _vertices4f+(first+count));
            else if (_vertices2d) _drawArrays(_vertices2d+first, _vertices2d+(first+count));
            else if (_vertices3d) _drawArrays(_vertices3d+first, _vertices3d+(first+count));
            else if (_vertices4d) _drawArrays(_vertices4d+first, _vertices4d+(first+count));
        }

        virtual void drawElements(GLenum,GLsizei count,const GLubyte* indices)
        {
            if (_vertices3f) _drawElements(_vertices3f, indices, indices + count);
            else if (_vertices2f) _drawElements(_vertices2f, indices, indices + count);
            else if (_vertices4f) _drawElements(_vertices4f, indices, indices + count);
            else if (_vertices2d) _drawElements(_vertices2d, indices, indices + count);
            else if (_vertices3d) _drawElements(_vertices3d, indices, indices + count);
            else if (_vertices4d) _drawElements(_vertices4d, indices, indices + count);
        }

        virtual void drawElements(GLenum,GLsizei count,const GLushort* indices)
        {
            if      (_vertices3f) _drawElements(_vertices3f, indices, indices + count);
            else if (_vertices2f) _drawElements(_vertices2f, indices, indices + count);
            else if (_vertices4f) _drawElements(_vertices4f, indices, indices + count);
            else if (_vertices2d) _drawElements(_vertices2d, indices, indices + count);
            else if (_vertices3d) _drawElements(_vertices3d, indices, indices + count);
            else if (_vertices4d) _drawElements(_vertices4d, indices, indices + count);
        }

        virtual void drawElements(GLenum,GLsizei count,const GLuint* indices)
        {
            if      (_vertices3f) _drawElements(_vertices3f, indices, indices + count);
            else if (_vertices2f) _drawElements(_vertices2f, indices, indices + count);
            else if (_vertices4f) _drawElements(_vertices4f, indices, indices + count);
            else if (_vertices2d) _drawElements(_vertices2d, indices, indices + count);
            else if (_vertices3d) _drawElements(_vertices3d, indices, indices + count);
            else if (_vertices4d) _drawElements(_vertices4d, indices, indices + count);
        }

        virtual void begin(GLenum) {}
        virtual void vertex(const Vec2& vert) { _bb.expandBy(osg::Vec3(vert[0],vert[1],0.0f)); }
        virtual void vertex(const Vec3& vert) { _bb.expandBy(vert); }
        virtual void vertex(const Vec4& vert) { if (vert[3]!=0.0f) _bb.expandBy(osg::Vec3(vert[0],vert[1],vert[2])/vert[3]); }
        virtual void vertex(const Vec2d& vert) { _bb.expandBy(osg::Vec3(vert[0],vert[1],0.0f)); }
        virtual void vertex(const Vec3d& vert) { _bb.expandBy(vert); }
        virtual void vertex(const Vec4d& vert) { if (vert[3]!=0.0f) _bb.expandBy(osg::Vec3(vert[0],vert[1],vert[2])/vert[3]); }
        virtual void vertex(float x,float y)  { _bb.expandBy(x,y,1.0f); }
        virtual void vertex(float x,float y,float z) { _bb.expandBy(x,y,z); }
        virtual void vertex(float x,float y,float z,float w) { if (w!=0.0f) _bb.expandBy(x/w,y/w,z/w); }
        virtual void vertex(double x,double y)  { _bb.expandBy(x,y,1.0f); }
        virtual void vertex(double x,double y,double z) { _bb.expandBy(x,y,z); }
        virtual void vertex(double x,double y,double z,double w) { if (w!=0.0f) _bb.expandBy(x/w,y/w,z/w); }
        virtual void end() {}

        const Vec2*     _vertices2f;
        const Vec3*     _vertices3f;
        const Vec4*     _vertices4f;
        const Vec2d*    _vertices2d;
        const Vec3d*    _vertices3d;
        const Vec4d*    _vertices4d;
        BoundingBox     _bb;
};

BoundingSphere Drawable::computeBound() const
{
    return BoundingSphere(getBoundingBox());
}

BoundingBox Drawable::computeBoundingBox() const
{
    ComputeBound cb;

    Drawable* non_const_this = const_cast<Drawable*>(this);
    non_const_this->accept(cb);

#if 0
    OSG_NOTICE<<"computeBound() "<<cb._bb.xMin()<<", "<<cb._bb.xMax()<<", "<<std::endl;
    OSG_NOTICE<<"               "<<cb._bb.yMin()<<", "<<cb._bb.yMax()<<", "<<std::endl;
    OSG_NOTICE<<"               "<<cb._bb.zMin()<<", "<<cb._bb.zMax()<<", "<<std::endl;
#endif

    return cb._bb;
}

void Drawable::setBound(const BoundingBox& bb) const
{
     _boundingBox = bb;
     _boundingSphere = computeBound();
     _boundingSphereComputed = true;
}


void Drawable::compileGLObjects(RenderInfo& renderInfo) const
{

}

VertexArrayState* Drawable::createVertexArrayStateImplementation(RenderInfo& renderInfo) const
{
    OSG_INFO<<"VertexArrayState* Drawable::createVertexArrayStateImplementation(RenderInfo& renderInfo) const "<<this<<std::endl;
    VertexArrayState* vos = new osg::VertexArrayState(renderInfo.getState());
    vos->assignAllDispatchers();
    return vos;
}
