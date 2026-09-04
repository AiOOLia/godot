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
#include <osg/GL>
#include <osg/PointSprite>
#include <osg/Point>
#include <osg/State>
#include <osg/buffered_value>
#include <osg/Notify>

using namespace osg;

PointSprite::PointSprite()
    : _coordOriginMode(UPPER_LEFT)
{
}

PointSprite::~PointSprite()
{
}

int PointSprite::compare(const StateAttribute& sa) const
{
    COMPARE_StateAttribute_Types(PointSprite,sa)

    COMPARE_StateAttribute_Parameter(_coordOriginMode)

    return 0; // passed all the above comparison macros, must be equal.
}


bool PointSprite::checkValidityOfAssociatedModes(osg::State& state) const
{
    return true;
}

void PointSprite::apply(osg::State& state) const
{
   
}
