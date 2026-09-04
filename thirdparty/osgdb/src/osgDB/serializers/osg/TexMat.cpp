#include <osg/TexMat>
#include <osgDB/ObjectWrapper.h>
#include <osgDB/InputStream.h>
#include <osgDB/OutputStream.h>

REGISTER_OBJECT_WRAPPER( TexMat,
                         new osg::TexMat,
                         osg::TexMat,
                         "osg::Object osg::StateAttribute osg::TexMat" )
{
    ADD_MATRIX_SERIALIZER( Matrix, osg::Matrix() );  // _matrix
    ADD_BOOL_SERIALIZER( ScaleByTextureRectangleSize, false );  // _scaleByTextureRectangleSize
}
