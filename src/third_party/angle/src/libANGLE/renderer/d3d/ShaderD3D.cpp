//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderD3D.cpp: Defines the rx::ShaderD3D class which implements rx::ShaderImpl.

#include "libANGLE/Shader.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/CompilerD3D.h"
#include "libANGLE/features.h"

#include "common/utilities.h"

// Definitions local to the translation unit
namespace
{

const char *GetShaderTypeString(GLenum type)
{
    switch (type)
    {
      case GL_VERTEX_SHADER:
        return "VERTEX";

      case GL_FRAGMENT_SHADER:
        return "FRAGMENT";

      default:
        UNREACHABLE();
        return "";
    }
}

}

namespace rx
{

template <typename VarT>
void FilterInactiveVariables(std::vector<VarT> *variableList)
{
    ASSERT(variableList);

    for (size_t varIndex = 0; varIndex < variableList->size();)
    {
        if (!(*variableList)[varIndex].staticUse)
        {
            variableList->erase(variableList->begin() + varIndex);
        }
        else
        {
            varIndex++;
        }
    }
}

template <typename VarT>
const std::vector<VarT> *GetShaderVariables(const std::vector<VarT> *variableList)
{
    ASSERT(variableList);
    return variableList;
}

ShaderD3D::ShaderD3D(GLenum type)
    : mShaderType(type),
      mShaderVersion(100)
{
    uncompile();
}

ShaderD3D::~ShaderD3D()
{
}

ShaderD3D *ShaderD3D::makeShaderD3D(ShaderImpl *impl)
{
    ASSERT(HAS_DYNAMIC_TYPE(ShaderD3D*, impl));
    return static_cast<ShaderD3D*>(impl);
}

const ShaderD3D *ShaderD3D::makeShaderD3D(const ShaderImpl *impl)
{
    ASSERT(HAS_DYNAMIC_TYPE(const ShaderD3D*, impl));
    return static_cast<const ShaderD3D*>(impl);
}

std::string ShaderD3D::getDebugInfo() const
{
    return mDebugInfo + std::string("\n// ") + GetShaderTypeString(mShaderType) + " SHADER END\n";
}


void ShaderD3D::parseVaryings(ShHandle compiler)
{
    if (!mHlsl.empty())
    {
        const std::vector<sh::Varying> *varyings = ShGetVaryings(compiler);
        ASSERT(varyings);

        for (size_t varyingIndex = 0; varyingIndex < varyings->size(); varyingIndex++)
        {
            mVaryings.push_back(gl::PackedVarying((*varyings)[varyingIndex]));
        }

        mUsesMultipleRenderTargets = mHlsl.find("GL_USES_MRT")          != std::string::npos;
        mUsesFragColor             = mHlsl.find("GL_USES_FRAG_COLOR")   != std::string::npos;
        mUsesFragData              = mHlsl.find("GL_USES_FRAG_DATA")    != std::string::npos;
        mUsesFragCoord             = mHlsl.find("GL_USES_FRAG_COORD")   != std::string::npos;
        mUsesFrontFacing           = mHlsl.find("GL_USES_FRONT_FACING") != std::string::npos;
        mUsesPointSize             = mHlsl.find("GL_USES_POINT_SIZE")   != std::string::npos;
        mUsesPointCoord            = mHlsl.find("GL_USES_POINT_COORD")  != std::string::npos;
        mUsesDepthRange            = mHlsl.find("GL_USES_DEPTH_RANGE")  != std::string::npos;
        mUsesFragDepth             = mHlsl.find("GL_USES_FRAG_DEPTH")   != std::string::npos;
        mUsesDiscardRewriting      = mHlsl.find("ANGLE_USES_DISCARD_REWRITING") != std::string::npos;
        mUsesNestedBreak           = mHlsl.find("ANGLE_USES_NESTED_BREAK") != std::string::npos;
    }
}

void ShaderD3D::resetVaryingsRegisterAssignment()
{
    for (size_t varyingIndex = 0; varyingIndex < mVaryings.size(); varyingIndex++)
    {
        mVaryings[varyingIndex].resetRegisterAssignment();
    }
}

// initialize/clean up previous state
void ShaderD3D::uncompile()
{
    // set by compileToHLSL
    mCompilerOutputType = SH_ESSL_OUTPUT;
    mHlsl.clear();
    mInfoLog.clear();

    mUsesMultipleRenderTargets = false;
    mUsesFragColor = false;
    mUsesFragData = false;
    mUsesFragCoord = false;
    mUsesFrontFacing = false;
    mUsesPointSize = false;
    mUsesPointCoord = false;
    mUsesDepthRange = false;
    mUsesFragDepth = false;
    mShaderVersion = 100;
    mUsesDiscardRewriting = false;
    mUsesNestedBreak = false;

    mVaryings.clear();
    mUniforms.clear();
    mInterfaceBlocks.clear();
    mActiveAttributes.clear();
    mActiveOutputVariables.clear();
    mDebugInfo.clear();
}

void ShaderD3D::compileToHLSL(ShHandle compiler, const std::string &source)
{
    int compileOptions = (SH_OBJECT_CODE | SH_VARIABLES);
    std::string sourcePath;

#if !defined (ANGLE_ENABLE_WINDOWS_STORE)
    if (gl::perfActive())
    {
        sourcePath = getTempPath();
        writeFile(sourcePath.c_str(), source.c_str(), source.length());
        compileOptions |= SH_LINE_DIRECTIVES;
    }
#endif

    int result;
    if (sourcePath.empty())
    {
        const char* sourceStrings[] =
        {
            source.c_str(),
        };

        result = ShCompile(compiler, sourceStrings, ArraySize(sourceStrings), compileOptions);
    }
    else
    {
        const char* sourceStrings[] =
        {
            sourcePath.c_str(),
            source.c_str(),
        };

        result = ShCompile(compiler, sourceStrings, ArraySize(sourceStrings), compileOptions | SH_SOURCE_PATH);
    }

    mShaderVersion = ShGetShaderVersion(compiler);

    if (result)
    {
        mHlsl = ShGetObjectCode(compiler);

#ifdef _DEBUG
        // Prefix hlsl shader with commented out glsl shader
        // Useful in diagnostics tools like pix which capture the hlsl shaders
        std::ostringstream hlslStream;
        hlslStream << "// GLSL\n";
        hlslStream << "//\n";

        size_t curPos = 0;
        while (curPos != std::string::npos)
        {
            size_t nextLine = source.find("\n", curPos);
            size_t len = (nextLine == std::string::npos) ? std::string::npos : (nextLine - curPos + 1);

            hlslStream << "// " << source.substr(curPos, len);

            curPos = (nextLine == std::string::npos) ? std::string::npos : (nextLine + 1);
        }
        hlslStream << "\n\n";
        hlslStream << mHlsl;
        mHlsl = hlslStream.str();
#endif

        mUniforms = *GetShaderVariables(ShGetUniforms(compiler));

        for (size_t uniformIndex = 0; uniformIndex < mUniforms.size(); uniformIndex++)
        {
            const sh::Uniform &uniform = mUniforms[uniformIndex];

            if (uniform.staticUse)
            {
                unsigned int index = -1;
                bool result = ShGetUniformRegister(compiler, uniform.name, &index);
                UNUSED_ASSERTION_VARIABLE(result);
                ASSERT(result);

                mUniformRegisterMap[uniform.name] = index;
            }
        }

        mInterfaceBlocks = *GetShaderVariables(ShGetInterfaceBlocks(compiler));

        for (size_t blockIndex = 0; blockIndex < mInterfaceBlocks.size(); blockIndex++)
        {
            const sh::InterfaceBlock &interfaceBlock = mInterfaceBlocks[blockIndex];

            if (interfaceBlock.staticUse)
            {
                unsigned int index = -1;
                bool result = ShGetInterfaceBlockRegister(compiler, interfaceBlock.name, &index);
                UNUSED_ASSERTION_VARIABLE(result);
                ASSERT(result);

                mInterfaceBlockRegisterMap[interfaceBlock.name] = index;
            }
        }
    }
    else
    {
        mInfoLog = ShGetInfoLog(compiler);

        TRACE("\n%s", mInfoLog.c_str());
    }
}

D3DWorkaroundType ShaderD3D::getD3DWorkarounds() const
{
    if (mUsesDiscardRewriting)
    {
        // ANGLE issue 486:
        // Work-around a D3D9 compiler bug that presents itself when using conditional discard, by disabling optimization
        return ANGLE_D3D_WORKAROUND_SKIP_OPTIMIZATION;
    }

    if (mUsesNestedBreak)
    {
        // ANGLE issue 603:
        // Work-around a D3D9 compiler bug that presents itself when using break in a nested loop, by maximizing optimization
        // We want to keep the use of ANGLE_D3D_WORKAROUND_MAX_OPTIMIZATION minimal to prevent hangs, so usesDiscard takes precedence
        return ANGLE_D3D_WORKAROUND_MAX_OPTIMIZATION;
    }

    return ANGLE_D3D_WORKAROUND_NONE;
}

// true if varying x has a higher priority in packing than y
bool ShaderD3D::compareVarying(const gl::PackedVarying &x, const gl::PackedVarying &y)
{
    if (x.type == y.type)
    {
        return x.arraySize > y.arraySize;
    }

    // Special case for handling structs: we sort these to the end of the list
    if (x.type == GL_STRUCT_ANGLEX)
    {
        return false;
    }

    if (y.type == GL_STRUCT_ANGLEX)
    {
        return true;
    }

    return gl::VariableSortOrder(x.type) < gl::VariableSortOrder(y.type);
}

unsigned int ShaderD3D::getUniformRegister(const std::string &uniformName) const
{
    ASSERT(mUniformRegisterMap.count(uniformName) > 0);
    return mUniformRegisterMap.find(uniformName)->second;
}

unsigned int ShaderD3D::getInterfaceBlockRegister(const std::string &blockName) const
{
    ASSERT(mInterfaceBlockRegisterMap.count(blockName) > 0);
    return mInterfaceBlockRegisterMap.find(blockName)->second;
}

GLenum ShaderD3D::getShaderType() const
{
    return mShaderType;
}

ShShaderOutput ShaderD3D::getCompilerOutputType() const
{
    return mCompilerOutputType;
}

bool ShaderD3D::compile(gl::Compiler *compiler, const std::string &source)
{
    uncompile();

    CompilerD3D *compilerD3D = CompilerD3D::makeCompilerD3D(compiler->getImplementation());
    ShHandle compilerHandle = compilerD3D->getCompilerHandle(mShaderType);

    mCompilerOutputType = ShGetShaderOutputType(compilerHandle);

    compileToHLSL(compilerHandle, source);

    if (mShaderType == GL_VERTEX_SHADER)
    {
        parseAttributes(compilerHandle);
    }

    parseVaryings(compilerHandle);

    if (mShaderType == GL_FRAGMENT_SHADER)
    {
        std::sort(mVaryings.begin(), mVaryings.end(), compareVarying);

        const std::string &hlsl = getTranslatedSource();
        if (!hlsl.empty())
        {
            mActiveOutputVariables = *GetShaderVariables(ShGetOutputVariables(compilerHandle));
            FilterInactiveVariables(&mActiveOutputVariables);
        }
    }

#if ANGLE_SHADER_DEBUG_INFO == ANGLE_ENABLED
    mDebugInfo += std::string("// ") + GetShaderTypeString(mShaderType) + " SHADER BEGIN\n";
    mDebugInfo += "\n// GLSL BEGIN\n\n" + source + "\n\n// GLSL END\n\n\n";
    mDebugInfo += "// INITIAL HLSL BEGIN\n\n" + getTranslatedSource() + "\n// INITIAL HLSL END\n\n\n";
    // Successive steps will append more info
#else
    mDebugInfo += getTranslatedSource();
#endif

    return !getTranslatedSource().empty();
}

void ShaderD3D::parseAttributes(ShHandle compiler)
{
    const std::string &hlsl = getTranslatedSource();
    if (!hlsl.empty())
    {
        mActiveAttributes = *GetShaderVariables(ShGetAttributes(compiler));
        FilterInactiveVariables(&mActiveAttributes);
    }
}

}
