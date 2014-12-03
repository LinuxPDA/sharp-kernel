#ifndef _RADIX_H_
#define _RADIX_H_

#include <asm/atomic.h>

#define RADIX_INIT(head)						\
{									\
  (head)->RdX_root = NULL;						\
}

#define RADIX_ENTRY(type,bits)						\
struct {								\
  struct type *RdX_next[(0x01 << bits)];				\
}

#define RADIX_HEAD(name,type)						\
struct name {								\
  struct type *RdX_root;						\
};

#define RADIX_INSERT(head,type,field,bits,tdef,key,klen,result)		\
{									\
  struct type **RdX_node = &((head)->RdX_root);				\
  tdef RdX_mask;							\
  int RdX_index;							\
  int RdX_size = sizeof(tdef)*8;					\
  char RdX_status = 0;							\
  int RdX_count = 0;							\
									\
  if(klen % bits) result = -1;						\
  else {								\
    RdX_mask = (0x01 << bits)-1;					\
    while((RdX_count*bits) < klen) {					\
      RdX_status = 1;							\
      if(!(*RdX_node)) {						\
         RdX_status = 2;						\
         (*RdX_node) = (struct type *)kmalloc(sizeof(struct type),	\
           GFP_KERNEL);							\
         if(!(*RdX_node)) {						\
           result = -2;							\
           break;							\
         }								\
         RdX_status = 3;						\
         memset((*RdX_node),0,sizeof(struct type));			\
         RdX_status = 4;						\
      }									\
      RdX_index = (key >> (RdX_size - bits - (RdX_count * bits))) & RdX_mask; \
      RdX_status = 5;							\
      RdX_node = &((*RdX_node)->field.RdX_next[RdX_index]);		\
      RdX_status = 6;							\
      RdX_count++;							\
      result = 0;							\
    }									\
    if(result != -2 && (!(*RdX_node))) {				\
      (*RdX_node) = (struct type *)kmalloc(sizeof(struct type),		\
        GFP_KERNEL);							\
      if(!(*RdX_node)) {						\
        result = -2;							\
      } else {								\
        memset((*RdX_node),0,sizeof(struct type));			\
      }									\
    }									\
  }									\
}

#define RADIX_DO(head,type,field,bits,tdef,key,klen,flm,elem,result,RxD_set) \
{									\
  struct type *RdX_node = (head)->RdX_root;				\
  tdef RdX_mask;							\
  int RdX_index;							\
  int RdX_size = sizeof(tdef)*8;					\
  int RdX_count = 0;							\
									\
  if(klen % bits) result = -1;						\
  else {								\
    RdX_mask = (0x01 << bits)-1;					\
    while((RdX_count*bits) < klen) {					\
      if(!RdX_node) {							\
        result = -2;							\
        break;								\
      } else {								\
        RdX_index = (key >> (RdX_size - bits - (RdX_count * bits))) & RdX_mask;\
        RdX_node = RdX_node->field.RdX_next[RdX_index];			\
      }									\
      RdX_count++;							\
    }									\
    if(!RdX_node) {							\
      result = -3;							\
    } else {								\
      if(RxD_set) {							\
        RdX_node->flm = elem;						\
      } else {								\
        elem = RdX_node->flm;						\
      }									\
      result = 0;							\
    }									\
  }									\
}

#define RADIX_SET(head,type,field,bits,tdef,key,klen,flm,elem,result)	\
        RADIX_DO(head,type,field,bits,tdef,key,klen,flm,elem,result,1)
#define RADIX_GET(head,type,field,bits,tdef,key,klen,flm,elem,result)	\
        RADIX_DO(head,type,field,bits,tdef,key,klen,flm,elem,result,0)

#define RADIX_VISIT_ALL(head,type,field,bits,visit,extra)		\
{									\
  int RdX_node_count = (0x1 << bits);					\
  struct type *RdX_stack[RdX_node_count];				\
  int RdX_stack_count[RdX_node_count];					\
  int RdX_stack_depth = 0;						\
  struct type *RdX_node;						\
  int RdX_done = 0;							\
  int R = 0;								\
									\
  if((RdX_node = (head)->RdX_root)) {					\
    while(!RdX_done) {							\
      if(RdX_node->field.RdX_next[R]) {					\
        RdX_stack[RdX_stack_depth] = RdX_node;				\
        RdX_stack_count[RdX_stack_depth] = R + 1;			\
        RdX_stack_depth++;						\
        RdX_node = RdX_node->field.RdX_next[R];				\
        R = 0;								\
      } else {								\
        R++;								\
        if(R >= RdX_node_count) {					\
          if(visit(RdX_node,extra)) {					\
            RdX_done = 1;						\
            break;							\
          }								\
          RdX_stack_depth--;						\
          if(RdX_stack_depth >= 0) {					\
            RdX_node = RdX_stack[RdX_stack_depth];			\
            R = RdX_stack_count[RdX_stack_depth];			\
          } else {							\
            RdX_done = 1;						\
          }								\
        }								\
      }									\
    }									\
  }									\
}

#endif
