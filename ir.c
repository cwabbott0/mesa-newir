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
#include <assert.h>

nir_shader *
nir_shader_create(void *mem_ctx)
{
   nir_shader *shader = ralloc(mem_ctx, nir_shader);
   
   shader->uniforms = _mesa_hash_table_create(shader, _mesa_key_string_equal);
   shader->inputs = _mesa_hash_table_create(shader, _mesa_key_string_equal);
   shader->outputs = _mesa_hash_table_create(shader, _mesa_key_string_equal);
   shader->globals = _mesa_hash_table_create(shader, _mesa_key_string_equal);
   
   exec_list_make_empty(&shader->functions);
   exec_list_make_empty(&shader->registers);
   shader->reg_alloc = 0;
   
   return shader;
}

nir_register *
nir_global_reg_create(nir_shader *shader)
{
   nir_register *reg = ralloc(shader, nir_register);
   
   reg->uses = _mesa_hash_table_create(shader, _mesa_key_pointer_equal);
   reg->defs = _mesa_hash_table_create(shader, _mesa_key_pointer_equal);
   
   reg->is_global = true;
   reg->num_components = 0;
   reg->num_array_elems = 0;
   reg->index = shader->reg_alloc++;
   reg->name = NULL;
   
   return reg;
}

nir_function *
nir_function_create(nir_shader *shader, const char *name)
{
   nir_function *func = ralloc(shader, nir_function);
   
   exec_list_push_tail(&shader->functions, &func->node);
   exec_list_make_empty(&func->overload_list);
   func->name = name;
   
   return func;
}

nir_function_overload *
nir_function_overload_create(nir_function *func)
{
   void *mem_ctx = ralloc_parent(func);
   
   nir_function_overload *overload = ralloc(mem_ctx, nir_function_overload);
   
   exec_list_make_empty(&overload->param_list);
   overload->return_type = NULL;
   
   exec_list_push_tail(&func->overload_list, &overload->node);
   overload->function = func;
   
   return overload;
}

static inline void
block_add_pred(nir_block *block, nir_block *pred)
{
   _mesa_hash_table_insert(block->predecessors, _mesa_hash_pointer(pred),
			   pred, pred);
}

static void
cf_init(nir_cf_node *node, nir_cf_node_type type)
{
   exec_node_init(&node->node);
   node->parent = NULL;
   node->type = type;
}

static void
link_blocks(nir_block *pred, nir_block *succ1, nir_block *succ2)
{
   pred->successors[0] = succ1;
   block_add_pred(succ1, pred);
   
   pred->successors[1] = succ2;
   if (succ2 != NULL)
      block_add_pred(succ2, pred);
}

static void
unlink_blocks(nir_block *pred, nir_block *succ)
{
   if (pred->successors[0] == succ) {
      pred->successors[0] = pred->successors[1];
      pred->successors[1] = NULL;
   } else {
      assert(pred->successors[1] == succ);
      pred->successors[1] = NULL;
   }
   
   struct hash_entry *entry = _mesa_hash_table_search(succ->predecessors,
						      _mesa_hash_pointer(pred),
						      pred);
   
   assert(entry);
   
   _mesa_hash_table_remove(succ->predecessors, entry);
}

static void
unlink_block_successors(nir_block *block)
{
   if (block->successors[0] != NULL)
      unlink_blocks(block, block->successors[0]);
   if (block->successors[1] != NULL)
      unlink_blocks(block, block->successors[1]);
}


nir_function_impl *
nir_function_impl_create(nir_function_overload *overload)
{
   assert(overload->impl == NULL);
   
   void *mem_ctx = ralloc_parent(overload);
   
   nir_function_impl *impl = ralloc(mem_ctx, nir_function_impl);
   
   overload->impl = impl;
   impl->overload = overload;
   
   cf_init(&impl->cf_node, nir_cf_node_function);
   
   exec_list_make_empty(&impl->body);
   exec_list_make_empty(&impl->registers);
   exec_list_make_empty(&impl->locals);
   exec_list_make_empty(&impl->parameters);
   impl->return_var = NULL;
   impl->reg_alloc = 0;
   
   /* create start & end blocks */
   nir_block *start_block = nir_block_create(mem_ctx);
   nir_block *end_block = nir_block_create(mem_ctx);
   start_block->cf_node.parent = &impl->cf_node;
   end_block->cf_node.parent = &impl->cf_node;
   impl->start_block = start_block;
   impl->end_block = end_block;
   
   exec_list_push_tail(&impl->body, &start_block->cf_node.node);
   
   start_block->successors[0] = end_block;
   block_add_pred(end_block, start_block);
   
   return impl;
}

