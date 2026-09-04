/**************************************************************************/
/*  osg_godot_image_reader.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "osg_godot_image_reader.h"

#include "osg_data_source.h"

#include <osg/Image>
#include <osgDB/ReaderWriter.h>
#include <osgDB/Registry.h>

#include <cstring>

namespace {

class OsgGodotImageReader : public osgDB::ReaderWriter {
public:
	OsgGodotImageReader() {
		supportsExtension("jpg", "JPEG image decoded by Godot");
		supportsExtension("jpeg", "JPEG image decoded by Godot");
		supportsExtension("png", "PNG image decoded by Godot");
		supportsExtension("bmp", "BMP image decoded by Godot");
		supportsExtension("tga", "TGA image decoded by Godot");
		// The trimmed InputStream maps PNG/BMP/TGA inline files to this
		// compatibility extension before asking the Registry for a reader.
		supportsExtension("stb", "PNG/BMP/TGA compatibility image decoder");
	}

	virtual const char *className() const override { return "Godot in-memory image reader"; }

	virtual ReadResult readImage(char *p_buffer, int p_size, const Options *p_options = nullptr) const override {
		if (!p_buffer || p_size <= 0) {
			return ReadResult::ERROR_IN_READING_FILE;
		}
		const std::string ext = p_options ? p_options->getOptionString() : std::string();
		PackedByteArray bytes;
		bytes.resize(p_size);
		memcpy(bytes.ptrw(), p_buffer, p_size);
		Ref<Image> image;
		String error;
		if (OsgDataSource::decode_image_bytes(bytes, ext.c_str(), image, error) != OK) {
			return ReadResult(error.utf8().get_data());
		}
		const PackedByteArray rgba = image->get_data();
		unsigned char *copy = new unsigned char[rgba.size()];
		memcpy(copy, rgba.ptr(), rgba.size());
		osg::Image *osg_image = new osg::Image;
		osg_image->setImage(image->get_width(), image->get_height(), 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, copy, osg::Image::USE_NEW_DELETE, 1);
		osg_image->setOrigin(osg::Image::TOP_LEFT);
		return osg_image;
	}
};

} // namespace

void osgdb_register_godot_image_reader() {
	static bool registered = false;
	if (registered) {
		return;
	}
	registered = true;
	osgDB::Registry::instance()->addReaderWriter(new OsgGodotImageReader);
}
