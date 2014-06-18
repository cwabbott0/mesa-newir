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
 * This file defines the list of all possible intrinsics. To use it, define the
 * macro INTRINSIC(...) and then include this file.
 */

#define ARR(...) { __VA_ARGS__ }

/* name, num_reg_inputs, reg_input_components, num_reg_outputs, reg_output_components, num_variables, has_const_index, is_load, is_reorderable_load */

INTRINSIC(load_var_float,  1, ARR(1), 0, ARR(), 1, false, true, false)
INTRINSIC(load_var_vec2,   1, ARR(2), 0, ARR(), 1, false, true, false)
INTRINSIC(load_var_vec3,   1, ARR(3), 0, ARR(), 1, false, true, false)
INTRINSIC(load_var_vec4,   1, ARR(4), 0, ARR(), 1, false, true, false)
INTRINSIC(store_var_float, 0, ARR(), 1, ARR(1), 1, false, false, false)
INTRINSIC(store_var_vec2,  0, ARR(), 1, ARR(2), 1, false, false, false)
INTRINSIC(store_var_vec3,  0, ARR(), 1, ARR(3), 1, false, false, false)
INTRINSIC(store_var_vec4,  0, ARR(), 1, ARR(4), 1, false, false, false)
INTRINSIC(copy_var,        0, ARR(), 0, ARR(),  2, false, false, false)

LAST_INTRINSIC
