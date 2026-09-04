#include <osg/Image>
#include <osg/Notify>
#include <osg/Geode>
#include <osg/ImageUtils>
#include <osg/GL>

#include <osgDB/Registry.h>
#include <osgDB/FileNameUtils.h>
#include <osgDB/FileUtils.h>

#include <sstream>
#include "./exif.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../../../../../stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../../stb/stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../../../../../stb/stb_image_resize2.h"

#if defined(_MSC_VER) && defined(OSG_DISABLE_MSVC_WARNINGS)
    // disable "structure was padded due to __declspec(align())
    #pragma warning( disable : 4324 )
#endif

/****************************************************************************
 *
 * Follows is code extracted from the simage library.  Original Authors:
 *
 *      Systems in Motion,
 *      <URL:http://www.sim.no>
 *
 *      Peder Blekken <pederb@sim.no>
 *      Morten Eriksen <mortene@sim.no>
 *      Marius Bugge Monsen <mariusbu@sim.no>
 *
 * The original COPYING notice
 *
 *      All files in this library are public domain, except simage_rgb.cpp which is
 *      Copyright (c) Mark J Kilgard <mjk@nvidia.com>. I will contact Mark
 *      very soon to hear if this source also can become public domain.
 *
 *      Please send patches for bugs and new features to: <pederb@sim.no>.
 *
 *      Peder Blekken
 *
 *
 * Ported into the OSG as a plugin, Robert Osfield December 2000.
 * Note, reference above to license of simage_rgb is not relevant to the OSG
 * as the OSG does not use it.  Also for patches, bugs and new features
 * please send them direct to the OSG dev team rather than address above.
 *
 **********************************************************************/

/*
 * Based on example code found in the libjpeg archive
 *
 */

#include <setjmp.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#if defined(_MSC_VER) && defined(OSG_DISABLE_MSVC_WARNINGS)
    #pragma warning( disable : 4611 )
#endif

/* CODE FOR READING/WRITING JPEG FROM STREAMS
 *  This code was taken directly from jdatasrc.c and jdatadst.c (libjpeg source)
 *  and modified to use a std::istream/ostream* instead of a FILE*
 */

/* Expanded data source object for stdio input */

class ReaderWriterJPEG : public osgDB::ReaderWriter
{
        WriteResult::WriteStatus write_JPEG_file (std::ostream &fout, const osg::Image &img, int quality = 100) const
        {
            if (!img.isDataContiguous())
            {
                OSG_WARN<<"Warning: Writing of image data, that is non contiguous, is not supported by JPEG plugin."<<std::endl;
                return WriteResult::ERROR_IN_WRITING_FILE;
            }

            int image_width = img.s();
            int image_height = img.t();
            if ( (image_width == 0) || (image_height == 0) )
            {
                OSG_DEBUG << "ReaderWriterJPEG::write_JPEG_file - Error no size" << std::endl;
                return WriteResult::ERROR_IN_WRITING_FILE;
            }

            int image_components = 3;
            // Only cater for gray, alpha and RGB for now
            switch(img.getPixelFormat()) {
              case(GL_DEPTH_COMPONENT):
              case(GL_LUMINANCE):
              case(GL_ALPHA): {
                  image_components = 1;
                  break;
              }
              case(GL_RGB): {
                  image_components = 3;
                  break;
              }
              case(GL_RGBA): {
                  image_components = 4;
                  break;
              }
              default:
              {
                  OSG_DEBUG << "ReaderWriterJPEG::write_JPEG_file - Error pixel format non supported" << std::endl;
                  return WriteResult::ERROR_IN_WRITING_FILE;
              }
            }

            // JPG不支持alpha通道，如果输入是RGBA，只会使用RGB部分
            // quality参数范围是1-100，表示JPG压缩质量，100为最高质量
            if (stbi_write_jpg_to_func([](void* stream, void* data, int size)->void {
                    std::ostream* ostream = static_cast<std::ostream*>(stream);
                    ostream->write(static_cast<const char*>(data), size);
                }, &fout, image_width, image_height, image_components, img.data(), quality) != 0) {
                return WriteResult::FILE_SAVED;
            }
            WriteResult::ERROR_IN_WRITING_FILE;
        }
        int getQuality(const osgDB::ReaderWriter::Options *options) const {
            if(options) {
                std::istringstream iss(options->getOptionString());
                std::string opt;
                while (iss >> opt) {
                    if(opt=="JPEG_QUALITY") {
                        int quality;
                        iss >> quality;
                        return quality;
                    }
                }
            }

            return 100;
        }
    public:

