.section .text
.code32
.global process_yield
.type process_yield, @function

/* Ceder el resto del turno usa la misma ruta que la preempcion.
   isr.S guarda todos los registros y retorna al contexto elegido con IRET.
   El vector 0x80 solo acepta llamadas desde ring 0; no es una API de usuario. */
process_yield:
    int $0x80
    ret
.size process_yield, . - process_yield

.section .note.GNU-stack, "", @progbits
