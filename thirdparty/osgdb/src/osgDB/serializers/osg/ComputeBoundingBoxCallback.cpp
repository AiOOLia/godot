#undef OBJECT_CAST
#define OBJECT_CAST dynamic_cast

#include <osg/Drawable>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER2(ComputeBoundingBoxCallback,
                         new osg::Drawable::ComputeBoundingBoxCallback,
                         osg::Drawable::ComputeBoundingBoxCallback,
                         "osg::ComputeBoundingBoxCallback",
                         "osg::Object osg::ComputeBoundingBoxCallback") {
}

#undef OBJECT_CAST
#define OBJECT_CAST static_cast
