# Paquete de revisión F0 — requiere aceptación humana

Fecha: 2026-10-14. Lenguaje fijado en
`5c01cea68f125f06dd3ac82a61776827bdb0c6f3` ([PIN.md](PIN.md)). Estado: **no
aprobado; no iniciar M1**.

## Material para revisar

1. Propuesta de gramática, modelo y tabla de 60 opcodes: [IR_SPEC_DRAFT.md](IR_SPEC_DRAFT.md).
2. Los cinco módulos IR manuales propuestos y fuentes Orbit oracle en
   `docs/proposed-ir/m1/`. Son fixtures de revisión; no se han parseado ni
   ejecutado y no constituyen corpus oficial de Orbit.
3. Targets propuestos: Windows x86-64 primero, Linux x86-64 segundo; macOS
   ARM64 fuera hasta M3. Solo single-thread hasta M3.
4. Deriva DB: propuesta de exclusión R6 hasta M3 salvo variante oficial
   compatible del runtime; detalle en [DECISIONS.md](DECISIONS.md).
5. Evidencia del runtime actual: ABI header de la revisión pineada y biblioteca
   local no rastreada, con sus SHA/proveniencia incompleta en [PIN.md](PIN.md).

## Decisiones solicitadas

- ¿Acepta el mantenedor la serialización JSON y las reglas de borde propuestas
  como IRv1, o publicará un formato equivalente? Debe confirmar explícitamente
  `int`, float NaN/payload/signed-zero, strings UTF-8 y campos por opcode.
- ¿Los cinco fixtures representan semántica válida de IRv1? Si no, corregirlos
  en el repo del lenguaje/contrato antes de congelar el formato.
- ¿Puede Orbit reproducir y publicar `libruntime_net.a` para Windows y Linux
  desde el commit pineado? ¿Cuál es el ABI/header blob oficial?
- Confirmar exclusión DB hasta M3; no se encontró una librería DB-compatible
  publicada en el pin.

## Gate de salida

La revisión queda aceptada solo con respuesta humana explícita y escrita a los
puntos anteriores (o aprobación de un IRv1 publicado equivalente). Hasta
entonces el rango IR soportado por R6 es `none`, no hay differential M1 medido,
y ningún código de loader/codegen/driver debe comenzar.
