/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/
#include <osg/GLExtensions>
#include <osg/TexEnv>
#include <osg/State>
#include <osg/Notify>
#include <osg/io_utils>

using namespace osg;

TexEnv::TexEnv(Mode mode)
{
    _mode = mode;
    _color = new Vec4Uniform(Vec4(0.0f,0.0f,0.0f,0.0f));
    configureUniformNames();
}

TexEnv::TexEnv(const TexEnv& texenv,const CopyOp& copyop):
    TextureAttribute(texenv,copyop),
    _mode(texenv._mode)
{
    _color = new Vec4Uniform(texenv.getColor());
    configureUniformNames();
}

TexEnv::~TexEnv()
{
}

void TexEnv::setMode( Mode mode )
{
    if (_mode!=mode)
    {
        _mode = mode;
        configureUniformNames();
    }
}

void TexEnv::configureUniformNames()
{
    _color->setName("osg_TextureEnvColor",_textureUnit);

    MakeString str;
    std::string TEXTURE_ENV_FUNCTION = str<<"TEXTURE_ENV_FUNCTION"<<_textureUnit;

    _defineList.clear();

    switch(_mode)
    {
        case(DECAL):
            _defineList[TEXTURE_ENV_FUNCTION] = StateSet::DefinePair(str.clear()<<"(color, texture_color, unit) texenv_DECAL(color, texture_color, "<<_textureUnit<<")", osg::StateAttribute::ON);
            break;
        case(MODULATE):
            _defineList[TEXTURE_ENV_FUNCTION] = StateSet::DefinePair(str.clear()<<"(color, texture_color, unit) texenv_MODULATE(color, texture_color, "<<_textureUnit<<")", osg::StateAttribute::ON);
            break;
        case(BLEND):
            _defineList[TEXTURE_ENV_FUNCTION] = StateSet::DefinePair(str.clear()<<"(color, texture_color, unit) texenv_MODULATE(color, texture_color, "<<_textureUnit<<")", osg::StateAttribute::ON);
            break;
        case(REPLACE):
            _defineList[TEXTURE_ENV_FUNCTION] = StateSet::DefinePair(str.clear()<<"(color, texture_color, unit) texenv_REPLACE(color, texture_color), "<<_textureUnit<<")", osg::StateAttribute::ON);
            break;
        case(ADD):
            _defineList[TEXTURE_ENV_FUNCTION] = StateSet::DefinePair(str.clear()<<"(color, texture_color, unit) texenv_ADD(color, texture_color), "<<_textureUnit<<")", osg::StateAttribute::ON);
            break;
    }
}

void TexEnv::apply(State& state) const
{

}
