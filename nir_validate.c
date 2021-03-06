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

#include "nir.h"
#include <assert.h>

/*
 * This file checks for invalid IR indicating a bug somewhere in the compiler.
 */

/*
 * Per-register validation state.
 */

typedef struct {
   /* 
    * equivalent to the uses and defs in nir_register, but built up by the
    * validator. At the end, we verify that the sets have the same entries.
    */
   struct hash_table *uses, *defs;
   nir_function_impl *where_defined; /* NULL for global registers */
} reg_validate_state;

typedef struct {
   /* map of register -> validation state (struct above) */
   struct hash_table *regs;
   
   /* the current instruction being validated */
   nir_instr *instr;
   
   /* the current basic block being validated */
   nir_block *block;
   
   /* the parent of the current cf node being visited */
   nir_cf_node *parent_node;
   
   /* the current function implementation being validated */
   nir_function_impl *impl;
   
   /* map of SSA value -> function implementation where it is defined */
   struct hash_table *ssa_defs;
   
   /* map of local variable -> function implementation where it is defined */
   struct hash_table *var_defs;
} validate_state;

static void validate_src(nir_src *src, validate_state *state);

static void
validate_reg_src(nir_reg_src *src, validate_state *state)
{
   assert(src->reg != NULL);
   
   struct hash_entry *entry =
      _mesa_hash_table_search(src->reg->uses, _mesa_hash_pointer(state->instr),
			      state->instr);
   assert(entry && "use not in nir_register.uses");
   
   entry = _mesa_hash_table_search(state->regs, _mesa_hash_pointer(src->reg),
				 src->reg);
   
   assert(entry);
   
   reg_validate_state *reg_state = (reg_validate_state *) entry->data;
   _mesa_hash_table_insert(reg_state->uses, _mesa_hash_pointer(state->instr),
			   state->instr, state->instr);
   
   if (!src->reg->is_global) {
      assert(reg_state->where_defined == state->impl &&
	     "using a register declared in a different function");
   }
   
   assert((src->reg->num_array_elems == 0 ||
	  src->base_offset < src->reg->num_array_elems) &&
	  "definitely out-of-bounds array access");
   
   if (src->indirect) {
      assert(src->reg->num_array_elems != 0);
      assert((src->indirect->is_ssa || src->indirect->reg.indirect == NULL) &&
	     "only one level of indirection allowed");
      validate_src(src->indirect, state);
   }
}

static void
validate_ssa_src(nir_ssa_def *def, validate_state *state)
{
   assert(def != NULL);
   
   struct hash_entry *entry = _mesa_hash_table_search(state->ssa_defs,
						      _mesa_hash_pointer(def),
						      def);
   
   assert(entry);
   
   assert((nir_function_impl *) entry->data == state->impl &&
	  "using an SSA value defined in a different function");
   
   /* TODO validate that the use is dominated by the definition */
}

static void
validate_src(nir_src *src, validate_state *state)
{
   if (src->is_ssa)
      validate_ssa_src(src->ssa, state);
   else
      validate_reg_src(&src->reg, state);
}

static void
validate_alu_src(nir_alu_src *src, validate_state *state)
{
   for (unsigned i = 0; i < 4; i++)
      assert(src->swizzle[i] < 4);
   
   validate_src(&src->src, state);
}

static void
validate_reg_dest(nir_reg_dest *dest, validate_state *state)
{
   assert(dest->reg != NULL);
   
   struct hash_entry *entry =
      _mesa_hash_table_search(dest->reg->defs, _mesa_hash_pointer(state->instr),
			      state->instr);
   assert(entry && "definition not in nir_register.defs");
   
   entry = _mesa_hash_table_search(state->regs, _mesa_hash_pointer(dest->reg),
				   dest->reg);
   
   assert(entry);
   
   reg_validate_state *reg_state = (reg_validate_state *) entry->data;
   _mesa_hash_table_insert(reg_state->defs, _mesa_hash_pointer(state->instr),
			   state->instr, state->instr);
   
   if (!dest->reg->is_global) {
      assert(reg_state->where_defined == state->impl &&
	     "writing to a register declared in a different function");
   }
   
   assert((dest->reg->num_array_elems == 0 ||
	  dest->base_offset < dest->reg->num_array_elems) &&
	  "definitely out-of-bounds array access");
   
   if (dest->indirect) {
      assert(dest->reg->num_array_elems != 0);
      assert((dest->indirect->is_ssa || dest->indirect->reg.indirect == NULL) &&
	     "only one level of indirection allowed");
      validate_src(dest->indirect, state);
   }
}

