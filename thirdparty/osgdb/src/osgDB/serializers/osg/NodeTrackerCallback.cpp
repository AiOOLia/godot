#undef OBJECT_CAST
#define OBJECT_CAST dynamic_cast

#include <osg/NodeTrackerCallback>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( NodeTrackerCallback,
                         new osg::NodeTrackerCallback,
                         osg::NodeTrackerCallback,
                         "osg::Object osg::NodeCallback osg::NodeTrackerCallback" )
{
    ADD_OBJECT_SERIALIZER( TrackNode, osg::Node, NULL );  // _trackNodePath
}

#undef OBJECT_CAST
#define OBJECT_CAST static_cast
