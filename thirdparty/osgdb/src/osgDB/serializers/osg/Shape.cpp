#include <osg/Shape>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( Shape,
                         /*new osg::Shape*/NULL,
                         osg::Shape,
                         "osg::Object osg::Shape" )
{
}
