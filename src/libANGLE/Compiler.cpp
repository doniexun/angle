//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Compiler.cpp: implements the gl::Compiler class.

#include "libANGLE/Compiler.h"

#include "common/debug.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/renderer/CompilerImpl.h"
#include "libANGLE/renderer/GLImplFactory.h"

namespace gl
{

namespace
{

// Global count of active shader compiler handles. Needed to know when to call ShInitialize and
// ShFinalize.
size_t activeCompilerHandles = 0;

ShShaderSpec SelectShaderSpec(GLint majorVersion, GLint minorVersion)
{
    if (majorVersion >= 3)
    {
        if (minorVersion == 1)
        {
            return SH_GLES3_1_SPEC;
        }
        else
        {
            return SH_GLES3_SPEC;
        }
    }
    return SH_GLES2_SPEC;
}

}  // anonymous namespace

Compiler::Compiler(rx::GLImplFactory *implFactory, const ContextState &state)
    : mImplementation(implFactory->createCompiler()),
      mSpec(SelectShaderSpec(state.getClientMajorVersion(), state.getClientMinorVersion())),
      mOutputType(mImplementation->getTranslatorOutputType()),
      mResources(),
      mFragmentCompiler(nullptr),
      mVertexCompiler(nullptr)
{
    ASSERT(state.getClientMajorVersion() == 2 || state.getClientMajorVersion() == 3);

    const gl::Caps &caps             = state.getCaps();
    const gl::Extensions &extensions = state.getExtensions();

    ShInitBuiltInResources(&mResources);
    mResources.MaxVertexAttribs             = caps.maxVertexAttributes;
    mResources.MaxVertexUniformVectors      = caps.maxVertexUniformVectors;
    mResources.MaxVaryingVectors            = caps.maxVaryingVectors;
    mResources.MaxVertexTextureImageUnits   = caps.maxVertexTextureImageUnits;
    mResources.MaxCombinedTextureImageUnits = caps.maxCombinedTextureImageUnits;
    mResources.MaxTextureImageUnits         = caps.maxTextureImageUnits;
    mResources.MaxFragmentUniformVectors    = caps.maxFragmentUniformVectors;
    mResources.MaxDrawBuffers               = caps.maxDrawBuffers;
    mResources.OES_standard_derivatives     = extensions.standardDerivatives;
    mResources.EXT_draw_buffers             = extensions.drawBuffers;
    mResources.EXT_shader_texture_lod       = extensions.shaderTextureLOD;
    mResources.OES_EGL_image_external          = extensions.eglImageExternal;
    mResources.OES_EGL_image_external_essl3    = extensions.eglImageExternalEssl3;
    mResources.NV_EGL_stream_consumer_external = extensions.eglStreamConsumerExternal;
    // TODO: use shader precision caps to determine if high precision is supported?
    mResources.FragmentPrecisionHigh = 1;
    mResources.EXT_frag_depth        = extensions.fragDepth;

    // GLSL ES 3.0 constants
    mResources.MaxVertexOutputVectors  = caps.maxVertexOutputComponents / 4;
    mResources.MaxFragmentInputVectors = caps.maxFragmentInputComponents / 4;
    mResources.MinProgramTexelOffset   = caps.minProgramTexelOffset;
    mResources.MaxProgramTexelOffset   = caps.maxProgramTexelOffset;
}

Compiler::~Compiler()
{
    release();
    SafeDelete(mImplementation);
}

Error Compiler::release()
{
    if (mFragmentCompiler)
    {
        ShDestruct(mFragmentCompiler);
        mFragmentCompiler = nullptr;

        ASSERT(activeCompilerHandles > 0);
        activeCompilerHandles--;
    }

    if (mVertexCompiler)
    {
        ShDestruct(mVertexCompiler);
        mVertexCompiler = nullptr;

        ASSERT(activeCompilerHandles > 0);
        activeCompilerHandles--;
    }

    if (activeCompilerHandles == 0)
    {
        ShFinalize();
    }

    mImplementation->release();

    return gl::Error(GL_NO_ERROR);
}

ShHandle Compiler::getCompilerHandle(GLenum type)
{
    ShHandle *compiler = nullptr;
    switch (type)
    {
        case GL_VERTEX_SHADER:
            compiler = &mVertexCompiler;
            break;

        case GL_FRAGMENT_SHADER:
            compiler = &mFragmentCompiler;
            break;

        default:
            UNREACHABLE();
            return nullptr;
    }

    if (!(*compiler))
    {
        if (activeCompilerHandles == 0)
        {
            ShInitialize();
        }

        *compiler = ShConstructCompiler(type, mSpec, mOutputType, &mResources);
        activeCompilerHandles++;
    }

    return *compiler;
}

}  // namespace gl
