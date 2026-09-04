#include <osg/BufferObject>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( DrawIndirectBufferObject,
                         new osg::DrawIndirectBufferObject,
                         osg::DrawIndirectBufferObject,
                         "osg::Object osg::BufferObject osg::DrawIndirectBufferObject" )
{
}
