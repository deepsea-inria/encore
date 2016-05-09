
#define SAVE_CONTEXT \
        mov     %rsp, 0x00(%rdi); \
        mov     %r15, 0x08(%rdi); \
        mov     %r14, 0x10(%rdi); \
        mov     %r13, 0x18(%rdi); \
        mov     %r12, 0x20(%rdi); \
        mov     %rbx, 0x28(%rdi); \
        mov     %rbp, 0x30(%rdi);

#define LOAD_CONTEXT \
        mov     0x00(%rsi), %rsp; \
        mov     0x08(%rsi), %r15; \
        mov     0x10(%rsi), %r14; \
        mov     0x18(%rsi), %r13; \
        mov     0x20(%rsi), %r12; \
        mov     0x28(%rsi), %rbx; \
        mov     0x30(%rsi), %rbp;

        .file "context.S"
        .text

/* %rdi contains the pointer to the target context structure in
 * which to save the calling context
 */
.globl _encore_context_save, encore_context_save
        .align 16
_encore_context_save:
encore_context_save:

        SAVE_CONTEXT

        ret

/* %rdi contains the pointer to the context structure to switch to
 */
.globl _encore_context_switch, encore_context_switch
        .align 16
_encore_context_switch:
encore_context_switch:

        LOAD_CONTEXT

        ret


/* %rdi contains the pointer to the context structure in which
 * the current context is to be stored
 * %rsi contains the pointer to the context structure to switch
 * to
 */
.globl _encore_context_swap, encore_context_swap
        .align 16
_encore_context_swap:
encore_context_swap:

        SAVE_CONTEXT

        LOAD_CONTEXT

        ret