nir_block *
nir_block_create(void *mem_ctx)
{
   nir_block *block = ralloc(mem_ctx, nir_block);
   
   cf_init(&block->cf_node, nir_cf_node_block);
   
   block->successors[0] = block->successors[1] = NULL;
   block->predecessors = _mesa_hash_table_create(mem_ctx,
						 _mesa_key_pointer_equal);
   
   exec_list_make_empty(&block->instr_list);
   
   return block;
}

static inline void
src_init(nir_src *src)
{
   src->is_ssa = false;
   src->src.reg.reg = NULL;
   src->src.reg.indirect = NULL;
   src->src.reg.base_offset = 0;
}

nir_if *
nir_if_create(void *mem_ctx)
{
   nir_if *if_stmt = ralloc(mem_ctx, nir_if);
   
   cf_init(&if_stmt->cf_node, nir_cf_node_if);
   src_init(&if_stmt->condition);
   
   nir_block *then = nir_block_create(mem_ctx);
   exec_list_make_empty(&if_stmt->then_list);
   exec_list_push_tail(&if_stmt->then_list, &then->cf_node.node);
   then->cf_node.parent = &if_stmt->cf_node;
   
   nir_block *else_stmt = nir_block_create(mem_ctx);
   exec_list_make_empty(&if_stmt->else_list);
   exec_list_push_tail(&if_stmt->else_list, &else_stmt->cf_node.node);
   else_stmt->cf_node.parent = &if_stmt->cf_node;
   
   return if_stmt;
}

nir_loop *
nir_loop_create(void *mem_ctx)
{
   nir_loop *loop = ralloc(mem_ctx, nir_loop);
   
   cf_init(&loop->cf_node, nir_cf_node_loop);
   
   nir_block *body = nir_block_create(mem_ctx);
   exec_list_make_empty(&loop->body);
   exec_list_push_tail(&loop->body, &body->cf_node.node);
   body->cf_node.parent = &loop->cf_node;
   
   body->successors[0] = body;
   block_add_pred(body, body);
   
   return loop;
}

static void
instr_init(nir_instr *instr, nir_instr_type type)
{
   instr->type = type;
   instr->block = NULL;
   exec_node_init(&instr->node);
}

static void
dest_init(nir_dest *dest)
{
   dest->is_ssa = false;
   dest->dest.reg.reg = NULL;
   dest->dest.reg.indirect = NULL;
   dest->dest.reg.base_offset = 0;
}

static void
alu_dest_init(nir_alu_dest *dest)
{
   dest_init(&dest->dest);
   dest->saturate = false;
   dest->write_mask = 0xf;
}

static void
alu_src_init(nir_alu_src *src)
{
   src_init(&src->src);
   src->abs = src->negate = false;
   src->swizzle[0] = 0;
   src->swizzle[1] = 1;
   src->swizzle[2] = 2;
   src->swizzle[3] = 3;
}

nir_alu_instr *
nir_alu_instr_create(void *mem_ctx, nir_op op)
{
   unsigned num_srcs = nir_op_infos[op].num_inputs;
   nir_alu_instr *instr =
      ralloc_size(mem_ctx,
		  sizeof(nir_alu_instr) + num_srcs * sizeof(nir_alu_src));
   
   instr_init(&instr->instr, nir_instr_type_alu);
   instr->op = op;
   alu_dest_init(&instr->dest);
   for (unsigned i = 0; i < num_srcs; i++)
      alu_src_init(&instr->src[i]);
   
   return instr;
}

nir_jump_instr *
nir_jump_instr_create(void *mem_ctx, nir_jump_type type)
{
   nir_jump_instr *instr = ralloc(mem_ctx, nir_jump_instr);
   instr_init(&instr->instr, nir_instr_type_jump);
   instr->type = type;
   return instr;
}

