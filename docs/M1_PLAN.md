# Plan M1 — bloqueado por contrato IR

Base reproducible prevista: lenguaje `5c01cea68f125f06dd3ac82a61776827bdb0c6f3`;
IR declarado: ninguno publicado todavía. No seguir `main` ni actualizar este
pin de forma automática.

## Ficheros del primer incremento de código (solo tras revisión/aprobación IRv1)

- `src/loader/ir_text.{h,c}`: parser estricto del formato textual aprobado,
  versión y diagnósticos; solo representaciones acordadas.
- `src/codegen-x64/{win64,sysv}.c`: emisión de objeto x86-64 por ABI, empezando
  con el subconjunto medido de M1; opcode desconocido/no implementado = error.
- `src/driver/r6.c`: CLI `r6 input.ir -o output`, invoca assembler/linker y enlaza
  la `libruntime_net.a` construida desde el commit pineado, sin generación C.
- `tests/diff/{run,compare}.{py,ps1}`: compila el mismo fuente Orbit mediante
  gcc de referencia y R6; compara exit status y stdout en bytes y reporta
  stderr/diagnósticos. Sin umbral temporal como criterio de éxito.
- `tests/ir/m1/`: cinco IR manuales aprobados y sus fuentes Orbit equivalentes.
- `docs/CONTRACT.md`: commits, hashes de runtime/artefactos, rango IR soportado,
  comandos/toolchains por plataforma y evidencia diferencial.

La extensión/nombres concretos del parser y cómo produce objetos quedan
supeditados a las herramientas disponibles y al contrato aprobado. No crear un
serializador supuesto para sortear este bloqueo.

## Primer programa y gate

Primer caso propuesto: función `main` con dos constantes enteras, `add`, llamada
a `print` del runtime con una cadena literal y retorno 0. Comprueba valores,
argumentos, llamada ABI, salida y exit. Antes de codificar debe existir su
representación exacta en el IR publicado y un ejecutable gcc oracle generado
desde la fuente Orbit. Gate M1: este y otros cuatro programas equivalentes
comparan salida y estado de salida byte por byte; las cinco comparaciones deben
ser verdes. Un binario R6 que simplemente arranque no satisface “funciona”.

## Orden de trabajo M1

1. Aceptar IRv1, el formato de valores/instrucciones y el oracle de un corpus
   emitido por Orbit; aclarar/runtime-link del commit pineado por target.
2. Añadir loader y un subconjunto inicial explícito; rechazo claro de todo lo
   demás. Sin optimizaciones.
3. Emitir objeto y enlazar el primer programa contra runtime oficial; comparar
   differentialmente con gcc antes de ampliar el subconjunto.
4. Completar los cinco programas M1, registrar comandos, conteos, bytes y SHAs;
   solo entonces declarar M1 verde.
