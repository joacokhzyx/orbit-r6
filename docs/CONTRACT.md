# Contrato interno M1

Fecha: 2026-10-14. Lenguaje: pin de [PIN.md](PIN.md). IR aceptado: versión 1
del contrato R6 en [IR_SPEC_DRAFT.md](IR_SPEC_DRAFT.md). Este archivo fija
interfaces entre loader, codegen, driver y diferencial; no amplía semántica.

## Representación del loader (`src/loader/ir.h`)

- `R6Module` conserva en orden funciones, globals, models, types y migrations.
- Cada `R6Function` conserva nombre, parámetros/tipos, instrucciones, conteo y
  tipos de registro, retorno, extern y route.
- `R6Instruction` conserva opcode enum, `dest` opcional, tres `R6Value`
  etiquetados y `elem_kind`. Los tipos numéricos son `int32_t` para reg/label y
  `int64_t` para literales durante validación de rango; floats se almacenan como
  `uint64_t` binary64 bits; strings son bytes UTF-8 validados y NUL terminados
  solo al materializar C strings para ABI.
- El loader rechaza claves extra/duplicadas, tags desconocidos, campos
  inválidos, longitud/type mismatch, UTF-8 inválido, registro fuera de rango,
  opcode no soportado y versión diferente de 1. No normaliza ni reordena.
- La propiedad de asignaciones pertenece a `R6Module`; `r6_module_free`
  libera recursivamente. Parsear JSON a este modelo no ejecuta callbacks ni
  produce efectos de runtime.

## Codegen

- Límite actual: Windows x86-64; `load_const`, `add`, `sub`, `mul`, `div`,
  `mod`, `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `jump_if_false`, `label`, `arg`,
  `and_op`, `or_op`, `not_op`, `call` y `ret`, solo para los tipos/combinaciones
  validados en codegen. Un opcode no implementado produce error con nombre e
  instrucción.
- Emisión nativa: el codegen Win64 baja IR a una representación interna
  temporal; `asm_win64.c` codifica bytes x86-64 y `coff.c` escribe AMD64 COFF.
  No genera/parsea C ni llama GCC/GAS al compilar programas. GNU `ld` resuelve
  símbolos del runtime oficial/CRT. M1 no optimiza.
- Evidencia semántica entera: `runtime/orbit_api.h:114` declara
  `typedef int orbit_int`; `c_backend.orb` baja `add/sub/mul` vía `uintptr_t`,
  `div/mod` vía `orbit_int` signed y comparaciones relacionales vía `intptr_t`.
  R6 refleja esas conversiones para el subconjunto medido; casos no cubiertos
  por el diferencial no se generalizan.
- Registros IR son slots de stack con tipo estático; no se promete asignación a
  registros físicos. Argumentos `arg` se encolan hasta `call` y se vacían en
  cada llamada; binding param se baja al prólogo de la función. Solo se admite
  llamada directa a símbolo declarado en el módulo.
- ABI externa: C ABI del target. Win64: los primeros cuatro argumentos usan
  slots posicionales RCX/RDX/R8/R9 o XMM0-XMM3 según su tipo; caller reserva
  32 bytes de shadow space y mantiene alineación de stack; retorno entero en
  RAX, float en XMM0. `orbit_print` es `void(const char*)`. Enlace runtime oficial net-only
  más `-lws2_32`.
- `main` del módulo se exporta como C `main`; retorno int32 determina exit
  status. No soportar `main` faltante o duplicado.

## Driver y diagnósticos

- CLI M1: `r6 <input.ir> -o <output.exe>`. Driver escribe archivos temporales
  solo bajo su build/temp local, invoca el assembler/linker toolchain y borra
  temporales al completar; conserva salida solo cuando link tuvo éxito.
- Diagnóstico de una línea a stderr:
  `r6: <archivo>:<linea>:<columna>: error[<codigo>]: <mensaje>`; exit no cero.
  Errores de JSON apuntan al token; errores semánticos/codegen, al índice de
  instrucción y opcode. Sin fallback, truncamiento silencioso ni mensajes de
  éxito en stderr.
- El harness differential compila la fuente `.orb` con el comando gcc del pin,
  ejecuta gcc y R6 con stdin idéntico y compara exit y stdout como bytes. El
  stderr del programa se captura separadamente. Diferencia implica fail.

## Aislamiento

- `src/loader`: parseo/validación y dueño de `ir.h`.
- `src/codegen-x64`: emisión desde `ir.h`; no parsea JSON ni llama al driver.
- `src/driver`: CLI, temporales, invocación de linker y captura de códigos.
- `tests/diff`: oráculo gcc, ejecución y comparación exacta; no cambia fixtures.
- Ningún carril crea shims, implementa runtime o altera `orbit-lang`.

Alternativa descartada: generar C para delegar lowering (prohibido por contrato).