/**
 * \name Control flow modification
 * 
 * These functions modify the control flow tree while keeping the control flow
 * graph up-to-date. The invariants respected are:
 * 1. Each then statement, else statement, or loop body must have at least one
 *    control flow node.
 * 2. Each if-statement and loop must have one basic block before it and one
 *    after.
 * 3. Two basic blocks cannot be directly next to each other.
 * 4. If a basic block has a jump instruction, there must be only one and it
 *    must be at the end of the block.
 * 
 * The purpose of the second one is so that we have places to insert code during
 * GCM, as well as eliminating the possibility of critical edges.
 */
/*@{*/

static void
link_non_block_to_block(nir_cf_node *node, nir_block *block)
{
   if (node->type == nir_cf_node_if) {
      /*
       * We're trying to link an if to a block after it; this just means linking
       * the last block of the then and else branches.
       */
      
      nir_if *if_stmt = nir_cf_node_as_if(node);
      
      nir_cf_node *last_then = nir_if_last_then_node(if_stmt);
      assert(last_then->type == nir_cf_node_block);
      nir_block *last_then_block = nir_cf_node_as_block(last_then);
      
      nir_cf_node *last_else = nir_if_last_else_node(if_stmt);
      assert(last_else->type == nir_cf_node_block);
      nir_block *last_else_block = nir_cf_node_as_block(last_else);
      
      if (exec_list_is_empty(&last_then_block->instr_list) ||
	  nir_block_last_instr(last_then_block)->type != nir_instr_type_jump) {
	 unlink_block_successors(last_then_block);
	 link_blocks(last_then_block, block, NULL);
      }
      
      if (exec_list_is_empty(&last_else_block->instr_list) ||
	  nir_block_last_instr(last_else_block)->type != nir_instr_type_jump) {
	 unlink_block_successors(last_else_block);
	 link_blocks(last_else_block, block, NULL);
      }
   } else {
      assert(node->type == nir_cf_node_loop);
      
      /*
       * We're trying to link a loop to a block after it; there isn't much we
       * can do, since this would entail rewriting every block in the loop that
       * ends in a break to point to the new basic block. So we'll just assume
       * that the loop is newly-created, and not do anything - this assumption
       * is reasonable, since it doesn't make much sense to call this function
       * in another situation anywyays.
       */
      exec_node_insert_after(&node->node, &block->cf_node.node);
   }
}

static void
link_block_to_non_block(nir_block *block, nir_cf_node *node)
{
   if (node->type == nir_cf_node_if) {
      /*
       * We're trying to link a block to an if after it; this just means linking
       * the block to the first block of the then and else branches.
       */
      
      nir_if *if_stmt = nir_cf_node_as_if(node);
      
      nir_cf_node *first_then = nir_if_first_then_node(if_stmt);
      assert(first_then->type == nir_cf_node_block);
      nir_block *first_then_block = nir_cf_node_as_block(first_then);
      
      nir_cf_node *first_else = nir_if_first_else_node(if_stmt);
      assert(first_else->type == nir_cf_node_block);
      nir_block *first_else_block = nir_cf_node_as_block(first_else);
      
      unlink_block_successors(block);
      link_blocks(block, first_then_block, first_else_block);
   } else {
      /*
       * For similar reasons as the corresponding case in
       * link_non_block_to_block(), don't worry about if the loop header has
       * any predecessors that need to be unlinked.
       */
      
      assert(node->type == nir_cf_node_loop);
      
      nir_loop *loop = nir_cf_node_as_loop(node);
      
      nir_cf_node *loop_header = nir_loop_first_cf_node(loop);
      assert(loop_header->type == nir_cf_node_block);
      nir_block *loop_header_block = nir_cf_node_as_block(loop_header);
      
      unlink_block_successors(block);
      link_blocks(block, loop_header_block, NULL);
   }
	 
}

/**
 * Takes a basic block and inserts a new empty basic block before it, making its
 * predecessors point to the new block. This essentially splits the block into
 * an empty header and a body so that another non-block CF node can be inserted
 * between the two. Note that this does *not* link the two basic blocks, so
 * some kind of cleanup *must* be performed after this call.
 */