static void
validate_ssa_def(nir_ssa_def *def, validate_state *state)
{
   assert(def->num_components <= 4);
   _mesa_hash_table_insert(state->ssa_defs, _mesa_hash_pointer(def), def,
			   state->impl);
}

static void
validate_dest(nir_dest *dest, validate_state *state)
{
   if (dest->is_ssa)
      validate_ssa_def(&dest->ssa, state);
   else
      validate_reg_dest(&dest->reg, state);
}

static void
validate_alu_dest(nir_alu_dest *dest, validate_state *state)
{
   unsigned dest_size =
      dest->dest.is_ssa ? dest->dest.ssa.num_components
			: dest->dest.reg.reg->num_components;
   /*
    * validate that the instruction doesn't write to components not in the
    * register/SSA value
    */
   assert(!(dest->write_mask & ~((1 << dest_size) - 1)));
   validate_dest(&dest->dest, state);
}

static void
validate_alu_instr(nir_alu_instr *instr, validate_state *state)
{
   assert(instr->op < nir_num_opcodes);
   
   validate_alu_dest(&instr->dest, state);
   
   for (unsigned i = 0; i < nir_op_infos[instr->op].num_inputs; i++) {
      validate_alu_src(&instr->src[i], state);
   }
   
   if (instr->has_predicate)
      validate_src(&instr->predicate, state);
}

static void
validate_deref_chain(nir_deref *deref)
{
   while (deref->child != NULL) {
      switch (deref->deref_type) {
	 case nir_deref_type_array:
	    assert(deref->child->type == glsl_get_array_element(deref->type));
	    break;
	    
	 case nir_deref_type_struct:
	    assert(deref->child->type ==
		   glsl_get_struct_field(deref->type,
					 nir_deref_as_struct(deref)->elem));
	    break;
	    
	 case nir_deref_type_var:
	 default:
	    assert(0);
	    break;
      }
      
      deref = deref->child;
   }
}

static void
validate_var_use(nir_variable *var, validate_state *state)
{
   if (var->data.mode == nir_var_local) {
      struct hash_entry *entry =
	 _mesa_hash_table_search(state->var_defs, _mesa_hash_pointer(var),
				 var);
      
      assert(entry);
      assert((nir_function_impl *) entry->data == state->impl);
   }
}

static void
validate_deref_var(nir_deref_var *deref, validate_state *state)
{
   assert(deref != NULL);
   assert(deref->deref.type == deref->var->type);
   
   validate_var_use(deref->var, state);
   
   validate_deref_chain(&deref->deref);
}

static void
validate_intrinsic_instr(nir_intrinsic_instr *instr, validate_state *state)
{
   unsigned num_srcs = nir_intrinsic_infos[instr->intrinsic].num_srcs;
   for (unsigned i = 0; i < num_srcs; i++) {
      validate_src(&instr->src[i], state);
   }
   
   if (nir_intrinsic_infos[instr->intrinsic].has_dest) {
      validate_dest(&instr->dest, state);
   }
   
   unsigned num_vars = nir_intrinsic_infos[instr->intrinsic].num_variables;
   for (unsigned i = 0; i < num_vars; i++) {
      validate_deref_var(instr->variables[i], state);
   }
   
   if (instr->has_predicate)
      validate_src(&instr->predicate, state);
}

static void
validate_tex_instr(nir_tex_instr *instr, validate_state *state)
{
   validate_dest(&instr->dest, state);
   
   bool src_type_seen[nir_num_texinput_types];
   for (unsigned i = 0; i < nir_num_texinput_types; i++)
      src_type_seen[i] = false;
   
   for (unsigned i = 0; i < instr->num_srcs; i++) {
      assert(!src_type_seen[instr->src_type[i]]);
      src_type_seen[instr->src_type[i]] = true;
      validate_src(&instr->src[i], state);
   }
   
   if (instr->sampler != NULL)
      validate_deref_var(instr->sampler, state);
}

static void
validate_call_instr(nir_call_instr *instr, validate_state *state)
{
   if (instr->return_var == NULL)
      assert(glsl_type_is_void(instr->callee->return_type));
   else
      assert(instr->return_var->type == instr->callee->return_type);
   
   assert(instr->num_params == instr->callee->num_params);
   
   for (unsigned i = 0; i < instr->num_params; i++) {
      assert(instr->callee->params[i].type == instr->params[i]->type);
   }
   
   if (instr->has_predicate)
      validate_src(&instr->predicate, state);
}

