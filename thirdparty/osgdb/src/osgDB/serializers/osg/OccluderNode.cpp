#include <osg/OccluderNode>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( OccluderNode,
                         new osg::OccluderNode,
                         osg::OccluderNode,
                         "osg::Object osg::Node osg::Group osg::OccluderNode" )
{
    ADD_OBJECT_SERIALIZER( Occluder, osg::ConvexPlanarOccluder, NULL );  // _occluder
}