static nir_block *
split_block_beginning(nir_block *block)
{
   nir_block *new_block = nir_block_create(ralloc_parent(block));
   exec_node_insert_node_before(&block->cf_node.node, &new_block->cf_node.node);
   
   struct hash_entry *entry;
   hash_table_foreach(block->predecessors, entry) {
      nir_block *pred = (nir_block *) entry->data;
      
      unlink_blocks(pred, block);
      link_blocks(pred, new_block, NULL);
   }
   
   return new_block;
}

/**
 * Moves the successors of source to the successors of dest, leaving both
 * successors of source NULL.
 */

static void
move_successors(nir_block *source, nir_block *dest)
{
   nir_block *succ1 = source->successors[0];
   if (succ1)
      unlink_blocks(source, succ1);
   
   nir_block *succ2 = source->successors[1];
   if (succ2)
      unlink_blocks(source, succ2);
   
   unlink_block_successors(dest);
   link_blocks(dest, succ1, succ2);
}

static nir_block *
split_block_end(nir_block *block)
{
   nir_block *new_block = nir_block_create(ralloc_parent(block));
   exec_node_insert_after(&block->cf_node.node, &new_block->cf_node.node);
   
   move_successors(block, new_block);
   
   return new_block;
}

/**
 * Inserts a non-basic block between two basic blocks and links them together.
 */

static void
insert_non_block(nir_block *before, nir_cf_node *node, nir_block *after)
{
   exec_node_insert_after(&before->cf_node.node, &node->node);
   link_block_to_non_block(before, node);
   link_non_block_to_block(node, after);
}

/**
 * Inserts a non-basic block before a basic block.
 */

static void
insert_non_block_before_block(nir_cf_node *node, nir_block *block)
{
   /* split off the beginning of block into new_block */
   nir_block *new_block = split_block_beginning(block);
   
   /* insert our node in between new_block and block */
   insert_non_block(new_block, node, block);
}

static void
insert_non_block_after_block(nir_block *block, nir_cf_node *node)
{
   /* split off the end of block into new_block */
   nir_block *new_block = split_block_end(block);
   
   /* insert our node in between block and new_block */
   insert_non_block(block, node, new_block);
}

/* walk up the control flow tree to find the innermost enclosed loop */
static nir_loop *
nearest_loop(nir_cf_node *node)
{
   while (node->type != nir_cf_node_loop) {
      node = node->parent;
   }
   
   return nir_cf_node_as_loop(node);
}

static nir_function_impl *
get_function(nir_cf_node *node)
{
   while (node->type != nir_cf_node_function) {
      node = node->parent;
   }
   
   return nir_cf_node_as_function(node);
}

/*
 * update the CFG after a jump instruction has been added to the end of a block
 */

static void
handle_jump(nir_block *block)
{
   nir_instr *instr = nir_block_last_instr(block);
   nir_jump_instr *jump_instr = nir_instr_as_jump(instr);
   
   unlink_block_successors(block);
   
   if (jump_instr->type == nir_jump_break ||
       jump_instr->type == nir_jump_continue) {
      nir_loop *loop = nearest_loop(&block->cf_node);
   
      if (jump_instr->type == nir_jump_break) {
	 nir_cf_node *first_node = nir_loop_first_cf_node(loop);
	 assert(first_node->type == nir_cf_node_block);
	 nir_block *first_block = nir_cf_node_as_block(first_node);
	 link_blocks(block, first_block, NULL);
      } else {
	 nir_cf_node *after = nir_cf_node_next(&loop->cf_node);
	 assert(after->type == nir_cf_node_block);
	 nir_block *after_block = nir_cf_node_as_block(after);
	 link_blocks(block, after_block, NULL);
      }
   } else {
      nir_function_impl *impl = get_function(&block->cf_node);
      link_blocks(block, impl->end_block, NULL);
   }
}

/**
 * Inserts a basic block before another by merging the instructions.
 * 
 * @param block the target of the insertion
 * @param before the block to be inserted - must not have been inserted before
 * @param has_jump whether \before has a jump instruction at the end
 */

static void
insert_block_before_block(nir_block *block, nir_block *before, bool has_jump)
{
   assert(!has_jump || exec_list_is_empty(&block->instr_list));
   
   foreach_list_typed(nir_instr, instr, node, &before->instr_list) {
      instr->block = block;
   }
   
   exec_list_prepend(&block->instr_list, &before->instr_list);
   
   if (has_jump)
      handle_jump(block);
}