static void
validate_load_const_instr(nir_load_const_instr *instr, validate_state *state)
{
   validate_dest(&instr->dest, state);
   
   if (instr->array_elems != 0) {
      assert(!instr->dest.is_ssa);
      assert(instr->dest.reg.base_offset + instr->array_elems <=
	     instr->dest.reg.reg->num_array_elems);
   }
   
   if (instr->has_predicate)
      validate_src(&instr->predicate, state);
}

static void
validate_ssa_undef_instr(nir_ssa_undef_instr *instr, validate_state *state)
{
   validate_ssa_def(&instr->def, state);
}

static void
validate_phi_instr(nir_phi_instr *instr, validate_state *state)
{
   /*
    * don't validate the sources until we get to them from their predecessor
    * basic blocks, to avoid validating an SSA use before its definition.
    */
   
   validate_dest(&instr->dest, state);
   
   unsigned size = 0;
   foreach_list(node, &instr->srcs) {
      size++;
   }
   
   assert(size == state->block->predecessors->entries);
}

static void
validate_instr(nir_instr *instr, validate_state *state)
{
   assert(instr->block == state->block);
   
   state->instr = instr;
   
   switch (instr->type) {
      case nir_instr_type_alu:
	 validate_alu_instr(nir_instr_as_alu(instr), state);
	 break;
	 
      case nir_instr_type_call:
	 validate_call_instr(nir_instr_as_call(instr), state);
	 break;
	 
      case nir_instr_type_intrinsic:
	 validate_intrinsic_instr(nir_instr_as_intrinsic(instr), state);
	 break;
	 
      case nir_instr_type_texture:
	 validate_tex_instr(nir_instr_as_texture(instr), state);
	 break;
	 
      case nir_instr_type_load_const:
	 validate_load_const_instr(nir_instr_as_load_const(instr), state);
	 break;
	 
      case nir_instr_type_phi:
	 validate_phi_instr(nir_instr_as_phi(instr), state);
	 break;
	 
      case nir_instr_type_ssa_undef:
	 validate_ssa_undef_instr(nir_instr_as_ssa_undef(instr), state);
	 break;
	 
      case nir_instr_type_jump:
	 break;
	 
      default:
	 assert(0);
	 break;
   }
}

static void
validate_phi_src(nir_phi_instr *instr, nir_block *pred, validate_state *state)
{
   foreach_list_typed(nir_phi_src, src, node, &instr->srcs) {
      if (src->pred == pred) {
	 validate_src(&src->src, state);
	 return;
      }
   }
   
   abort();
}

static void
validate_phi_srcs(nir_block *block, nir_block *succ, validate_state *state)
{
   nir_foreach_instr(succ, instr) {
      if (instr->type != nir_instr_type_phi)
	 break;
      
      validate_phi_src(nir_instr_as_phi(instr), block, state);
   }
}

static void validate_cf_node(nir_cf_node *node, validate_state *state);

static void
validate_block(nir_block *block, validate_state *state)
{
   assert(block->cf_node.parent == state->parent_node);
   
   state->block = block;
   
   nir_foreach_instr(block, instr) {
      if (instr->type == nir_instr_type_phi) {
	 assert(instr == nir_block_first_instr(block) ||
	        nir_instr_prev(instr)->type == nir_instr_type_phi);
      }
      
      if (instr->type == nir_instr_type_jump) {
	 assert(instr == nir_block_last_instr(block));
      }
      
      validate_instr(instr, state);
   }
   
   assert(block->successors[0] != NULL);
   
   for (unsigned i = 0; i < 2; i++) {
      if (block->successors[i] != NULL) {
	 struct hash_entry *entry =
	    _mesa_hash_table_search(block->successors[i]->predecessors,
				    _mesa_hash_pointer(block), block);
	 assert(entry);
	 
	 validate_phi_srcs(block, block->successors[i], state);
      }
   }
   
   if (!exec_list_is_empty(&block->instr_list) &&
       nir_block_last_instr(block)->type == nir_instr_type_jump)
      assert(block->successors[1] == NULL);
}

