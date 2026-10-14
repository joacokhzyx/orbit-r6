# Paquete de revisión F0 — aceptación registrada

Fecha: 2026-10-14. Lenguaje fijado en
`5c01cea68f125f06dd3ac82a61776827bdb0c6f3` ([PIN.md](PIN.md)). Estado:
**IRv1 y cinco fixtures aceptados por el usuario el 2026-10-14**. M1 puede
comenzar en Windows x86-64. La publicación por Orbit sigue pendiente; R6 usa
este contrato local fijado al commit y falla todo lo UNSPECIFIED.

## Material para revisar

1. Contrato aceptado, gramática, modelo y 60 opcodes:
   [IR_SPEC_DRAFT.md](IR_SPEC_DRAFT.md).
2. Los cinco IR manuales y sus oráculos fuente `.orb` en
   `docs/proposed-ir/m1/`; aceptados como fixtures M1 (su equivalencia aún debe
   medirse diferencialmente).
3. Targets aceptados: Windows x86-64 primero, Linux x86-64 después; macOS
   ARM64 desde M3. Single-thread y DB fuera del corpus hasta M3, salvo runtime
   oficial compatible para DB.
4. Runtime Windows recompilado con el script oficial del commit pineado;
   hash del resultado y divergencia del archivo local en [PIN.md](PIN.md).
   Runtime Linux aún no compilado/verificado en este host.

## Pendiente de verificación, no aprobación

El ABI/header blob coincide con el objeto versionado del pin. El archivo local
`.orbit/lib/libruntime_net.a` no coincide en SHA con el artefacto recreado; no
se usa como evidencia ni como dependencia. La biblioteca recreada en Windows
está verificada como x86-64 y expone símbolos ABI. Falta obtener/medir la
variante Linux desde la misma fuente y pin antes de habilitar Linux. El rango
IR implementado se declarará solo cuando el driver y M1 diferencial estén
verdes.
