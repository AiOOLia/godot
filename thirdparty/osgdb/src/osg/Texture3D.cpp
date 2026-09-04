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
#include <osg/Texture3D>
#include <osg/State>
#include <osg/GLU>
#include <osg/Notify>

#include <string.h>



using namespace osg;

Texture3D::Texture3D():
            _textureWidth(0),
            _textureHeight(0),
            _textureDepth(0),
            _numMipmapLevels(0)
{
}


Texture3D::Texture3D(Image* image):
            _textureWidth(0),
            _textureHeight(0),
            _textureDepth(0),
            _numMipmapLevels(0)
{
    setImage(image);
}

Texture3D::Texture3D(const Texture3D& text,const CopyOp& copyop):
            Texture(text,copyop),
            _textureWidth(text._textureWidth),
            _textureHeight(text._textureHeight),
            _textureDepth(text._textureDepth),
            _numMipmapLevels(text._numMipmapLevels),
            _subloadCallback(text._subloadCallback)
{
    setImage(copyop(text._image.get()));
}

Texture3D::~Texture3D()
{
    setImage(NULL);
}

int Texture3D::compare(const StateAttribute& sa) const
{
    // check the types are equal and then create the rhs variable
    // used by the COMPARE_StateAttribute_Parameter macros below.
    COMPARE_StateAttribute_Types(Texture3D,sa)

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
    COMPARE_StateAttribute_Parameter(_textureDepth)
    COMPARE_StateAttribute_Parameter(_subloadCallback)

    return 0; // passed all the above comparison macros, must be equal.
}

void Texture3D::setImage(Image* image)
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

    _modifiedCount.setAllElementsTo(0);

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

void Texture3D::computeRequiredTextureDimensions(State& state, const osg::Image& image,GLsizei& inwidth, GLsizei& inheight,GLsizei& indepth, GLsizei& numMipmapLevels) const
{
    const GLExtensions* extensions = state.get<GLExtensions>();

    int width,height,depth;

    if( !_resizeNonPowerOfTwoHint && extensions->isNonPowerOfTwoTextureSupported(_min_filter) )
    {
        width = image.s();
        height = image.t();
        depth = image.r();
    }
    else
    {
        width = Image::computeNearestPowerOfTwo(image.s()-2*_borderWidth)+2*_borderWidth;
        height = Image::computeNearestPowerOfTwo(image.t()-2*_borderWidth)+2*_borderWidth;
        depth = Image::computeNearestPowerOfTwo(image.r()-2*_borderWidth)+2*_borderWidth;
    }

    // cap the size to what the graphics hardware can handle.
    if (width>extensions->maxTexture3DSize) width = extensions->maxTexture3DSize;
    if (height>extensions->maxTexture3DSize) height = extensions->maxTexture3DSize;
    if (depth>extensions->maxTexture3DSize) depth = extensions->maxTexture3DSize;

    inwidth = width;
    inheight = height;
    indepth = depth;

    bool useHardwareMipMapGeneration = !image.isMipmap() && _useHardwareMipMapGeneration && extensions->isGenerateMipMapSupported;

    if( _min_filter == LINEAR || _min_filter == NEAREST || useHardwareMipMapGeneration )
    {
        numMipmapLevels = 1;
    }
    else if( image.isMipmap() )
    {
        numMipmapLevels = image.getNumMipmapLevels();
    }
    else
    {
        numMipmapLevels = 0;
        for( ; (width || height || depth) ;++numMipmapLevels)
        {

            if (width == 0)
                width = 1;
            if (height == 0)
                height = 1;
            if (depth == 0)
                depth = 1;

            width >>= 1;
            height >>= 1;
            depth >>= 1;
        }
    }
}

void Texture3D::apply(State& state) const
{

   
}

void Texture3D::computeInternalFormat() const
{
    if (_image.valid()) computeInternalFormatWithImage(*_image);
    else computeInternalFormatType();
}

void Texture3D::applyTexImage3D(GLenum target, Image* image, State& state, GLsizei& inwidth, GLsizei& inheight, GLsizei& indepth, GLsizei& numMipmapLevels) const
{
    

}

void Texture3D::copyTexSubImage3D(State& state, int xoffset, int yoffset, int zoffset, int x, int y, int width, int height )
{
    
}

void Texture3D::allocateMipmap(State& state) const
{
    const unsigned int contextID = state.getContextID();

    // get the texture object for the current contextID.
    TextureObject* textureObject = getTextureObject(contextID);

    if (textureObject && _textureWidth != 0 && _textureHeight != 0 && _textureDepth != 0)
    {
        const GLExtensions* extensions = state.get<GLExtensions>();

        // bind texture
        textureObject->bind(state);

        // compute number of mipmap levels
        int width = _textureWidth;
        int height = _textureHeight;
        int depth = _textureDepth;
        int numMipmapLevels = Image::computeNumberOfMipmapLevels(width, height, depth);

        // we do not reallocate the level 0, since it was already allocated
        width >>= 1;
        height >>= 1;
        depth >>= 1;

        for( GLsizei k = 1; k < numMipmapLevels  && (width || height || depth); k++)
        {
            if (width == 0)
                width = 1;
            if (height == 0)
                height = 1;
            if (depth == 0)
                depth = 1;

            extensions->glTexImage3D( GL_TEXTURE_3D, k, _internalFormat,
                     width, height, depth, _borderWidth,
                     _sourceFormat ? _sourceFormat : _internalFormat,
                     _sourceType ? _sourceType : GL_UNSIGNED_BYTE, NULL);

            width >>= 1;
            height >>= 1;
            depth >>= 1;
        }

        // inform state that this texture is the current one bound.
        state.haveAppliedTextureAttribute(state.getActiveTextureUnit(), this);
    }
}