/**
 * Inserts a basic block after another by merging the instructions.
 * 
 * @param block the target of the insertion
 * @param after the block to be inserted - must not have been inserted before
 * @param has_jump whether \after has a jump instruction at the end
 */

static void
insert_block_after_block(nir_block *block, nir_block *after, bool has_jump)
{  
   foreach_list_typed(nir_instr, instr, node, &after->instr_list) {
      instr->block = block;
   }
   
   exec_list_append(&block->instr_list, &after->instr_list);
   
   if (has_jump)
      handle_jump(block);
}

void
nir_cf_node_insert_after(nir_cf_node *node, nir_cf_node *after)
{
   if (after->type == nir_cf_node_block) {
      /*
       * either node or the one after it must be a basic block, by invariant #2;
       * in either case, just merge the blocks together.
       */
      nir_block *after_block = nir_cf_node_as_block(after);
      
      bool has_jump = !exec_list_is_empty(&after_block->instr_list) &&
	 nir_block_last_instr(after_block)->type == nir_instr_type_jump;
	 
      if (node->type == nir_cf_node_block) {
	 insert_block_after_block(nir_cf_node_as_block(node), after_block,
				  has_jump);
      } else {
	 nir_cf_node *next = nir_cf_node_next(node);
	 assert(next->type == nir_cf_node_block);
	 nir_block *next_block = nir_cf_node_as_block(next);
	 
	 insert_block_before_block(next_block, after_block, has_jump);
      }
   } else {
      if (node->type == nir_cf_node_block) {
	 insert_non_block_after_block(nir_cf_node_as_block(node), after);
      } else {
	 /*
	  * We have to insert a non-basic block after a non-basic block. Since
	  * every non-basic block has a basic block after it, this is equivalent
	  * to inserting a non-basic block before a basic block.
	  */
	 
	 nir_cf_node *next = nir_cf_node_next(node);
	 assert(next->type == nir_cf_node_block);
	 nir_block *next_block = nir_cf_node_as_block(next);
	 
	 insert_non_block_before_block(after, next_block);
      }
   }
}

void
nir_cf_node_insert_before(nir_cf_node *node, nir_cf_node *before)
{
   if (before->type == nir_cf_node_block) {
      nir_block *before_block = nir_cf_node_as_block(before);
      
      bool has_jump = !exec_list_is_empty(&before_block->instr_list) &&
	 nir_block_last_instr(before_block)->type == nir_instr_type_jump;
      
      if (node->type == nir_cf_node_block) {
	 insert_block_before_block(nir_cf_node_as_block(node), before_block,
				   has_jump);
      } else {
	 nir_cf_node *prev = nir_cf_node_prev(node);
	 assert(prev->type == nir_cf_node_block);
	 nir_block *prev_block = nir_cf_node_as_block(prev);
	 
	 insert_block_after_block(prev_block, before_block, has_jump);
      }
   } else {
      if (node->type == nir_cf_node_block) {
	 insert_non_block_before_block(before, nir_cf_node_as_block(node));
      } else {
	 /*
	  * We have to insert a non-basic block before a non-basic block. This
	  * is equivalent to inserting a non-basic block after a basic block.
	  */
	 
	 nir_cf_node *prev_node = nir_cf_node_prev(node);
	 assert(prev_node->type == nir_cf_node_block);
	 nir_block *prev_block = nir_cf_node_as_block(prev_node);
	 
	 insert_non_block_after_block(prev_block, before);
      }
   }
}

void
nir_cf_node_insert_begin(struct exec_list *list, nir_cf_node *node)
{
   nir_cf_node *begin = exec_node_data(nir_cf_node, list->head, node);
   nir_cf_node_insert_before(begin, node);
}

void
nir_cf_node_insert_end(struct exec_list *list, nir_cf_node *node)
{
   nir_cf_node *end = exec_node_data(nir_cf_node, list->tail_pred, node);
   nir_cf_node_insert_after(end, node);
}

/**
 * Stitch two basic blocks together into one. The aggregate must have the same
 * predecessors as the first and the same successors as the second.
 */

