#include <osg/StateAttribute>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( StateAttribute,
                         /*new osg::StateAttribute*/NULL,
                         osg::StateAttribute,
                         "osg::Object osg::StateAttribute" )
{
    ADD_OBJECT_SERIALIZER( UpdateCallback, osg::StateAttributeCallback, NULL );  // _updateCallback
    ADD_OBJECT_SERIALIZER( EventCallback, osg::StateAttributeCallback, NULL );  // _eventCallback
}
