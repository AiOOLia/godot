#undef OBJECT_CAST
#define OBJECT_CAST dynamic_cast

#include <osg/Node>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER2(ComputeBoundingSphereCallback,
                         new osg::Node::ComputeBoundingSphereCallback,
                         osg::Node::ComputeBoundingSphereCallback,
                         "osg::ComputeBoundingSphereCallback",
                         "osg::Object osg::ComputeBoundingSphereCallback") {
}

#undef OBJECT_CAST
#define OBJECT_CAST static_cast
