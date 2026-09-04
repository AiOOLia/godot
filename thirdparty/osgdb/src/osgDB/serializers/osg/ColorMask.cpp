#include <osg/ColorMask>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( ColorMask,
                         new osg::ColorMask,
                         osg::ColorMask,
                         "osg::Object osg::StateAttribute osg::ColorMask" )
{
    ADD_BOOL_SERIALIZER( RedMask, true );  // _red
    ADD_BOOL_SERIALIZER( GreenMask, true );  // _green
    ADD_BOOL_SERIALIZER( BlueMask, true );  // _blue
    ADD_BOOL_SERIALIZER( AlphaMask, true );  // _alpha
}
