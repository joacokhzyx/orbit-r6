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
  `and_op`, `or_op`, `not_op`, `neg`, `copy`, `jump`, `load_var`, `decl_var`,
  `store_var`, `list_create`, `list_push`, `list_len`, `list_get`, `call` y `ret`, solo para
  las combinaciones validadas en codegen.
  Un opcode no implementado produce error con nombre e instrucción.
- Emisión nativa: el codegen Win64 baja IR a una representación interna
  temporal; `asm_win64.c` codifica bytes x86-64 y `coff.c` escribe AMD64 COFF.
  No genera/parsea C ni llama GCC/GAS al compilar programas. GNU `ld` resuelve
  símbolos del runtime oficial/CRT. M1 no optimiza.
- Evidencia semántica entera: `runtime/orbit_api.h:114` declara
  `typedef int orbit_int`; `c_backend.orb` baja `add/sub/mul` vía `uintptr_t`,
  `div/mod` vía `orbit_int` signed y comparaciones relacionales vía `intptr_t`.
  R6 refleja esas conversiones para el subconjunto medido; casos no cubiertos
  por el diferencial no se generalizan.
- `sub/mul/div` también aceptan registros binary64 y usan SSE2; fixture
  `09-float-arithmetic` compara operaciones exactas representables contra el
  compilador de referencia. No se amplía esta evidencia a bordes NaN/infinito.
- `ne/lt/le/gt/ge` aceptan int o float. Para float, `10-float-compare-nan`
  verifica los seis predicados de comparación con NaN y signed zero contra el
  pin; `ne` combina `setne` con parity (`setp`), `lt/le` enmascaran el flag
  unordered de `ucomisd`. `eq` y `ne` aceptan también strings y llaman al
  `strcmp` del CRT; el pin emite `strcmp(a,b)==0/!=0` para strings
  (`c_backend.orb:2550-2564`). Strings relacionales siguen rechazados.
- `neg` acepta int (negación modular en el slot nativo) y float (voltea el bit
  de signo con XOR, preservando el payload); `11-neg` compara valores finitos
  con GCC. Overflow de int no queda cubierto por ese fixture.
- `copy` mueve escalares a slots y `jump` resuelve labels locales por función;
  `12-jump-copy` confirma salto incondicional y copia; `13-while-counter`
  verifica back-edge, branch condicional y variable int local contra el pin.
  `decl_var/store_var` soportan locales escalares y referencias `list`
  declaradas en la función, inicializadas/actualizadas desde registros.
  `load_var` también lee globals
  `int`/`bool` literales: el pin serializa esos `IRGlobal.value` como String
  decimal (`builder.orb:2288-2304,2333-2338`; `c_backend.orb:1479-1503`). Otros
  globals, params por nombre y captures fallan explícitamente.
- Listas: Win64 M1 soporta únicamente `list<int>` en `main`, usando
  `list_create` (8-byte slots y `elem_kind=2`), `list_push` int y `list_len`.
  Creación y push llaman `orbit_list_create_typed`/`orbit_list_push_int` del
  runtime fijado. El retorno C `OrbitResult` usa sret; `ok` está en byte 0 y
  `value` en byte 16 según el layout del header (`runtime/orbit_api.h:179-184`).
  Longitud lee `OrbitList.len` (offset 8; `orbit_api.h:217-224,517-519`), igual
  al accessor inline oficial. `list_get` solo admite int en `main`; comprueba
  nulos, bounds unsigned y kind, y llama a `orbit_list_index_error` o
  `orbit_list_kind_error` oficiales ante fallo (`orbit_api.h:358-360,422-468`).
  Los demás kinds, set/pop, helpers fuera de `main` y uso de lista como
  argumento/retorno fallan cerrados.
- Registros IR son slots de stack con tipo estático; no se promete asignación a
  registros físicos. Argumentos `arg` se encolan hasta `call` y se vacían en
  cada llamada; binding param se baja al prólogo de la función. Se admiten hasta
  256 parámetros escalares: Win64 ubica posiciones 0–3 en registros y las
  restantes en slots de 8 bytes a partir de `[rsp+32]`; el callee los recibe
  desde `[rbp+48]`. Se reserva el área saliente por función y se conserva la
  alineación de 16 bytes. Solo se admite llamada directa a símbolo del módulo.
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
