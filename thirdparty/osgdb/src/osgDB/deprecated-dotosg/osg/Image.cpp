#include "osg/Image"

#include "osgDB/Registry.h"
#include "osgDB/Input.h"
#include "osgDB/Output.h"

using namespace osg;
using namespace osgDB;

// forward declare functions to use later.
bool Image_readLocalData(Object& obj, Input& fr);
bool Image_writeLocalData(const Object& obj, Output& fw);

// register the read and write functions with the osgDB::Registry.
REGISTER_DOTOSGWRAPPER(Image)
(
    new osg::Image,
    "Image",
    "Object Image",
    &Image_readLocalData,
    &Image_writeLocalData
);

bool Image_readLocalData(Object& /*obj*/, Input& /*fr*/)
{
    bool iteratorAdvanced = false;

    // Image& image = static_cast<Image&>(obj);

    // no current image reading code
    // as it is all handled by osg::Registry::readImage() via plugins.

    return iteratorAdvanced;
}


bool Image_writeLocalData(const Object& /*obj*/, Output& /*fw*/)
{
    // const Image& image = static_cast<const Image&>(obj);

    // no current image writing code here
    // as it is all handled by osg::Registry::writeImage() via plugins.

    return true;
}
