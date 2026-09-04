#include <osg/AudioStream>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( AudioSink,
                         /*new osg::AudioSink*/NULL,
                         osg::AudioSink,
                         "osg::Object osg::AudioSink" )
{
}
