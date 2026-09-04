#include <osg/Hint>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( Hint,
                         new osg::Hint,
                         osg::Hint,
                         "osg::Object osg::StateAttribute osg::Hint" )
{
    ADD_GLENUM_SERIALIZER( Target, GLenum, GL_NONE );  // _target
    ADD_GLENUM_SERIALIZER( Mode, GLenum, GL_DONT_CARE );  // _mode
}
