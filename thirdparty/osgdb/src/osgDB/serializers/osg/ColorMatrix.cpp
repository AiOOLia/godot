#include <osg/ColorMatrix>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( ColorMatrix,
                         new osg::ColorMatrix,
                         osg::ColorMatrix,
                         "osg::Object osg::StateAttribute osg::ColorMatrix" )
{
    ADD_MATRIX_SERIALIZER( Matrix, osg::Matrix() );  // _matrix
}