static void
validate_if(nir_if *if_stmt, validate_state *state)
{
   assert(!exec_node_is_head_sentinel(if_stmt->cf_node.node.prev));
   nir_cf_node *prev_node = nir_cf_node_prev(&if_stmt->cf_node);
   assert(prev_node->type == nir_cf_node_block);
   
   nir_block *prev_block = nir_cf_node_as_block(prev_node);
   assert(&prev_block->successors[0]->cf_node ==
	  nir_if_first_then_node(if_stmt));
   assert(&prev_block->successors[1]->cf_node ==
	  nir_if_first_else_node(if_stmt));
   
   assert(!exec_node_is_tail_sentinel(if_stmt->cf_node.node.next));
   nir_cf_node *next_node = nir_cf_node_next(&if_stmt->cf_node);
   assert(next_node->type == nir_cf_node_block);
   
   if (!if_stmt->condition.is_ssa) {
      nir_register *reg = if_stmt->condition.reg.reg;
      struct hash_entry *entry =
	 _mesa_hash_table_search(reg->if_uses, _mesa_hash_pointer(if_stmt),
				 if_stmt);
      assert(entry);
   }
   
   assert(!exec_list_is_empty(&if_stmt->then_list));
   assert(!exec_list_is_empty(&if_stmt->else_list));
   
   nir_cf_node *old_parent = state->parent_node;
   state->parent_node = &if_stmt->cf_node;
   
   foreach_list_typed(nir_cf_node, cf_node, node, &if_stmt->then_list) {
      validate_cf_node(cf_node, state);
   }
   
   foreach_list_typed(nir_cf_node, cf_node, node, &if_stmt->else_list) {
      validate_cf_node(cf_node, state);
   }
   
   state->parent_node = old_parent;
}

static void
validate_loop(nir_loop *loop, validate_state *state)
{
   assert(!exec_node_is_head_sentinel(loop->cf_node.node.prev));
   nir_cf_node *prev_node = nir_cf_node_prev(&loop->cf_node);
   assert(prev_node->type == nir_cf_node_block);
   
   nir_block *prev_block = nir_cf_node_as_block(prev_node);
   assert(&prev_block->successors[0]->cf_node == nir_loop_first_cf_node(loop));
   assert(prev_block->successors[1] == NULL);
   
   assert(!exec_node_is_tail_sentinel(loop->cf_node.node.next));
   nir_cf_node *next_node = nir_cf_node_next(&loop->cf_node);
   assert(next_node->type == nir_cf_node_block);
   
   assert(!exec_list_is_empty(&loop->body));
   
   nir_cf_node *old_parent = state->parent_node;
   state->parent_node = &loop->cf_node;
   
   foreach_list_typed(nir_cf_node, cf_node, node, &loop->body) {
      validate_cf_node(cf_node, state);
   }
   
   state->parent_node = old_parent;
}

static void
validate_cf_node(nir_cf_node *node, validate_state *state)
{
   assert(node->parent == state->parent_node);
   
   switch (node->type) {
      case nir_cf_node_block:
	 validate_block(nir_cf_node_as_block(node), state);
	 break;
	 
      case nir_cf_node_if:
	 validate_if(nir_cf_node_as_if(node), state);
	 break;
	 
      case nir_cf_node_loop:
	 validate_loop(nir_cf_node_as_loop(node), state);
	 break;
	 
      default:
	 assert(0);
	 break;
   }
}

static void
prevalidate_reg_decl(nir_register *reg, bool is_global, validate_state *state)
{
   assert(reg->is_global == is_global);
   
   reg_validate_state *reg_state = ralloc(state->regs, reg_validate_state);
   reg_state->uses = _mesa_hash_table_create(reg_state,
					     _mesa_key_pointer_equal);
   reg_state->defs = _mesa_hash_table_create(reg_state,
					     _mesa_key_pointer_equal);
   
   reg_state->where_defined = is_global ? NULL : state->impl;
   
   _mesa_hash_table_insert(state->regs, _mesa_hash_pointer(reg), reg,
			   reg_state);
}

static void
postvalidate_reg_decl(nir_register *reg, validate_state *state)
{
   struct hash_entry *entry = _mesa_hash_table_search(state->regs,
						      _mesa_hash_pointer(reg),
						      reg);
   
   reg_validate_state *reg_state = (reg_validate_state *) entry->data;
   
   if (reg_state->uses->entries != reg->uses->entries) {
      printf("extra entries in register uses:\n");
      struct hash_entry *entry;
      hash_table_foreach(reg->uses, entry) {
	 struct hash_entry *entry2 =
	    _mesa_hash_table_search(reg_state->uses,
				    _mesa_hash_pointer(entry->data),
				    entry->data);
	    
	 if (entry2 == NULL) {
	    printf("0x%p\n", entry->data);
	 }
      }
      
      abort();
   }
   
   if (reg_state->defs->entries != reg->defs->entries) {
      printf("extra entries in register defs:\n");
      struct hash_entry *entry;
      hash_table_foreach(reg->defs, entry) {
	 struct hash_entry *entry2 =
	    _mesa_hash_table_search(reg_state->defs,
				    _mesa_hash_pointer(entry->data),
				    entry->data);
	    
	 if (entry2 == NULL) {
	    printf("0x%p\n", entry->data);
	 }
      }
      
      abort();
   }
}

