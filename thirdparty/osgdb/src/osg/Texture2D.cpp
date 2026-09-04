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
#include <osg/Texture2D>
#include <osg/State>
#include <osg/ContextData>
#include <osg/Notify>

using namespace osg;

Texture2D::Texture2D():
            _textureWidth(0),
            _textureHeight(0),
            _numMipmapLevels(0)
{
    setUseHardwareMipMapGeneration(true);
}

Texture2D::Texture2D(Image* image):
            _textureWidth(0),
            _textureHeight(0),
            _numMipmapLevels(0)
{
    setUseHardwareMipMapGeneration(true);
    setImage(image);
}

Texture2D::Texture2D(const Texture2D& text,const CopyOp& copyop):
            Texture(text,copyop),
            _textureWidth(text._textureWidth),
            _textureHeight(text._textureHeight),
            _numMipmapLevels(text._numMipmapLevels),
            _subloadCallback(text._subloadCallback)
{
    setImage(copyop(text._image.get()));
}

Texture2D::~Texture2D()
{
    setImage(NULL);
}

int Texture2D::compare(const StateAttribute& sa) const
{
    // check the types are equal and then create the rhs variable
    // used by the COMPARE_StateAttribute_Parameter macros below.
    COMPARE_StateAttribute_Types(Texture2D,sa)

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
#if 1
    if (_textureWidth != 0 && rhs._textureWidth != 0)
    {
        COMPARE_StateAttribute_Parameter(_textureWidth)
    }
    if (_textureHeight != 0 && rhs._textureHeight != 0)
    {
        COMPARE_StateAttribute_Parameter(_textureHeight)
    }
#endif
    COMPARE_StateAttribute_Parameter(_subloadCallback)

    return 0; // passed all the above comparison macros, must be equal.
}

void Texture2D::setImage(Image* image)
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

bool Texture2D::textureObjectValid(State& state) const
{
    TextureObject* textureObject = getTextureObject(state.getContextID());
    if (!textureObject) return false;

    // return true if image isn't assigned as we won't be override the value.
    if (!_image) return true;

    // compute the internal texture format, this set the _internalFormat to an appropriate value.
    computeInternalFormat();

    GLsizei new_width, new_height, new_numMipmapLevels;

    // compute the dimensions of the texture.
    computeRequiredTextureDimensions(state, *_image, new_width, new_height, new_numMipmapLevels);

    return textureObject->match(GL_TEXTURE_2D, new_numMipmapLevels, _internalFormat, new_width, new_height, 1, _borderWidth);
}


