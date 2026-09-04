#include <osg/Node>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( UniformCallback,
                         new osg::UniformCallback,
                         osg::UniformCallback,
                         "osg::Object osg::Callback osg::UniformCallback" )
{
}
