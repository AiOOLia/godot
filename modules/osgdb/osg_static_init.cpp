/**************************************************************************/
/*  osg_static_init.cpp                                                   */
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

#include "osg_static_init.h"

#include "osg_godot_image_reader.h"

#include <osgDB/Registry.h>

USE_OSGPLUGIN(osg)
USE_OSGPLUGIN(osg2)
USE_SERIALIZER_WRAPPER_LIBRARY(osg)
// Traditional .osg files use DotOsgWrapper, not the OSG2 serializers above.
USE_DOTOSGWRAPPER(Object)
USE_DOTOSGWRAPPER(Node)
USE_DOTOSGWRAPPER(Group)
USE_DOTOSGWRAPPER(Transform)
USE_DOTOSGWRAPPER(MatrixTransform)
USE_DOTOSGWRAPPER(Geode)
USE_DOTOSGWRAPPER(Drawable)
USE_DOTOSGWRAPPER(Geometry)
USE_DOTOSGWRAPPER(LOD)
USE_DOTOSGWRAPPER(PagedLOD)
USE_DOTOSGWRAPPER(ProxyNode)
USE_DOTOSGWRAPPER(StateAttribute)
USE_DOTOSGWRAPPER(StateSet)
USE_DOTOSGWRAPPER(Material)
USE_DOTOSGWRAPPER(Texture)
USE_DOTOSGWRAPPER(Texture2D)
USE_DOTOSGWRAPPER(Image)
USE_DOTOSGWRAPPER(AlphaFunc)
USE_DOTOSGWRAPPER(BlendFunc)
USE_DOTOSGWRAPPER(CullFace)
USE_DOTOSGWRAPPER(FrontFace)
USE_COMPRESSOR_WRAPPER(NullCompressor)
#ifdef USE_ZLIB
USE_COMPRESSOR_WRAPPER(ZLibCompressor)
#endif

void osgdb_initialize_static_registry() {
	// Registration is performed by static proxies in the source objects pulled
	// in above. Accessing the singleton here finishes initialization on the main
	// thread before any parser job can be queued.
	(void)osgDB::Registry::instance();
	osgdb_register_godot_image_reader();
}
