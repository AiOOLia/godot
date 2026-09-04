#include <osg/TextureRectangle>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( TextureRectangle,
                         new osg::TextureRectangle,
                         osg::TextureRectangle,
                         "osg::Object osg::StateAttribute osg::Texture osg::TextureRectangle" )
{
    ADD_IMAGE_SERIALIZER( Image, osg::Image, NULL );  // _image
    ADD_INT_SERIALIZER( TextureWidth, 0 );  // _textureWidth
    ADD_INT_SERIALIZER( TextureHeight, 0 );  // _textureHeight
}
