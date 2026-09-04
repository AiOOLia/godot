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
#include <osg/Texture1D>
#include <osg/State>
#include <osg/GLU>

typedef void (GL_APIENTRY * MyCompressedTexImage1DArbProc) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);

using namespace osg;

Texture1D::Texture1D():
            _textureWidth(0),
            _numMipmapLevels(0)
{
}

Texture1D::Texture1D(osg::Image* image):
            _textureWidth(0),
            _numMipmapLevels(0)
{
    setImage(image);
}

Texture1D::Texture1D(const Texture1D& text,const CopyOp& copyop):
            Texture(text,copyop),
            _textureWidth(text._textureWidth),
            _numMipmapLevels(text._numMipmapLevels),
            _subloadCallback(text._subloadCallback)
{
    setImage(copyop(text._image.get()));
}

Texture1D::~Texture1D()
{
    setImage(NULL);
}

int Texture1D::compare(const StateAttribute& sa) const
{
    // check the types are equal and then create the rhs variable
    // used by the COMPARE_StateAttribute_Parameter macros below.
    COMPARE_StateAttribute_Types(Texture1D,sa)

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
    COMPARE_StateAttribute_Parameter(_subloadCallback)

    return 0; // passed all the above comparison macros, must be equal.
}

void Texture1D::setImage(Image* image)
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
    _modifiedCount.setAllElementsTo(0);

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


void Texture1D::apply(State& state) const
{

}

void Texture1D::computeInternalFormat() const
{
    if (_image.valid()) computeInternalFormatWithImage(*_image);
    else computeInternalFormatType();
}

void Texture1D::applyTexImage1D(GLenum target, Image* image, State& state, GLsizei& inwidth, GLsizei& numMipmapLevels) const
{

}

void Texture1D::copyTexImage1D(State& state, int x, int y, int width)
{

}

void Texture1D::copyTexSubImage1D(State& state, int xoffset, int x, int y, int width)
{

}

void Texture1D::allocateMipmap(State& state) const
{

}
