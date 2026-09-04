#include <osg/Shape>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( ConvexHull,
                         new osg::ConvexHull,
                         osg::ConvexHull,
                         "osg::Object osg::Shape osg::TriangleMesh osg::ConvexHull" )
{
}
