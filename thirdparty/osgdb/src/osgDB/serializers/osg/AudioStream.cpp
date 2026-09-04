#include <osg/AudioStream>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( AudioStream,
                         /*new osg::AudioStream*/NULL,
                         osg::AudioStream,
                         "osg::Object osg::AudioStream" )
{
}
