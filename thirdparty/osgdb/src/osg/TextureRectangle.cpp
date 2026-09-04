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
#include <osg/TextureRectangle>
#include <osg/State>
#include <osg/GLU>
#include <osg/Notify>

#include <osg/Timer>

#ifndef GL_UNPACK_CLIENT_STORAGE_APPLE
#define GL_UNPACK_CLIENT_STORAGE_APPLE    0x85B2
#endif

#ifndef GL_APPLE_vertex_array_range
#define GL_VERTEX_ARRAY_RANGE_APPLE       0x851D
#define GL_VERTEX_ARRAY_RANGE_LENGTH_APPLE 0x851E
#define GL_VERTEX_ARRAY_STORAGE_HINT_APPLE 0x851F
#define GL_VERTEX_ARRAY_RANGE_POINTER_APPLE 0x8521
#define GL_STORAGE_CACHED_APPLE           0x85BE
#define GL_STORAGE_SHARED_APPLE           0x85BF
#endif

// #define DO_TIMING

using namespace osg;

TextureRectangle::TextureRectangle():
    _textureWidth(0),
    _textureHeight(0)
{
    setWrap(WRAP_S, CLAMP);
    setWrap(WRAP_T, CLAMP);

    setFilter(MIN_FILTER, LINEAR);
    setFilter(MAG_FILTER, LINEAR);
}

TextureRectangle::TextureRectangle(Image* image):
            _textureWidth(0),
            _textureHeight(0)
{
    setWrap(WRAP_S, CLAMP);
    setWrap(WRAP_T, CLAMP);

    setFilter(MIN_FILTER, LINEAR);
    setFilter(MAG_FILTER, LINEAR);

    setImage(image);
}

TextureRectangle::TextureRectangle(const TextureRectangle& text,const CopyOp& copyop):
    Texture(text,copyop),
    _textureWidth(text._textureWidth),
    _textureHeight(text._textureHeight),
    _subloadCallback(text._subloadCallback)
{
    setImage(copyop(text._image.get()));
}

TextureRectangle::~TextureRectangle()
{
    setImage(NULL);
}

int TextureRectangle::compare(const StateAttribute& sa) const
{
    // check the types are equal and then create the rhs variable
    // used by the COMPARE_StateAttribute_Parameter macros below.
    COMPARE_StateAttribute_Types(TextureRectangle,sa)

    if (_image!=rhs._image) // smart pointer comparison.
    {
        if (_image.valid())
        {
            if (rhs._image.valid())
            {
                int result = _image->compare(*rhs._image);
                if (result!=0) return result;
            }
            else
            {
                return 1; // valid lhs._image is greater than null.
            }
        }
        else if (rhs._image.valid())
        {
            return -1; // valid rhs._image is greater than null.
        }
    }

    if (!_image && !rhs._image)
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

void TextureRectangle::setImage(Image* image)
{
    if (_image == image) return;

    if (_image.valid())
    {
        _image->removeClient(this);

        if (_image->requiresUpdateCall())
        {
            setUpdateCallback(0);
            setDataVariance(osg::Object::STATIC);
        }
    }

    // delete old texture objects.
    dirtyTextureObject();

    _image = image;

    if (_image.valid())
    {
        _image->addClient(this);

        if (_image->requiresUpdateCall())
        {
            setUpdateCallback(new Image::UpdateCallback());
            setDataVariance(osg::Object::DYNAMIC);
        }
    }
}


void TextureRectangle::apply(State& state) const
{
    
}

void TextureRectangle::applyTexImage_load(GLenum target, Image* image, State& state, GLsizei& inwidth, GLsizei& inheight) const
{
    
}

void TextureRectangle::applyTexImage_subload(GLenum target, Image* image, State& state, GLsizei& inwidth, GLsizei& inheight, GLint& inInternalFormat) const
{
    
}

void TextureRectangle::computeInternalFormat() const
{
    if (_image.valid()) computeInternalFormatWithImage(*_image);
    else computeInternalFormatType();
}

void TextureRectangle::copyTexImage2D(State& state, int x, int y, int width, int height )
{
   
}

void TextureRectangle::copyTexSubImage2D(State& state, int xoffset, int yoffset, int x, int y, int width, int height )
{
    
}

void TextureRectangle::allocateMipmap(State&) const
{
    OSG_NOTICE<<"Warning: TextureRectangle::allocateMipmap(State&) called eroneously, GL_TEXTURE_RECTANGLE does not support mipmapping."<<std::endl;
}

