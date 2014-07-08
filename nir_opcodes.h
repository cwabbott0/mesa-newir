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
 * This header file defines all the available opcodes in one place. It expands
 * to a list of macros of the form:
 * 
 * OPCODE(name, num_inputs, per_component, output_size, input_sizes)
 * 
 * Which should correspond one-to-one with the nir_op_info structure. It is
 * included in both ir.h to create the nir_op enum (with members of the form
 * nir_op_(name)) and and in opcodes.c to create nir_op_infos, which is a
 * const array of nir_op_info structures for each opcode.
 */

#define ARR(...) { __VA_ARGS__ }

#define UNOP(name) OPCODE(name, 1, false, 0, ARR(0))
#define UNOP_HORIZ(name, output_size, input_size) \
   OPCODE(name, 1, true, output_size, ARR(input_size))

#define UNOP_REDUCE(name, output_size) \
   UNOP_HORIZ(name##2, output_size, 2) \
   UNOP_HORIZ(name##3, output_size, 3) \
   UNOP_HORIZ(name##4, output_size, 4)

UNOP(mov)

UNOP(inot) /* invert every bit of the integer */
UNOP(fnot) /* (src == 0.0) ? 1.0 : 0.0 */
UNOP(fneg)
UNOP(ineg)
UNOP(fabs)
UNOP(iabs)
UNOP(fsign)
UNOP(isign)
UNOP(frcp)
UNOP(frsq)
UNOP(fsqrt)
UNOP(fexp) /* < e^x */
UNOP(flog) /* log base e */
UNOP(fexp2)
UNOP(flog2)
UNOP(f2i)         /**< Float-to-integer conversion. */
UNOP(f2u)         /**< Float-to-unsigned conversion. */
UNOP(i2f)         /**< Integer-to-float conversion. */
UNOP(f2b)         /**< Float-to-boolean conversion */
UNOP(b2f)         /**< Boolean-to-float conversion */
UNOP(i2b)         /**< int-to-boolean conversion */
UNOP(u2f)         /**< Unsigned-to-float conversion. */

UNOP_REDUCE(bany, 1) /* returns ~0 if any component of src[0] != 0 */
UNOP_REDUCE(ball, 1) /* returns ~0 if all components of src[0] != 0 */
UNOP_REDUCE(fany, 1) /* returns 1.0 if any component of src[0] != 0 */
UNOP_REDUCE(fall, 1) /* returns 1.0 if all components of src[0] != 0 */

/**
 * \name Unary floating-point rounding operations.
 */
/*@{*/
UNOP(ftrunc)
UNOP(fceil)
UNOP(ffloor)
UNOP(ffract)
UNOP(fround_even)
/*@}*/

/**
 * \name Trigonometric operations.
 */
/*@{*/
UNOP(fsin)
UNOP(fcos)
/*@}*/

/**
 * \name Partial derivatives.
 */
/*@{*/
UNOP(fddx)
UNOP(fddy)
/*@}*/

/**
 * \name Floating point pack and unpack operations.
 */
/*@{*/
UNOP_HORIZ(pack_snorm_2x16, 1, 2)
UNOP_HORIZ(pack_snorm_4x8, 1, 4)
UNOP_HORIZ(pack_unorm_2x16, 1, 2)
UNOP_HORIZ(pack_unorm_4x8, 1, 8)
UNOP_HORIZ(pack_half_2x16, 1, 2)
UNOP_HORIZ(unpack_snorm_2x16, 2, 1)
UNOP_HORIZ(unpack_snorm_4x8, 4, 1)
UNOP_HORIZ(unpack_unorm_2x16, 2, 1)
UNOP_HORIZ(unpack_unorm_4x8, 4, 1)
UNOP_HORIZ(unpack_half_2x16, 2, 1)
/*@}*/

/**
 * \name Lowered floating point unpacking operations.
 */
/*@{*/
UNOP_HORIZ(unpack_half_2x16_split_x, 1, 1)
UNOP_HORIZ(unpack_half_2x16_split_y, 1, 1)
/*@}*/

/**
 * \name Bit operations, part of ARB_gpu_shader5.
 */
/*@{*/
UNOP(bitfield_reverse)
UNOP(bit_count)
UNOP(find_msb)
UNOP(find_lsb)
/*@}*/

UNOP_REDUCE(fnoise1, 1)
UNOP_REDUCE(fnoise2, 2)
UNOP_REDUCE(fnoise3, 3)
UNOP_REDUCE(fnoise4, 4)

#define BINOP(name) OPCODE(name, 2, true, 0, ARR(0, 0))
#define BINOP_HORIZ(name, output_size, src1_size, src2_size) \
   OPCODE(name, 2, true, output_size, ARR(src1_size, src2_size))
#define BINOP_REDUCE(name, output_size) \
   BINOP_HORIZ(name##2, output_size, 2, 2) \
   BINOP_HORIZ(name##3, output_size, 3, 3) \
   BINOP_HORIZ(name##4, output_size, 4, 4) \

BINOP(fadd)
BINOP(iadd)
BINOP(fsub)
BINOP(isub)

BINOP(fmul)
BINOP(imul) /* low 32-bits of signed/unsigned integer multiply */
BINOP(imul_high) /* high 32-bits of signed integer multiply */
BINOP(umul_high) /* high 32-bits of unsigned integer multiply */

BINOP(fdiv)
BINOP(idiv)
BINOP(udiv)

/**
 * returns a boolean representing the carry resulting from the addition of
 * the two unsigned arguments.
 */
BINOP(uadd_carry)

/**
 * returns a boolean representing the borrow resulting from the subtraction
 * of the two unsigned arguments.
 */
BINOP(usub_borrow)

BINOP(fmod)

/**
 * \name comparisons
 */
/*@{*/

/**
 * these integer-aware comparisons return a boolean (0 or ~0)
 */
BINOP(flt)
BINOP(fge)
BINOP(feq)
BINOP(fne)
BINOP(ilt)
BINOP(ige)
BINOP(ieq)
BINOP(ine)
BINOP(ult)
BINOP(uge)

/**
 * These comparisons for integer-less hardware return 1.0 and 0.0 for true
 * and false respectively
 */
BINOP(slt) /* Set on Less Than */
BINOP(sge) /* Set on Greater Than or Equal */
BINOP(seq) /* Set on Equal */
BINOP(sne) /* Set on Not Equal */

/*@}*/

BINOP(ishl)
BINOP(ishr)
BINOP(ushr)

/**
 * \name bitwise logic operators
 * 
 * These are also used as boolean and, or, xor for hardware supporting
 * integers.
 */
/*@{*/
BINOP(iand)
BINOP(ior)
BINOP(ixor)
/*@{*/

/**
 * \name floating point logic operators
 * 
 * These use (src != 0.0) for testing the truth of the input, and output 1.0
 * for true and 0.0 for false
 */
BINOP(fand)
BINOP(for)
BINOP(fxor)

BINOP_REDUCE(fdot, 1)

BINOP(fmin)
BINOP(imin)
BINOP(fmax)
BINOP(imax)
BINOP(umax)

BINOP(fpow)

BINOP_HORIZ(pack_half_2x16_split, 1, 1, 1)

BINOP(bfm)

BINOP(ldexp)

/* BINOP(vector_extract) (ugly and appears to never be used in GLSL IR) */

/**
 * Combines the first component of each input to make a 2-component vector.
 */
BINOP_HORIZ(vec2, 2, 1, 1)

#define TRIOP(name) OPCODE(name, 3, true, 0, ARR(0, 0, 0))
#define TRIOP_HORIZ(name, output_size, src1_size, src2_size, src3_size) \
   OPCODE(name, 3, false, output_size, ARR(src1_size, src2_size, src3_size))

TRIOP(ffma)

TRIOP(flrp)

/**
 * \name Conditional Select
 *
 * A vector conditional select instruction (like ?:, but operating per-
 * component on vectors). There are two versions, one for floating point
 * bools (0.0 vs 1.0) and one for integer bools (0 vs ~0).
 */

OPCODE(fcsel, 3, true, 0, ARR(1, 0, 0))
OPCODE(icsel, 3, true, 0, ARR(1, 0, 0))

TRIOP(bfi)

OPCODE(fvector_insert, 3, true, 0, ARR(0, 1, 1))
OPCODE(ivector_insert, 3, true, 0, ARR(0, 1, 1))

/**
 * Combines the first component of each input to make a 3-component vector.
 */
TRIOP_HORIZ(vec3, 3, 1, 1, 1)

#define QUADOP(name) OPCODE(name, 4, true, 0, ARR(0, 0, 0, 0))
#define QUADOP_HORIZ(name, output_size, src1_size, src2_size, src3_size, \
		     src4_size) \
   OPCODE(name, 4, false, output_size, ARR(src1_size, src2_size, src3_size, \
	  src4_size))

QUADOP(bitfield_insert)

QUADOP_HORIZ(vec4, 4, 1, 1, 1, 1)

LAST_OPCODE(vec4)
