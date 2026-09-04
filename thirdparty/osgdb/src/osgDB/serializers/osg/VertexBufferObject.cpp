#include <osg/BufferObject>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( VertexBufferObject,
                         new osg::VertexBufferObject,
                         osg::VertexBufferObject,
                         "osg::Object osg::BufferObject osg::VertexBufferObject" )
{
}