static void
stitch_blocks(nir_block *before, nir_block *after)
{
   /*
    * We move after into before, so we have to deal with up to 2 successors vs.
    * possibly a large number of predecessors.
    * 
    * TODO: special case when before is empty and after isn't?
    */
   
   move_successors(after, before);
   
   foreach_list_typed(nir_instr, instr, node, &after->instr_list) {
      instr->block = before;
   }
   
   exec_list_append(&before->instr_list, &after->instr_list);
   exec_node_remove(&after->cf_node.node);
}

void
nir_cf_node_remove(nir_cf_node *node)
{
   if (node->type == nir_cf_node_block) {
      /*
       * Basic blocks can't really be removed by themselves, since they act as
       * padding between the non-basic blocks. So all we do here is empty the
       * block of instructions.
       * 
       * TODO: could we assert here?
       */
      exec_list_make_empty(&nir_cf_node_as_block(node)->instr_list);
   } else {
      nir_cf_node *before = nir_cf_node_prev(node);
      assert(before->type == nir_cf_node_block);
      nir_block *before_block = nir_cf_node_as_block(before);
      
      nir_cf_node *after = nir_cf_node_next(node);
      assert(after->type == nir_cf_node_block);
      nir_block *after_block = nir_cf_node_as_block(after);
      
      exec_node_remove(&node->node);
      stitch_blocks(before_block, after_block);
   }  
}

static void
add_use(nir_src *src, nir_instr *instr)
{
   if (src->is_ssa)
      return;
   
   nir_register *reg = src->src.reg.reg;
   
   _mesa_hash_table_insert(reg->uses, _mesa_hash_pointer(instr), instr, instr);
   
   if (src->src.reg.indirect != NULL)
      add_use(src->src.reg.indirect, instr);
}

static void
add_def(nir_dest *dest, nir_instr *instr)
{
   if (dest->is_ssa)
      return;
   
   nir_register *reg = dest->dest.reg.reg;
   
   _mesa_hash_table_insert(reg->defs, _mesa_hash_pointer(instr), instr, instr);
   
   if (dest->dest.reg.indirect != NULL)
      add_use(dest->dest.reg.indirect, instr);
}

static void
add_defs_uses_alu(nir_alu_instr *instr)
{
   add_def(&instr->dest.dest, &instr->instr);
   for (unsigned i = 0; i < nir_op_infos[instr->op].num_inputs; i++)
      add_use(&instr->src[i].src, &instr->instr);
}

static void
add_defs_uses_intrinsic(nir_intrinsic_instr *instr)
{
   unsigned num_inputs = nir_intrinsic_infos[instr->intrinsic].num_reg_inputs;
   for (unsigned i = 0; i < num_inputs; i++)
      add_use(&instr->reg_inputs[i], &instr->instr);
   
   unsigned num_outputs = nir_intrinsic_infos[instr->intrinsic].num_reg_outputs;
   for (unsigned i = 0; i < num_outputs; i++)
      add_def(&instr->reg_outputs[i], &instr->instr);
}

static void
add_defs_uses_load_const(nir_load_const_instr *instr)
{
   add_def(&instr->dest, &instr->instr);
}

static void
add_defs_uses(nir_instr *instr)
{
   switch (instr->type) {
      case nir_instr_type_alu:
	 add_defs_uses_alu(nir_instr_as_alu(instr));
	 break;
	 
      case nir_instr_type_intrinsic:
	 add_defs_uses_intrinsic(nir_instr_as_intrinsic(instr));
	 break;
      case nir_instr_type_load_const:
	 add_defs_uses_load_const(nir_instr_as_load_const(instr));
	 break;
	 
      default:
	 break;
   }
}

void
nir_instr_insert_before(nir_instr *instr, nir_instr *before)
{
   before->block = instr->block;
   add_defs_uses(before);
   exec_node_insert_node_before(&instr->node, &before->node);
}

void
nir_instr_insert_after(nir_instr *instr, nir_instr *after)
{
   after->block = instr->block;
   add_defs_uses(after);
   exec_node_insert_after(&instr->node, &after->node);
}

void
nir_instr_insert_before_block(nir_block *block, nir_instr *before)
{
   before->block = block;
   add_defs_uses(before);
   exec_node_insert_after((struct exec_node *) &block->instr_list.head,
			  &before->node);
}

void
nir_instr_insert_after_block(nir_block *block, nir_instr *after)
{
   after->block = block;
   add_defs_uses(after);
   exec_node_insert_node_before((struct exec_node *) &block->instr_list.tail,
				&after->node);
}