        ReaderWriterJPEG()
        {
           supportsExtension("jpeg","JPEG image format");
           supportsExtension("jpg","JPEG image format");
        }

        virtual const char* className() const { return "JPEG Image Reader/Writer"; }

        virtual ReadResult readObject(std::istream& fin,const osgDB::ReaderWriter::Options* options =NULL) const
        {
            return readImage(fin, options);
        }

        virtual ReadResult readObject(const std::string& file, const osgDB::ReaderWriter::Options* options =NULL) const
        {
            return readImage(file, options);
        }

        virtual ReadResult readImage(std::istream& fin,const osgDB::ReaderWriter::Options* =NULL) const
        {
            return ReadResult();
        }

        virtual ReadResult readImage(char* buffer, int size, const osgDB::ReaderWriter::Options* = NULL) const
        {
            // 使用stb_image从内存加载
            int width, height, channels;
            auto res = stbi_info_from_memory((stbi_uc*)buffer,
                size,
                &width, &height, &channels
            );
            easyexif::EXIFInfo result;
            int code = result.parseFrom((unsigned char*)buffer, size);
            auto exif_orientation = result.Orientation;

            int out_channels = channels;

            if (channels == 3) {
                out_channels = 4; // 强制转换为RGBA
            }

            unsigned char* data = stbi_load_from_memory(
                (stbi_uc*)buffer,
                size,
                &width, &height, &channels,
                out_channels  
            );

            //int internalFormat = numComponents_ret;
            int internalFormat =
                out_channels == 1 ? GL_LUMINANCE :
                out_channels == 2 ? GL_LUMINANCE_ALPHA :
                out_channels == 4 ? GL_RGBA : (GLenum)-1;

            unsigned int pixelFormat =
                out_channels == 1 ? GL_LUMINANCE :
                out_channels == 2 ? GL_LUMINANCE_ALPHA :
                out_channels == 4 ? GL_RGBA : (GLenum)-1;

            unsigned int dataType = GL_UNSIGNED_BYTE;

            osg::ref_ptr<osg::Image> pOsgImage = new osg::Image;
            pOsgImage->setImage(width, height, 1,
                internalFormat,
                pixelFormat,
                dataType,
                data,
                osg::Image::USE_NEW_DELETE);

            if (exif_orientation > 0)
            {
                // guide for meaning of exif_orientation provided by webpage: http://sylvana.net/jpegcrop/exif_orientation.html
                switch (exif_orientation)
                {
                case(1):
                    OSG_INFO << "EXIF_Orientation 1 (top, left side), No need to rotate image. " << std::endl;
                    break;
                case(2):
                    OSG_INFO << "EXIF_Orientation 2 (top, right side), flip x." << std::endl;
                    pOsgImage = osg::createImageWithOrientationConversion(pOsgImage.get(),
                        osg::Vec3i(pOsgImage->s() - 1, 0, 0),
                        osg::Vec3i(-pOsgImage->s(), 0, 0),
                        osg::Vec3i(0, pOsgImage->t(), 0),
                        osg::Vec3i(0, 0, 1));
                    break;
                case(3):
                    OSG_INFO << "EXIF_Orientation 3 (bottom, right side), rotate 180." << std::endl;
                    pOsgImage = osg::createImageWithOrientationConversion(pOsgImage.get(),
                        osg::Vec3i(pOsgImage->s() - 1, pOsgImage->t() - 1, 0),
                        osg::Vec3i(-pOsgImage->s(), 0, 0),
                        osg::Vec3i(0, -pOsgImage->t(), 0),
                        osg::Vec3i(0, 0, 1));
                    break;
                case(4):
                    OSG_INFO << "EXIF_Orientation 4 (bottom, left side). flip y, rotate 180." << std::endl;
                    pOsgImage = osg::createImageWithOrientationConversion(pOsgImage.get(),
                        osg::Vec3i(0, pOsgImage->t() - 1, 0),
                        osg::Vec3i(pOsgImage->s(), 0, 0),
                        osg::Vec3i(0, -pOsgImage->t(), 0),
                        osg::Vec3i(0, 0, 1));
                    break;
                case(5):
                    OSG_INFO << "EXIF_Orientation 5 (left side, top). flip y, rotate 90." << std::endl;
                    pOsgImage = osg::createImageWithOrientationConversion(pOsgImage.get(),
                        osg::Vec3i(pOsgImage->s() - 1, pOsgImage->t() - 1, 0),
                        osg::Vec3i(0, -pOsgImage->t(), 0),
                        osg::Vec3i(-pOsgImage->s(), 0, 0),
                        osg::Vec3i(0, 0, 1));
                    break;
                case(6):
                    OSG_INFO << "EXIF_Orientation 6 (right side, top). rotate 90." << std::endl;
                    pOsgImage = osg::createImageWithOrientationConversion(pOsgImage.get(),
                        osg::Vec3i(pOsgImage->s() - 1, 0, 0),
                        osg::Vec3i(0, pOsgImage->t(), 0),
                        osg::Vec3i(-pOsgImage->s(), 0, 0),
                        osg::Vec3i(0, 0, 1));
                    break;
                case(7):
                    OSG_INFO << "EXIF_Orientation 7 (right side, bottom), flip Y, rotate 270." << std::endl;
                    pOsgImage = osg::createImageWithOrientationConversion(pOsgImage.get(),
                        osg::Vec3i(0, 0, 0),
                        osg::Vec3i(0, pOsgImage->t(), 0),
                        osg::Vec3i(pOsgImage->s(), 0, 0),
                        osg::Vec3i(0, 0, 1));
                    break;
                case(8):
                    OSG_INFO << "EXIF_Orientation 8 (left side, bottom). rotate 270." << std::endl;
                    pOsgImage = osg::createImageWithOrientationConversion(pOsgImage.get(),
                        osg::Vec3i(0, pOsgImage->t() - 1, 0),
                        osg::Vec3i(0, -pOsgImage->t(), 0),
                        osg::Vec3i(pOsgImage->s(), 0, 0),
                        osg::Vec3i(0, 0, 1));
                    break;
                }

            }

            return pOsgImage.release();
        }

