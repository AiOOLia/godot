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
#include <osg/PrimitiveSet>
#include <osg/BufferObject>
#include <osg/State>
#include <osg/Notify>

using namespace osg;

#define VOA_NOTICE OSG_INFO
//#define VOA_NOTICE OSG_NOTICE

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// PrimitiveSet
//
unsigned int PrimitiveSet::getNumPrimitives() const
{
    switch(_mode)
    {
        case(POINTS): return getNumIndices();
        case(LINES): return getNumIndices()/2;
        case(TRIANGLES): return getNumIndices()/3;
        case(QUADS): return getNumIndices()/4;
        case(LINE_STRIP):
        case(LINE_LOOP):
        case(TRIANGLE_STRIP):
        case(TRIANGLE_FAN):
        case(QUAD_STRIP):
        case(PATCHES):
        case(POLYGON): return (getNumIndices()>0) ? 1 : 0;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DrawArray
//
void DrawArrays::draw(State& state, bool) const
{

}

void DrawArrays::accept(PrimitiveFunctor& functor) const
{
    functor.drawArrays(_mode,_first,_count);
}

void DrawArrays::accept(PrimitiveIndexFunctor& functor) const
{
    functor.drawArrays(_mode,_first,_count);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DrawArrayLengths
//
unsigned int DrawArrayLengths::getNumPrimitives() const
{
    switch(_mode)
    {
        case(POINTS): return getNumIndices();
        case(LINES): return getNumIndices()/2;
        case(TRIANGLES): return getNumIndices()/3;
        case(QUADS): return getNumIndices()/4;
        case(LINE_STRIP):
        case(LINE_LOOP):
        case(TRIANGLE_STRIP):
        case(TRIANGLE_FAN):
        case(QUAD_STRIP):
        case(PATCHES):
        case(POLYGON): return size();
    }
    return 0;
}

void DrawArrayLengths::draw(State& state, bool) const
{
}

void DrawArrayLengths::accept(PrimitiveFunctor& functor) const
{
    GLint first = _first;
    for(vector_type::const_iterator itr=begin();
        itr!=end();
        ++itr)
    {
        functor.drawArrays(_mode,first,*itr);
        first += *itr;
    }
}

void DrawArrayLengths::accept(PrimitiveIndexFunctor& functor) const
{
    GLint first = _first;
    for(vector_type::const_iterator itr=begin();
        itr!=end();
        ++itr)
    {
        functor.drawArrays(_mode,first,*itr);
        first += *itr;
    }
}

unsigned int DrawArrayLengths::getNumIndices() const
{
    unsigned int count = 0;
    for(vector_type::const_iterator itr=begin();
        itr!=end();
        ++itr)
    {
        count += *itr;
    }
    return count;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DrawElementsUByte
//
DrawElementsUByte::~DrawElementsUByte()
{
    releaseGLObjects();
}

void DrawElementsUByte::draw(State& state, bool useVertexBufferObjects) const
{
}

void DrawElementsUByte::accept(PrimitiveFunctor& functor) const
{
    if (!empty()) functor.drawElements(_mode,size(),&front());
}

void DrawElementsUByte::accept(PrimitiveIndexFunctor& functor) const
{
    if (!empty()) functor.drawElements(_mode,size(),&front());
}

void DrawElementsUByte::offsetIndices(int offset)
{
    for(iterator itr=begin();
        itr!=end();
        ++itr)
    {
        *itr += offset;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DrawElementsUShort
//
DrawElementsUShort::~DrawElementsUShort()
{
    releaseGLObjects();
}

void DrawElementsUShort::draw(State& state, bool useVertexBufferObjects) const
{
}

void DrawElementsUShort::accept(PrimitiveFunctor& functor) const
{
    if (!empty()) functor.drawElements(_mode,size(),&front());
}

void DrawElementsUShort::accept(PrimitiveIndexFunctor& functor) const
{
    if (!empty()) functor.drawElements(_mode,size(),&front());
}

void DrawElementsUShort::offsetIndices(int offset)
{
    for(iterator itr=begin();
        itr!=end();
        ++itr)
    {
        *itr += offset;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DrawElementsUInt
//
DrawElementsUInt::~DrawElementsUInt()
{
    releaseGLObjects();
}

void DrawElementsUInt::draw(State& state, bool useVertexBufferObjects) const
{
}

void DrawElementsUInt::accept(PrimitiveFunctor& functor) const
{
    if (!empty()) functor.drawElements(_mode,size(),&front());
}

void DrawElementsUInt::accept(PrimitiveIndexFunctor& functor) const
{
    if (!empty()) functor.drawElements(_mode,size(),&front());
}

void DrawElementsUInt::offsetIndices(int offset)
{
    for(iterator itr=begin();
        itr!=end();
        ++itr)
    {
        *itr += offset;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MultiDrawArrays
//
#ifdef OSG_HAS_MULTIDRAWARRAYS
void MultiDrawArrays::draw(osg::State& state, bool) const
{
}

void MultiDrawArrays::accept(PrimitiveFunctor& functor) const
{
    unsigned int primcount = osg::minimum(_firsts.size(), _counts.size());
    for(unsigned int i=0; i<primcount; ++i)
    {
        functor.drawArrays(_mode, _firsts[i], _counts[i]);
    }
}

void MultiDrawArrays::accept(PrimitiveIndexFunctor& functor) const
{
    unsigned int primcount = osg::minimum(_firsts.size(), _counts.size());
    for(unsigned int i=0; i<primcount; ++i)
    {
        functor.drawArrays(_mode, _firsts[i], _counts[i]);
    }
}

unsigned int MultiDrawArrays::getNumIndices() const
{
    unsigned int total=0;
    for(Counts::const_iterator itr = _counts.begin(); itr!=_counts.end(); ++itr)
    {
        total += *itr;
    }
    return total;
}

unsigned int MultiDrawArrays::index(unsigned int pos) const
{
    unsigned int i;
    for(i=0; i<_counts.size(); ++i)
    {
        unsigned int count = _counts[i];
        if (pos<count) break;
        pos -= count;
    }
    if (i>=_firsts.size()) return 0;

    return _firsts[i] + pos;
}

void MultiDrawArrays::offsetIndices(int offset)
{
    for(Firsts::iterator itr = _firsts.begin(); itr!=_firsts.end(); ++itr)
    {
        *itr += offset;
    }
}

unsigned int MultiDrawArrays::getNumPrimitives() const
{
    switch(_mode)
    {
        case(POINTS): return getNumIndices();
        case(LINES): return getNumIndices()/2;
        case(TRIANGLES): return getNumIndices()/3;
        case(QUADS): return getNumIndices()/4;
        case(LINE_STRIP):
        case(LINE_LOOP):
        case(TRIANGLE_STRIP):
        case(TRIANGLE_FAN):
        case(QUAD_STRIP):
        case(PATCHES):
        case(POLYGON):
        {
            unsigned int primcount = osg::minimum(_firsts.size(), _counts.size());
            return primcount;
        }
    }
    return 0;
}

void MultiDrawArrays::add(GLint first, GLsizei count)
{
    _firsts.push_back(first);
    _counts.push_back(count);
}
#endif
