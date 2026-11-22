# Dustline

Juego de conducción **off-road de mundo abierto** para macOS, hecho con Unreal Engine 5.8 (Apple Silicon).

> Inspirado en el género de los simuladores off-road de móvil, pero **sin código ni recursos de ningún juego existente**: todo lo de este repositorio es propio o de licencia libre (ver [CREDITS.md](CREDITS.md)).

## Qué hay

- **Mundo** de unos 4 × 4 km generado por código (ruido fractal + erosión hidráulica), con lagos, ~27 000 abetos propios de bajo poligonaje, rocas, troncos y material de terreno automático (hierba, tierra, barro, arena, roca, nieve).
- **7 coches** (Clase G, Urus, Panda 4x4, Defender, Buggy, Pick-up, Rock Crawler) con carrocerías CC0 de Kenney y **piezas propias generadas por código**: ruedas (carretera, todo terreno, barro, clavos, balón), suspensión, transmisión, motor (en línea o en V según la cilindrada) y turbo.
- **Física propia** del vehículo (suspensión por trazado, neumáticos por superficie, motor con curva de potencia y turbo con retardo, agua, barro, nieve, colisiones).
- **Garaje interactivo** dentro del juego (tecla **G**): cambia modelo, color, motor, turbo, lastre, suspensión, ruedas, neumático y tracción y lo ves al instante; modo rayos X.

## Controles

`W/S` acelerar y frenar · `A/D` girar · `Espacio` freno de mano · `Shift` turbo · `R` enderezar · `C` cámara · `1-3` clima (solo física por ahora) · `G` garaje.

## Estructura

| Carpeta | Contenido |
|---|---|
| `Source/Dustline` | Módulo C++: coche (`DustlineCar*`), física, garaje (Slate), generador de mallas y de árboles, terreno procedural |
| `Config/` | Ajustes del proyecto (mapa, modo de juego, empaquetado, gráficos ligeros) |
| `Content/` | Texturas, materiales, modelos y scripts de Python (`Content/Python`) |
| `tools/` | Scripts: ejecutar, capturar, regenerar el mundo, empaquetar e instalar |
| `prototype-swift/` | Prototipo anterior en Swift + SceneKit (físicas y sistemas) |

El mapa (`Content/Maps/*.umap`) **no se versiona**: se regenera con `tools/rebuild_world.sh`.

## Requisitos

- macOS con Apple Silicon, Xcode (con Metal Toolchain: `xcodebuild -downloadComponent MetalToolchain`).
- Unreal Engine 5.8.4 instalado en `/Volumes/SSD/UnrealEngine/UE_5.8` (ajusta las rutas de `tools/*.sh` si lo tienes en otro sitio).

## Uso

```bash
tools/rebuild_world.sh          # regenera el mapa
tools/play.sh 0                 # juega (0-6 = modelo de coche) en el editor, sin empaquetar
tools/ship.sh "mensaje"         # commit + push + empaquetar + instalar en /Applications
```

## Estado

Prototipo en desarrollo. Pendiente: clima visual en el terreno, sonido de motor, carrocerías más realistas. Rendimiento medido: ~28 fps en un Mac de 8 GB.

## Licencia

Código: MIT (ver [LICENSE](LICENSE)). Recursos de terceros: ver [CREDITS.md](CREDITS.md). Unreal Engine es propiedad de Epic Games y **no** se incluye en este repositorio.
