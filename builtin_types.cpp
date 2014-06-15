/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file builtin_types.cpp
 *
 * The glsl_type class has static members to represent all the built-in types
 * (such as the glsl_type::_float_type flyweight) as well as convenience pointer
 * accessors (such as glsl_type::float_type).  Those global variables are
 * declared and initialized in this file.
 *
 * This also contains _mesa_glsl_initialize_types(), a function which populates
 * a symbol table with the available built-in types for a particular language
 * version and set of enabled extensions.
 */

#include "glsl_types.h"

/**
 * Declarations of type flyweights (glsl_type::_foo_type) and
 * convenience pointers (glsl_type::foo_type).
 * @{
 */
#define DECL_TYPE(NAME, ...)                                    \
   const glsl_type glsl_type::_##NAME##_type = glsl_type(__VA_ARGS__, #NAME); \
   const glsl_type *const glsl_type::NAME##_type = &glsl_type::_##NAME##_type;

#define STRUCT_TYPE(NAME)                                       \
   const glsl_type glsl_type::_struct_##NAME##_type =           \
      glsl_type(NAME##_fields, Elements(NAME##_fields), #NAME); \
   const glsl_type *const glsl_type::struct_##NAME##_type =     \
      &glsl_type::_struct_##NAME##_type;

static const struct glsl_struct_field gl_DepthRangeParameters_fields[] = {
   { glsl_type::float_type, "near", false, -1 },
   { glsl_type::float_type, "far",  false, -1 },
   { glsl_type::float_type, "diff", false, -1 },
};

static const struct glsl_struct_field gl_PointParameters_fields[] = {
   { glsl_type::float_type, "size", false, -1 },
   { glsl_type::float_type, "sizeMin", false, -1 },
   { glsl_type::float_type, "sizeMax", false, -1 },
   { glsl_type::float_type, "fadeThresholdSize", false, -1 },
   { glsl_type::float_type, "distanceConstantAttenuation", false, -1 },
   { glsl_type::float_type, "distanceLinearAttenuation", false, -1 },
   { glsl_type::float_type, "distanceQuadraticAttenuation", false, -1 },
};

static const struct glsl_struct_field gl_MaterialParameters_fields[] = {
   { glsl_type::vec4_type, "emission", false, -1 },
   { glsl_type::vec4_type, "ambient", false, -1 },
   { glsl_type::vec4_type, "diffuse", false, -1 },
   { glsl_type::vec4_type, "specular", false, -1 },
   { glsl_type::float_type, "shininess", false, -1 },
};

static const struct glsl_struct_field gl_LightSourceParameters_fields[] = {
   { glsl_type::vec4_type, "ambient", false, -1 },
   { glsl_type::vec4_type, "diffuse", false, -1 },
   { glsl_type::vec4_type, "specular", false, -1 },
   { glsl_type::vec4_type, "position", false, -1 },
   { glsl_type::vec4_type, "halfVector", false, -1 },
   { glsl_type::vec3_type, "spotDirection", false, -1 },
   { glsl_type::float_type, "spotExponent", false, -1 },
   { glsl_type::float_type, "spotCutoff", false, -1 },
   { glsl_type::float_type, "spotCosCutoff", false, -1 },
   { glsl_type::float_type, "constantAttenuation", false, -1 },
   { glsl_type::float_type, "linearAttenuation", false, -1 },
   { glsl_type::float_type, "quadraticAttenuation", false, -1 },
};

static const struct glsl_struct_field gl_LightModelParameters_fields[] = {
   { glsl_type::vec4_type, "ambient", false, -1 },
};

static const struct glsl_struct_field gl_LightModelProducts_fields[] = {
   { glsl_type::vec4_type, "sceneColor", false, -1 },
};

static const struct glsl_struct_field gl_LightProducts_fields[] = {
   { glsl_type::vec4_type, "ambient", false, -1 },
   { glsl_type::vec4_type, "diffuse", false, -1 },
   { glsl_type::vec4_type, "specular", false, -1 },
};

static const struct glsl_struct_field gl_FogParameters_fields[] = {
   { glsl_type::vec4_type, "color", false, -1 },
   { glsl_type::float_type, "density", false, -1 },
   { glsl_type::float_type, "start", false, -1 },
   { glsl_type::float_type, "end", false, -1 },
   { glsl_type::float_type, "scale", false, -1 },
};

#include "builtin_type_macros.h"
/** @} */
