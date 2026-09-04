#include <osg/TransferFunction>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( TransferFunction,
                         new osg::TransferFunction,
                         osg::TransferFunction,
                         "osg::Object osg::TransferFunction" )
{
}