void Texture2D::apply(State& state) const
{
    //state.setReportGLErrors(true);

    // get the contextID (user defined ID of 0 upwards) for the
    // current OpenGL context.
    const unsigned int contextID = state.getContextID();

    // get the texture object for the current contextID.
    TextureObject* textureObject = getTextureObject(contextID);
    if (textureObject)
    {
        bool textureObjectInvalidated = false;
        if (_subloadCallback.valid())
        {
            textureObjectInvalidated = !_subloadCallback->textureObjectValid(*this, state);
        }
        else if (_image.valid() && getModifiedCount(contextID) != _image->getModifiedCount())
        {
            textureObjectInvalidated = !textureObjectValid(state);
        }

        if (textureObjectInvalidated)
        {
            // OSG_NOTICE<<"Discarding TextureObject"<<std::endl;
            _textureObjectBuffer[contextID]->release();
            _textureObjectBuffer[contextID] = 0;
            textureObject = 0;
        }
    }

    if (textureObject)
    {
        textureObject->bind(state);

        if (_subloadCallback.valid())
        {
            applyTexParameters(GL_TEXTURE_2D,state);

            _subloadCallback->subload(*this,state);
        }
        else if (_image.valid() && getModifiedCount(contextID) != _image->getModifiedCount())
        {
            // update the modified tag to show that it is up to date.
            getModifiedCount(contextID) = _image->getModifiedCount();

            applyTexParameters(GL_TEXTURE_2D,state);

            applyTexImage2D_subload(state,GL_TEXTURE_2D,_image.get(),
                                    _textureWidth, _textureHeight, _internalFormat, _numMipmapLevels);
        }
        else if (_readPBuffer.valid())
        {
            _readPBuffer->bindPBufferToTexture(GL_FRONT);
        }

        if (getTextureParameterDirty(state.getContextID()))
            applyTexParameters(GL_TEXTURE_2D,state);

    }
    else if (_subloadCallback.valid())
    {
        _textureObjectBuffer[contextID] = _subloadCallback->generateTextureObject(*this, state);
        textureObject = _textureObjectBuffer[contextID].get();

        textureObject->bind(state);

        applyTexParameters(GL_TEXTURE_2D,state);

        if (_image.valid()) getModifiedCount(contextID) = _image->getModifiedCount();

        _subloadCallback->load(*this,state);

        textureObject->setAllocated(_numMipmapLevels,_internalFormat,_textureWidth,_textureHeight,1,_borderWidth);

        // in theory the following line is redundant, but in practice
        // have found that the first frame drawn doesn't apply the textures
        // unless a second bind is called?!!
        // perhaps it is the first glBind which is not required...
        //glBindTexture( GL_TEXTURE_2D, handle );

        // update the modified tag to show that it is up to date.
    }
    else if (_image.valid() && _image->data())
    {
        GLExtensions * extensions = state.get<GLExtensions>();

        // keep the image around at least till we go out of scope.
        osg::ref_ptr<osg::Image> image = _image;

        // compute the internal texture format, this set the _internalFormat to an appropriate value.
        computeInternalFormat();

        // compute the dimensions of the texture.
        computeRequiredTextureDimensions(state,*image,_textureWidth, _textureHeight, _numMipmapLevels);

        GLenum texStorageSizedInternalFormat = extensions->isTextureStorageEnabled && (_borderWidth==0) ? selectSizedInternalFormat(_image.get()) : 0;

        textureObject = generateAndAssignTextureObject(contextID,GL_TEXTURE_2D,_numMipmapLevels,
            texStorageSizedInternalFormat!=0 ? texStorageSizedInternalFormat : _internalFormat,
            _textureWidth,_textureHeight,1,_borderWidth);

        textureObject->bind(state);

        applyTexParameters(GL_TEXTURE_2D,state);

        // update the modified tag to show that it is up to date.
        getModifiedCount(contextID) = image->getModifiedCount();

        if (textureObject->isAllocated() && image->supportsTextureSubloading())
        {
            //OSG_NOTICE<<"Reusing texture object"<<std::endl;
            applyTexImage2D_subload(state,GL_TEXTURE_2D,image.get(),
                                 _textureWidth, _textureHeight, _internalFormat, _numMipmapLevels);
        }
        else
        {
            //OSG_NOTICE<<"Creating new texture object"<<std::endl;
            applyTexImage2D_load(state,GL_TEXTURE_2D,image.get(),
                                 _textureWidth, _textureHeight, _numMipmapLevels);

            textureObject->setAllocated(true);
        }

        // unref image data?
        if (isSafeToUnrefImageData(state) && image->getDataVariance()==STATIC)
        {
            Texture2D* non_const_this = const_cast<Texture2D*>(this);
            non_const_this->_image = NULL;
        }

        // in theory the following line is redundant, but in practice
        // have found that the first frame drawn doesn't apply the textures
        // unless a second bind is called?!!
        // perhaps it is the first glBind which is not required...
        //glBindTexture( GL_TEXTURE_2D, handle );

    }
    else if ( (_textureWidth!=0) && (_textureHeight!=0) && (_internalFormat!=0) )
    {
    }
    else
    {
    }

    // if texture object is now valid and we have to allocate mipmap levels, then
    if (textureObject != 0 && _texMipmapGenerationDirtyList[contextID])
    {
        generateMipmap(state);
    }
}

void Texture2D::computeInternalFormat() const
{
    if (_image.valid()) computeInternalFormatWithImage(*_image);
    else computeInternalFormatType();
}

void Texture2D::copyTexImage2D(State& state, int x, int y, int width, int height )
{
    
}

void Texture2D::copyTexSubImage2D(State& state, int xoffset, int yoffset, int x, int y, int width, int height )
{
    
}

void Texture2D::allocateMipmap(State& state) const
{
    
}
