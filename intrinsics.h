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

/**
 * This header file defines all the available intrinsics in one place. It
 * expands to a list of macros of the form:
 * 
 * INTRINSIC(name, num_reg_inputs, reg_input_components, num_reg_outputs,
 * 	     reg_output_components, num_variables, has_const_index, flags)
 * 
 * Which should correspond one-to-one with the nir_intrinsic_info structure. It
 * is included in both ir.h to create the nir_intrinsic enum (with members of
 * the form nir_intrinsic_(name)) and and in opcodes.c to create
 * nir_intrinsic_infos, which is a const array of nir_intrinsic_info structures
 * for each intrinsic.
 */

#define ARR(...) { __VA_ARGS__ }


INTRINSIC(load_var_vec1,  1, ARR(1), 0, ARR(), 1, false,
	  NIR_INTRINSIC_CAN_ELIMINATE)
INTRINSIC(load_var_vec2,   1, ARR(2), 0, ARR(), 1, false,
	  NIR_INTRINSIC_CAN_ELIMINATE)
INTRINSIC(load_var_vec3,   1, ARR(3), 0, ARR(), 1, false,
	  NIR_INTRINSIC_CAN_ELIMINATE)
INTRINSIC(load_var_vec4,   1, ARR(4), 0, ARR(), 1, false,
	  NIR_INTRINSIC_CAN_ELIMINATE)
INTRINSIC(store_var_vec1, 0, ARR(), 1, ARR(1), 1, false,  0)
INTRINSIC(store_var_vec2,  0, ARR(), 1, ARR(2), 1, false, 0)
INTRINSIC(store_var_vec3,  0, ARR(), 1, ARR(3), 1, false, 0)
INTRINSIC(store_var_vec4,  0, ARR(), 1, ARR(4), 1, false, 0)
INTRINSIC(copy_var,        0, ARR(), 0, ARR(),  2, false, 0)

#define LOAD(name, has_const_index, flags) \
   INTRINSIC(load_##name, 1, ARR(1), 1, ARR(4), 0, has_const_index, \
	     NIR_INTRINSIC_CAN_ELIMINATE | flags)

LOAD(uniform, false, NIR_INTRINSIC_CAN_REORDER)
LOAD(ubo, true, NIR_INTRINSIC_CAN_REORDER)
LOAD(input, false, NIR_INTRINSIC_CAN_REORDER)
/* LOAD(ssbo, true, 0) */

#define STORE(name, has_const_index, flags) \
   INTRINSIC(store_##name, 2, ARR(1, 4), 0, ARR(), 0, has_const_index, flags)

STORE(output, false, 0)
/* STORE(ssbo, true, 0) */

LAST_INTRINSIC(store_output)
