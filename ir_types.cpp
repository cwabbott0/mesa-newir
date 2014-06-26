/*
 * Copyright © 2014 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Connor Abbott (cwabbott0@gmail.com)
 *
 */

#include "ir_types.h"
#include "glsl_types.h"

/*
 * copied from ir.h, don't want to include it here yet...
 */
static inline bool
is_gl_identifier(const char *s)
{
   return s && s[0] == 'g' && s[1] == 'l' && s[2] == '_';
}

void
glsl_print_type(const glsl_type *type, FILE *fp)
{
   if (type->base_type == GLSL_TYPE_ARRAY) {
      glsl_print_type(type->fields.array, fp);
      fprintf(fp, "[%u]", type->length);
   } else if ((type->base_type == GLSL_TYPE_STRUCT)
              && !is_gl_identifier(type->name)) {
      fprintf(fp, "%s@%p", type->name, (void *) type);
   } else {
      fprintf(fp, "%s", type->name);
   }
}

void
glsl_print_struct(const glsl_type *type, FILE *fp)
{
   assert(type->base_type == GLSL_TYPE_STRUCT);
   
   fprintf(fp, "struct {\n");
   for (unsigned i = 0; i < type->length; i++) {
      fprintf(fp, "\t");
      glsl_print_type(type->fields.structure[i].type, fp);
      fprintf(fp, " %s;\n", type->fields.structure[i].name);
   }
   fprintf(fp, "}\n");
}

const glsl_type *
glsl_get_array_element(const glsl_type* type)
{
   return type->fields.array;
}

const glsl_type*
glsl_get_struct_field(const glsl_type *type, const char *field)
{
   for (unsigned i = 0; i < type->length; i++) {
      if (strcmp(type->fields.structure[i].name, field) == 0) {
	 return type->fields.structure[i].type;
      }
   }
   
   return NULL;
}

bool
glsl_type_is_void(const glsl_type *type)
{
   return type == glsl_type::void_type;
}


