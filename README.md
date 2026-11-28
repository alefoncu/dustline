# Dustline

Juego de conducción **off-road de mundo abierto** para macOS, hecho con Unreal Engine 5.8 (Apple Silicon).

> Inspirado en el género de los simuladores off-road de móvil, pero **sin código ni recursos de ningún juego existente**: todo lo de este repositorio es propio o de licencia libre (ver [CREDITS.md](CREDITS.md)).

## Qué hay

- **Mundo** de unos 4 × 4 km generado por código (ruido fractal + erosión hidráulica), con lagos, ~27 000 abetos propios de bajo poligonaje, rocas, troncos y material de terreno automático (hierba, tierra, barro, arena, roca, nieve).
- **6 coches** (Clase G, Panda 4x4, Defender, Buggy, Pick-up, Rock Crawler) con **carrocerías y piezas propias generadas por código**: ruedas (carretera, todo terreno, barro, clavos, balón), suspensión, transmisión, motor (en línea o en V según la cilindrada) y turbo.
- **Física propia** del vehículo (suspensión por trazado, neumáticos por superficie, motor con curva de potencia y turbo con retardo, agua, barro, nieve, colisiones).
- **Interfaz estilo Apple** (Slate propio): menú principal sobre la escena viva, pausa, ajustes (gráficos, juego, controles), velocímetro con cristal esmerilado y taller de piezas (motores, turbos, ruedas, neumáticos, suspensiones, tracción, chasis y pintura). Tipografía SF del sistema (no se distribuye con el juego).
- **Taller de piezas** (tecla **G** o «Configurar coche»): eliges entre catálogos de motor, turbo, ruedas, neumáticos, suspensión, tracción, chasis y pintura, y ves la pieza al instante (rayos X); o conduces el coche de serie.

- **Conducción realista**: neumático con curva de deslizamiento combinada, ruedas con inercia propia resueltas a la vez con los diferenciales (abiertos o bloqueados), motor con curva de par y turbo con retraso, convertidor de par, cambio automático o manual, reductora, ABS y control de tracción desactivables, barras estabilizadoras y contacto de la carrocería (vuelcos, panza).
- **Terreno que se nota**: hierba, hierba alta, tierra, grava, arena, barro, roca, nieve y hielo, cada uno con su agarre, su pico de deslizamiento, su resistencia y su hundimiento; el patinaje excava y el coche se puede atascar. La misma máscara horneada (`T_SurfaceMask` / `Content/Data/world.bin`) pinta el material y alimenta la física.
- **Agua**: ríos excavados en el terreno a partir del caudal real de la erosión, lagos en el fondo de las cuencas y el mar; flotación por columnas, arrastre con la corriente, inundación del habitáculo y motor ahogado si la toma de aire queda bajo el agua.
- **Clima vivo, no seleccionable**: frentes de lluvia y nieve que cruzan el mapa con el viento, más fríos en la montaña y el norte, más secos en el este; el suelo se moja (barro y charcos), se seca, se cubre de nieve o se hiela celda a celda, y eso cambia el agarre.

- **Sonido procedural** (sin grabaciones): motor por armónicos de encendido según rpm, carga y cilindros (con golpeteo diésel), turbo y válvula de descarga, rodadura y suelo (grava, barro, nieve), derrapes, agua, lluvia y viento. Volumen en Ajustes → Juego.
- **Salpicaduras, terrones de barro, nieve en polvo y polvo** según la rueda y el suelo, y **rodadas** con la huella de los tacos que se van borrando.

## Controles

`W/S` acelerar y frenar (parado, `S` mete la marcha atrás) · `A/D` girar · `Espacio` freno de mano · `Shift` turbo · `B` bloqueo de diferenciales · `L` reductora · `E/Q` subir/bajar marcha (cambio manual) · `R` enderezar · `C` cámara · `G` garaje · `Esc` pausa y menú.

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
tools/play.sh 0                 # juega (0-5 = modelo de coche) en el editor, sin empaquetar
tools/ship.sh "mensaje"         # commit + push + empaquetar + instalar en /Applications
```

## Estado

Prototipo en desarrollo. Pendiente: carrocerías más realistas. Rendimiento medido: ~28 fps en un Mac de 8 GB.

## Licencia

Código: MIT (ver [LICENSE](LICENSE)). Recursos de terceros: ver [CREDITS.md](CREDITS.md). Unreal Engine es propiedad de Epic Games y **no** se incluye en este repositorio.
