

 
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

#include <osg/ScriptEngine>

REGISTER_OBJECT_WRAPPER( Script,
                         new osg::Script,
                         osg::Script,
                         "osg::Object osg::Script" )
{
ADD_STRING_SERIALIZER( Script, "" );  // _script
ADD_STRING_SERIALIZER( Language, "" );  // _script
}



