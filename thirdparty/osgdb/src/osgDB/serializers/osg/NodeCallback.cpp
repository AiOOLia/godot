#include <osg/Node>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( NodeCallback,
                         new osg::NodeCallback,
                         osg::NodeCallback,
                         "osg::Object osg::Callback osg::NodeCallback" )
{
}
