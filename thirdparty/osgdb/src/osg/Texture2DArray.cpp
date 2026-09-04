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
#include <osg/Texture2DArray>
#include <osg/State>
#include <osg/Notify>

#include <string.h>

//#define DO_TIMING

using namespace osg;

Texture2DArray::Texture2DArray():
            _textureWidth(0),
            _textureHeight(0),
            _textureDepth(0),
            _numMipmapLevels(0)
{
}

Texture2DArray::Texture2DArray(const Texture2DArray& text,const CopyOp& copyop):
            Texture(text,copyop),
            _textureWidth(text._textureWidth),
            _textureHeight(text._textureHeight),
            _textureDepth(0),
            _numMipmapLevels(text._numMipmapLevels),
            _subloadCallback(text._subloadCallback)
{
    setTextureDepth(text._textureDepth);

    for(unsigned int i = 0; i<static_cast<unsigned int>(_images.size()); ++i)
    {
        setImage(i, copyop(text._images[i].get()));
    }
}

Texture2DArray::~Texture2DArray()
{
    for(unsigned int i = 0; i<static_cast<unsigned int>(_images.size()); ++i)
    {
        setImage(i, NULL);
    }
}

int Texture2DArray::compare(const StateAttribute& sa) const
{
    // check the types are equal and then create the rhs variable
    // used by the COMPARE_StateAttribute_Parameter macros below.
    COMPARE_StateAttribute_Types(Texture2DArray,sa)

    if (_images.size()<rhs._images.size()) return -1;
    if (_images.size()>rhs._images.size()) return 1;

    bool noImages = true;
    for (unsigned int n=0; n < static_cast<unsigned int>(_images.size()); n++)
    {
        if (noImages && _images[n].valid()) noImages = false;
        if (noImages && rhs._images[n].valid()) noImages = false;

        if (_images[n]!=rhs._images[n]) // smart pointer comparison.
        {
            if (_images[n].valid())
            {
                if (rhs._images[n].valid())
                {
                    int result = _images[n]->compare(*rhs._images[n]);
                    if (result!=0) return result;
                }
                else
                {
                    return 1; // valid lhs._image is greater than null.
                }
            }
            else if (rhs._images[n].valid())
            {
                return -1; // valid rhs._image is greater than null.
            }
        }
    }

    if (noImages)
    {
        int result = compareTextureObjects(rhs);
        if (result!=0) return result;
    }

    int result = compareTexture(rhs);
    if (result!=0) return result;

    // compare each parameter in turn against the rhs.
    COMPARE_StateAttribute_Parameter(_textureWidth)
    COMPARE_StateAttribute_Parameter(_textureHeight)
    COMPARE_StateAttribute_Parameter(_textureDepth)
    COMPARE_StateAttribute_Parameter(_subloadCallback)

    return 0; // passed all the above comparison macros, must be equal.
}

void Texture2DArray::setImage(unsigned int layer, Image* image)
{
    if (layer>= static_cast<unsigned int>(_images.size()))
    {
        // _images vector not large enough to contain layer so expand it.
        _images.resize(layer+1);
        _modifiedCount.resize(layer+1);
    }
    else
    {
        // do not need to replace already assigned images
        if (_images[layer] == image) return;
    }

    unsigned numImageRequireUpdateBefore = 0;
    for (unsigned int i=0; i<getNumImages(); ++i)
    {
        if (_images[i].valid() && _images[i]->requiresUpdateCall()) ++numImageRequireUpdateBefore;
    }

    if (_images[layer].valid())
    {
        _images[layer]->removeClient(this);
    }

    // set image
    _images[layer] = image;
    _modifiedCount[layer].setAllElementsTo(0);

    if (_images[layer].valid())
    {
        _images[layer]->addClient(this);
    }

    // find out if we need to reset the update callback to handle the animation of image
    unsigned numImageRequireUpdateAfter = 0;
    for (unsigned int i=0; i<getNumImages(); ++i)
    {
        if (_images[i].valid() && _images[i]->requiresUpdateCall()) ++numImageRequireUpdateAfter;
    }

    if (numImageRequireUpdateBefore>0)
    {
        if (numImageRequireUpdateAfter==0)
        {
            setUpdateCallback(0);
            setDataVariance(osg::Object::STATIC);
        }
    }
    else if (numImageRequireUpdateAfter>0)
    {
        setUpdateCallback(new Image::UpdateCallback());
        setDataVariance(osg::Object::DYNAMIC);
    }
}

Image* Texture2DArray::getImage(unsigned int layer)
{
    return (layer<static_cast<unsigned int>(_images.size())) ? _images[layer].get() : 0;
}

const Image* Texture2DArray::getImage(unsigned int layer) const
{
    return (layer<static_cast<unsigned int>(_images.size())) ? _images[layer].get() : 0;
}

bool Texture2DArray::imagesValid() const
{
    if (_images.empty()) return false;

    for(Images::const_iterator itr = _images.begin();
        itr != _images.end();
        ++itr)
    {
        if (!(itr->valid()) || !((*itr)->valid())) return false;
    }
    return true;
}

void Texture2DArray::setTextureSize(int width, int height, int depth)
{
    // set dimensions
    _textureWidth = width;
    _textureHeight = height;
    setTextureDepth(depth);
}

void Texture2DArray::setTextureDepth(int depth)
{
    // if we decrease the number of layers, then delete non-used
    if (depth < static_cast<int>(_images.size()))
    {
        _images.resize(depth);
        _modifiedCount.resize(depth);
    }

    // resize the texture array
    _textureDepth = depth;
}

void Texture2DArray::computeInternalFormat() const
{
    if (imagesValid()) computeInternalFormatWithImage(*_images[0]);
    else computeInternalFormatType();
}

GLsizei Texture2DArray::computeTextureDepth() const
{
    GLsizei textureDepth = _textureDepth;
    if (textureDepth==0)
    {
        for(Images::const_iterator itr = _images.begin();
            itr != _images.end();
            ++itr)
        {
            const osg::Image* image = itr->get();
            if (image) textureDepth += image->r();
        }
    }
    return textureDepth;
}


void Texture2DArray::apply(State& state) const
{
    
}


void Texture2DArray::applyTexImage2DArray_subload(State& state, Image* image, GLsizei layer, GLsizei inwidth, GLsizei inheight, GLsizei indepth, GLint inInternalFormat, GLsizei& numMipmapLevels) const
{
   
}


void Texture2DArray::copyTexSubImage2DArray(State& state, int xoffset, int yoffset, int zoffset, int x, int y, int width, int height )
{
    
}

void Texture2DArray::allocateMipmap(State& state) const
{
    
}
