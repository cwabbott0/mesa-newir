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

#include "ir.h"

int main(void)
{
   nir_shader *shader = nir_shader_create(NULL);
   nir_function *func = nir_function_create(shader, "main");
   nir_function_overload *overload = nir_function_overload_create(func);
   nir_function_impl *impl = nir_function_impl_create(overload);
   
   nir_loop *loop = nir_loop_create(shader);
   nir_cf_node_insert_begin(&impl->body, &loop->cf_node);
   
   nir_register *reg = nir_local_reg_create(impl);
   reg->num_components = 1;
   
   nir_load_const_instr *instr = nir_load_const_instr_create(shader);
   instr->dest.reg.reg = reg;
   instr->value.u[0] = ~0;
   nir_instr_insert_before_cf(&loop->cf_node, &instr->instr);
   
   nir_if *if_stmt = nir_if_create(shader);
   if_stmt->condition.reg.reg = reg;
   nir_cf_node_insert_begin(&loop->body, &if_stmt->cf_node);
   
   nir_jump_instr *break_instr = nir_jump_instr_create(shader, nir_jump_break);
   nir_instr_insert_after_cf_list(&if_stmt->then_list, &break_instr->instr);
   
   nir_print_shader(shader, stdout);
   nir_validate_shader(shader);   
   
   ralloc_free(shader);
   
   return 0;
}