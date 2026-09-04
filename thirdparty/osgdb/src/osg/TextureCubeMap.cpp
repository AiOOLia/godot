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
#include <osg/ref_ptr>
#include <osg/Image>
#include <osg/State>
#include <osg/TextureCubeMap>
#include <osg/Notify>

#include <osg/GLU>


using namespace osg;

static GLenum faceTarget[6] =
{
    GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};


TextureCubeMap::TextureCubeMap():
            _textureWidth(0),
            _textureHeight(0),
            _numMipmapLevels(0)
{
    setUseHardwareMipMapGeneration(false);
}

TextureCubeMap::TextureCubeMap(const TextureCubeMap& text,const CopyOp& copyop):
            Texture(text,copyop),
            _textureWidth(text._textureWidth),
            _textureHeight(text._textureHeight),
            _numMipmapLevels(text._numMipmapLevels),
            _subloadCallback(text._subloadCallback)
{
    setImage(0, copyop(text._images[0].get()));
    setImage(1, copyop(text._images[1].get()));
    setImage(2, copyop(text._images[2].get()));
    setImage(3, copyop(text._images[3].get()));
    setImage(4, copyop(text._images[4].get()));
    setImage(5, copyop(text._images[5].get()));
}

TextureCubeMap::~TextureCubeMap()
{
    setImage(0, NULL);
    setImage(1, NULL);
    setImage(2, NULL);
    setImage(3, NULL);
    setImage(4, NULL);
    setImage(5, NULL);
}

int TextureCubeMap::compare(const StateAttribute& sa) const
{
    // check the types are equal and then create the rhs variable
    // used by the COMPARE_StateAttribute_Parameter macros below.
    COMPARE_StateAttribute_Types(TextureCubeMap,sa)

    bool noImages = true;
    for (int n=0; n<6; n++)
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
        // no image attached to either Texture2D
        // but could these textures already be downloaded?
        // check the _textureObjectBuffer to see if they have been
        // downloaded

        int result = compareTextureObjects(rhs);
        if (result!=0) return result;
    }

    int result = compareTexture(rhs);
    if (result!=0) return result;

    // compare each parameter in turn against the rhs.
    COMPARE_StateAttribute_Parameter(_textureWidth)
    COMPARE_StateAttribute_Parameter(_textureHeight)
    COMPARE_StateAttribute_Parameter(_subloadCallback)

    return 0; // passed all the above comparison macros, must be equal.
}


void TextureCubeMap::setImage(unsigned int face, Image* image)
{
    if (_images[face] == image) return;

    unsigned numImageRequireUpdateBefore = 0;
    for (unsigned int i=0; i<getNumImages(); ++i)
    {
        if (_images[i].valid() && _images[i]->requiresUpdateCall()) ++numImageRequireUpdateBefore;
    }

    if (_images[face].valid())
    {
        _images[face]->removeClient(this);
    }

    _images[face] = image;
    _modifiedCount[face].setAllElementsTo(0);

    if (_images[face].valid())
    {
        _images[face]->addClient(this);
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

Image* TextureCubeMap::getImage(unsigned int face)
{
    return _images[face].get();
}

const Image* TextureCubeMap::getImage(unsigned int face) const
{
    return _images[face].get();
}

bool TextureCubeMap::imagesValid() const
{
    for (int n=0; n<6; n++)
    {
        if (!_images[n].valid() || !_images[n]->data())
            return false;
    }
    return true;
}

void TextureCubeMap::computeInternalFormat() const
{
    if (imagesValid()) computeInternalFormatWithImage(*_images[0]);
    else computeInternalFormatType();
}

void TextureCubeMap::apply(State& state) const
{
    
}

void TextureCubeMap::copyTexSubImageCubeMap(State& state, int face, int xoffset, int yoffset, int x, int y, int width, int height )
{
    
}

void TextureCubeMap::allocateMipmap(State& state) const
{
    
}
