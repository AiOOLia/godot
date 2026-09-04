#include <osg/TexEnvFilter>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( TexEnvFilter,
                         new osg::TexEnvFilter,
                         osg::TexEnvFilter,
                         "osg::Object osg::StateAttribute osg::TexEnvFilter" )
{
    ADD_FLOAT_SERIALIZER( LodBias, 0.0f );  // _lodBias
}
