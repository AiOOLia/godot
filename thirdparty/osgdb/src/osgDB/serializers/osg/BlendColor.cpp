#include <osg/BlendColor>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( BlendColor,
                         new osg::BlendColor,
                         osg::BlendColor,
                         "osg::Object osg::StateAttribute osg::BlendColor" )
{
    ADD_VEC4_SERIALIZER( ConstantColor, osg::Vec4() );  // _constantColor
}
