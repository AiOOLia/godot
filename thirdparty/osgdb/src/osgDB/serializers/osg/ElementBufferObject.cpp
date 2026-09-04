#include <osg/BufferObject>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>


REGISTER_OBJECT_WRAPPER( ElementBufferObject,
                         new osg::ElementBufferObject,
                         osg::ElementBufferObject,
                         "osg::Object osg::BufferObject osg::ElementBufferObject" )
{
}