void
nir_instr_insert_before_cf(nir_cf_node *node, nir_instr *before)
{
   if (node->type == nir_cf_node_block) {
      nir_instr_insert_before_block(nir_cf_node_as_block(node), before);
   } else {
      nir_cf_node *next = nir_cf_node_next(node);
      assert(next->type == nir_cf_node_block);
      nir_block *next_block = nir_cf_node_as_block(next);
      
      nir_instr_insert_after_block(next_block, before);
   }
}

void
nir_instr_insert_after_cf(nir_cf_node *node, nir_instr *after)
{
      if (node->type == nir_cf_node_block) {
      nir_instr_insert_after_block(nir_cf_node_as_block(node), after);
   } else {
      nir_cf_node *prev = nir_cf_node_prev(node);
      assert(prev->type == nir_cf_node_block);
      nir_block *prev_block = nir_cf_node_as_block(prev);
      
      nir_instr_insert_before_block(prev_block, after);
   }
}

void
nir_instr_insert_before_cf_list(struct exec_list *list, nir_instr *before)
{
   nir_cf_node *first_node = exec_node_data(nir_cf_node,
					    exec_list_get_head(list), node);
   nir_instr_insert_before_cf(first_node, before);
}

void
nir_instr_insert_after_cf_list(struct exec_list *list, nir_instr *after)
{
   nir_cf_node *last_node = exec_node_data(nir_cf_node,
					   exec_list_get_tail(list), node);
   nir_instr_insert_after_cf(last_node, after);
}

static void
remove_use(nir_src *src, nir_instr *instr)
{
   if (src->is_ssa)
      return;
   
   nir_register *reg = src->src.reg.reg;
   
   struct hash_entry *entry = _mesa_hash_table_search(reg->uses,
						      _mesa_hash_pointer(instr),
						      instr);
   if (entry)
      _mesa_hash_table_remove(reg->uses, entry);
   
   if (src->src.reg.indirect != NULL)
      add_use(src->src.reg.indirect, instr);
}

static void
remove_def(nir_dest *dest, nir_instr *instr)
{
   if (dest->is_ssa)
      return;
   
   nir_register *reg = dest->dest.reg.reg;
   
   struct hash_entry *entry = _mesa_hash_table_search(reg->defs,
						      _mesa_hash_pointer(instr),
						      instr);
   if (entry)
      _mesa_hash_table_remove(reg->defs, entry);
   
   if (dest->dest.reg.indirect != NULL)
      add_use(dest->dest.reg.indirect, instr);
}

static void
remove_defs_uses_alu(nir_alu_instr *instr)
{
   remove_def(&instr->dest.dest, &instr->instr);
   for (unsigned i = 0; i < nir_op_infos[instr->op].num_inputs; i++)
      remove_use(&instr->src[i].src, &instr->instr);
}

static void
remove_defs_uses_intrinsic(nir_intrinsic_instr *instr)
{
   unsigned num_inputs = nir_intrinsic_infos[instr->intrinsic].num_reg_inputs;
   for (unsigned i = 0; i < num_inputs; i++)
      remove_use(&instr->reg_inputs[i], &instr->instr);
   
   unsigned num_outputs = nir_intrinsic_infos[instr->intrinsic].num_reg_outputs;
   for (unsigned i = 0; i < num_outputs; i++)
      remove_def(&instr->reg_outputs[i], &instr->instr);
}

static void
remove_defs_uses_load_const(nir_load_const_instr *instr)
{
   remove_def(&instr->dest, &instr->instr);
}

static void
remove_defs_uses(nir_instr *instr)
{
   switch (instr->type) {
      case nir_instr_type_alu:
	 remove_defs_uses_alu(nir_instr_as_alu(instr));
	 break;
	 
      case nir_instr_type_intrinsic:
	 remove_defs_uses_intrinsic(nir_instr_as_intrinsic(instr));
	 break;
	 
      case nir_instr_type_load_const:
	 remove_defs_uses_load_const(nir_instr_as_load_const(instr));
	 break;
	 
      default:
	 break;
   }
}

void nir_instr_remove(nir_instr *instr)
{
   remove_defs_uses(instr);
   exec_node_remove(&instr->node);
}

/*@}*/
