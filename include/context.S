
#define SAVE_CONTEXT(target) \
        mov     %rsp, 0x00(target); \
        mov     %r15, 0x08(target); \
        mov     %r14, 0x10(target); \
        mov     %r13, 0x18(target); \
        mov     %r12, 0x20(target); \
        mov     %rbx, 0x28(target); \
        mov     %rbp, 0x30(target);

#define LOAD_CONTEXT(source) \
        mov     0x00(source), %rsp; \
        mov     0x08(source), %r15; \
        mov     0x10(source), %r14; \
        mov     0x18(source), %r13; \
        mov     0x20(source), %r12; \
        mov     0x28(source), %rbx; \
        mov     0x30(source), %rbp;

        .file "context.S"
        .text

/* %rdi contains the pointer to the target context structure in
 * which to save the calling context
 */
.globl _encore_context_save, encore_context_save
        .align 16
_encore_context_save:
encore_context_save:

        SAVE_CONTEXT(%rdi)

        ret

/* %rdi contains the pointer to the context structure to switch to
 */
.globl _encore_context_switch, encore_context_switch
        .align 16
_encore_context_switch:
encore_context_switch:

        LOAD_CONTEXT(%rdi)

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

        SAVE_CONTEXT(%rdi)

        LOAD_CONTEXT(%rsi)

        ret