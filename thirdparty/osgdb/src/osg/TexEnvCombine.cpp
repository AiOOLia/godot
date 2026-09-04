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
#include <osg/TexEnvCombine>
#include <osg/State>
#include <osg/Notify>

using namespace osg;

TexEnvCombine::TexEnvCombine():
            _needsTexEnvCrossbar(false),
            _combine_RGB(GL_MODULATE),
            _combine_Alpha(GL_MODULATE),
            _source0_RGB(GL_TEXTURE),
            _source1_RGB(GL_PREVIOUS_ARB),
            _source2_RGB(GL_CONSTANT_ARB),
            _source0_Alpha(GL_TEXTURE),
            _source1_Alpha(GL_PREVIOUS_ARB),
            _source2_Alpha(GL_CONSTANT_ARB),
            _operand0_RGB(GL_SRC_COLOR),
            _operand1_RGB(GL_SRC_COLOR),
            _operand2_RGB(GL_SRC_ALPHA),
            _operand0_Alpha(GL_SRC_ALPHA),
            _operand1_Alpha(GL_SRC_ALPHA),
            _operand2_Alpha(GL_SRC_ALPHA),
            _scale_RGB(1.0),
            _scale_Alpha(1.0),
            _constantColor(0.0f,0.0f,0.0f,0.0f)
{
}

TexEnvCombine::~TexEnvCombine()
{
}

void TexEnvCombine::apply(State& state) const
{

}
void TexEnvCombine::setCombine_RGB(GLint cm) { _combine_RGB = cm; }
void TexEnvCombine::setCombine_Alpha(GLint cm) { _combine_Alpha = cm; }

void TexEnvCombine::setSource0_RGB(GLint sp) { _source0_RGB = sp; computeNeedForTexEnvCombiners(); }
void TexEnvCombine::setSource1_RGB(GLint sp) { _source1_RGB = sp; computeNeedForTexEnvCombiners(); }
void TexEnvCombine::setSource2_RGB(GLint sp) { _source2_RGB = sp; computeNeedForTexEnvCombiners(); }

void TexEnvCombine::setSource0_Alpha(GLint sp) { _source0_Alpha = sp; computeNeedForTexEnvCombiners(); }
void TexEnvCombine::setSource1_Alpha(GLint sp) { _source1_Alpha = sp; computeNeedForTexEnvCombiners(); }
void TexEnvCombine::setSource2_Alpha(GLint sp) { _source2_Alpha = sp; computeNeedForTexEnvCombiners(); }

void TexEnvCombine::setOperand0_RGB(GLint op) { _operand0_RGB = op; }
void TexEnvCombine::setOperand1_RGB(GLint op) { _operand1_RGB = op; }
void TexEnvCombine::setOperand2_RGB(GLint op) { _operand2_RGB = op; }

static GLint Valid_Operand_Alpha(GLint op, const char* functionName)
{
    if (op==TexEnvCombine::SRC_ALPHA || op==TexEnvCombine::ONE_MINUS_SRC_ALPHA) return op;

    OSG_WARN<<"Warning:: TexEnvCombine::"<<functionName<<"("<<op<<") invalid parameter value,"<<std::endl<<
                  "          must be SRC_ALPHA or ONE_MINUS_SRC_ALPHA, resetting to SRC_ALPHA."<<std::endl;
    return TexEnvCombine::SRC_ALPHA;
}

void TexEnvCombine::setOperand0_Alpha(GLint op)
{
    _operand0_Alpha = Valid_Operand_Alpha(op,"setOperand0_Alpha");
}
void TexEnvCombine::setOperand1_Alpha(GLint op)
{
    _operand1_Alpha = Valid_Operand_Alpha(op,"setOperand1_Alpha");
}
void TexEnvCombine::setOperand2_Alpha(GLint op)
{
    _operand2_Alpha = Valid_Operand_Alpha(op,"setOperand2_Alpha");
}

void TexEnvCombine::setScale_RGB(float scale) { _scale_RGB = scale; }
void TexEnvCombine::setScale_Alpha(float scale) { _scale_Alpha = scale; }