        virtual ReadResult readImage(const std::string& file, const osgDB::ReaderWriter::Options* options) const
        {
            std::string ext = osgDB::getLowerCaseFileExtension(file);
            if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

            std::string fileName = osgDB::findDataFile( file, options );
            if (fileName.empty()) return ReadResult::FILE_NOT_FOUND;

            osgDB::ifstream istream(fileName.c_str(), std::ios::in | std::ios::binary);
            if(!istream) return ReadResult::ERROR_IN_READING_FILE;
            // 获取流大小
            istream.seekg(0, std::ios::end);
            size_t size = istream.tellg();
            istream.seekg(0, std::ios::beg);

            // 将数据读入内存缓冲区
            std::vector<unsigned char> buffer(size);
            if (!istream.read(reinterpret_cast<char*>(buffer.data()), size)) {
                return nullptr;
            }
            //
            return readImage((char*)buffer.data(), size, options);
        }

        virtual WriteResult writeImage(const osg::Image& img,std::ostream& fout,const osgDB::ReaderWriter::Options *options) const
        {
            osg::ref_ptr<osg::Image> tmp_img = new osg::Image(img);
            tmp_img->flipVertical();
            WriteResult::WriteStatus ws = write_JPEG_file(fout, *(tmp_img.get()), getQuality(options));
            return ws;
        }

        virtual WriteResult writeImage(const osg::Image &img,const std::string& fileName, const osgDB::ReaderWriter::Options *options) const
        {
            std::string ext = osgDB::getFileExtension(fileName);
            if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

            osgDB::ofstream fout(fileName.c_str(), std::ios::out | std::ios::binary);
            if(!fout) return WriteResult::ERROR_IN_WRITING_FILE;

            return writeImage(img,fout,options);

            fout.close();
        }
};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(jpeg, ReaderWriterJPEG)