static void
validate_var_decl(nir_variable *var, bool is_global, validate_state *state)
{
   assert(is_global != (var->data.mode == nir_var_local));
   
   /*
    * TODO validate some things ir_validate.cpp does (requires more GLSL type
    * support)
    */
   
   if (!is_global) {
      _mesa_hash_table_insert(state->var_defs, _mesa_hash_pointer(var), var,
			      state->impl);
   }
}

static void
validate_function_impl(nir_function_impl *impl, validate_state *state)
{
   assert(impl->overload->impl == impl);
   assert(impl->cf_node.parent == NULL);
   
   assert(impl->num_params == impl->overload->num_params);
   for (unsigned i = 0; i < impl->num_params; i++)
      assert(impl->params[i]->type == impl->overload->params[i].type);
   
   if (glsl_type_is_void(impl->overload->return_type))
      assert(impl->return_var == NULL);
   else
      assert(impl->return_var->type == impl->overload->return_type);
   
   assert(exec_list_is_empty(&impl->end_block->instr_list));
   assert(impl->end_block->successors[0] == NULL);
   assert(impl->end_block->successors[1] == NULL);
   
   state->impl = impl;
   state->parent_node = &impl->cf_node;
   
   foreach_list_typed(nir_variable, var, node, &impl->locals) {
      validate_var_decl(var, false, state);
   }
   
   foreach_list_typed(nir_register, reg, node, &impl->registers) {
      prevalidate_reg_decl(reg, false, state);
   }
   
   foreach_list_typed(nir_cf_node, node, node, &impl->body) {
      validate_cf_node(node, state);
   }
   
   foreach_list_typed(nir_register, reg, node, &impl->registers) {
      postvalidate_reg_decl(reg, state);
   }
}

static void
validate_function_overload(nir_function_overload *overload,
			   validate_state *state)
{
   if (overload->impl != NULL)
      validate_function_impl(overload->impl, state);
}

static void
validate_function(nir_function *func, validate_state *state)
{
   foreach_list_typed(nir_function_overload, overload, node, &func->overload_list) {
      assert(overload->function == func);
      validate_function_overload(overload, state);
   }
}

static void
init_validate_state(validate_state *state)
{
   state->regs = _mesa_hash_table_create(NULL, _mesa_key_pointer_equal);
   state->ssa_defs = _mesa_hash_table_create(NULL, _mesa_key_pointer_equal);
   state->var_defs = _mesa_hash_table_create(NULL, _mesa_key_pointer_equal);
}

static void
destroy_validate_state(validate_state *state)
{
   _mesa_hash_table_destroy(state->regs, NULL);
   _mesa_hash_table_destroy(state->ssa_defs, NULL);
   _mesa_hash_table_destroy(state->var_defs, NULL);
}

void
nir_validate_shader(nir_shader *shader)
{
   validate_state state;
   init_validate_state(&state);
   
   struct hash_entry *entry;
   hash_table_foreach(shader->uniforms, entry) {
      validate_var_decl((nir_variable *) entry->data, true, &state);
   }
   
   hash_table_foreach(shader->inputs, entry) {
     validate_var_decl((nir_variable *) entry->data, true, &state);
   }
   
   hash_table_foreach(shader->outputs, entry) {
      validate_var_decl((nir_variable *) entry->data, true, &state);
   }
   
   hash_table_foreach(shader->globals, entry) {
     validate_var_decl((nir_variable *) entry->data, true, &state);
   }
   
   foreach_list_typed(nir_register, reg, node, &shader->registers) {
      prevalidate_reg_decl(reg, true, &state);
   }
   
   foreach_list_typed(nir_function, func, node, &shader->functions) {
      validate_function(func, &state);
   }
   
   foreach_list_typed(nir_register, reg, node, &shader->registers) {
      postvalidate_reg_decl(reg, &state);
   }
   
   destroy_validate_state(&state);
}
