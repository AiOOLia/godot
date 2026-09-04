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
#include <osg/Light>
#include <osg/StateSet>
#include <osg/Notify>

using namespace osg;

Light::Light( void )
{
    init();
}

Light::Light(unsigned int lightnum)
{
    init();
    _lightnum = lightnum;
}

Light::~Light( void )
{
}


void Light::init( void )
{
    _lightnum = 0;
    _ambient.set(0.05f,0.05f,0.05f,1.0f);
    _diffuse.set(0.8f,0.8f,0.8f,1.0f);
    _specular.set(0.05f,0.05f,0.05f,1.0f);
    _position.set(0.0f,0.0f,1.0f,0.0f);
    _direction.set(0.0f,0.0f,-1.0f);
    _spot_exponent = 0.0f;
    _spot_cutoff = 180.0f;
    _constant_attenuation = 1.0f;
    _linear_attenuation = 0.0f;
    _quadratic_attenuation = 0.0f;

    //     OSG_DEBUG << "_ambient "<<_ambient<<std::endl;
    //     OSG_DEBUG << "_diffuse "<<_diffuse<<std::endl;
    //     OSG_DEBUG << "_specular "<<_specular<<std::endl;
    //     OSG_DEBUG << "_position "<<_position<<std::endl;
    //     OSG_DEBUG << "_direction "<<_direction<<std::endl;
    //     OSG_DEBUG << "_spot_exponent "<<_spot_exponent<<std::endl;
    //     OSG_DEBUG << "_spot_cutoff "<<_spot_cutoff<<std::endl;
    //     OSG_DEBUG << "_constant_attenuation "<<_constant_attenuation<<std::endl;
    //     OSG_DEBUG << "_linear_attenuation "<<_linear_attenuation<<std::endl;
    //     OSG_DEBUG << "_quadratic_attenuation "<<_quadratic_attenuation<<std::endl;
}

void Light::setLightNum(int num)
{
    if (_lightnum==num) return;

    ReassignToParents needToReassingToParentsWhenMemberValueChanges(this);

    _lightnum = num;
}

void Light::captureLightState()
{
}

void Light::apply(State&) const
{
}
