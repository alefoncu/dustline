# Dustline

Juego de conducción **off-road de mundo abierto** para macOS, hecho con Unreal Engine 5.8 (Apple Silicon).

> Inspirado en el género de los simuladores off-road de móvil, pero **sin código ni recursos de ningún juego existente**: todo lo de este repositorio es propio o de licencia libre (ver [CREDITS.md](CREDITS.md)).

## Qué hay

- **Mundo** de unos 4 × 4 km generado por código (ruido fractal + erosión hidráulica), con lagos, ~27 000 abetos propios de bajo poligonaje, rocas, troncos y material de terreno automático (hierba, tierra, barro, arena, roca, nieve).
- **Pistas para conducir**: una red de ~20 km de pistas de tierra y grava une diez **puntos de interés** (campamento base, miradores, lagos, vados y valles). Se trazan con búsqueda de caminos que penaliza la pendiente y la ladera, se suavizan para que las curvas se puedan tomar (radio ≥ ~6-7 m) y se excavan en el terreno con la rampa limitada (~20 %), calzada de 8 m sin árboles ni rocas y **vados de grava** de 40 cm donde cruzan un río. Alrededor de cada punto hay un claro. Fuera de las pistas sigue el campo a través.
- **Explorar**: brújula en el HUD hacia el punto sin descubrir más cercano (tipo y distancia), aviso al descubrirlo y **mapa** a pantalla completa (tecla **M**) con relieve, agua, pistas, puntos y tu posición.
- **6 coches** (Clase G, Panda 4x4, Defender, Buggy, Pick-up, Rock Crawler) con **carrocerías y piezas propias generadas por código**: ruedas (carretera, todo terreno, barro, clavos, balón), suspensión, transmisión, motor (en línea o en V según la cilindrada) y turbo.
- **Física propia** del vehículo (suspensión por trazado, neumáticos por superficie, motor con curva de potencia y turbo con retardo, agua, barro, nieve, colisiones).
- **Interfaz estilo Apple** (Slate propio): menú principal sobre la escena viva, pausa, ajustes (gráficos, juego, controles), velocímetro con cristal esmerilado y taller de piezas (motores, turbos, ruedas, neumáticos, suspensiones, tracción, chasis y pintura). Tipografía SF del sistema (no se distribuye con el juego).
- **Taller de piezas** (tecla **G** o «Configurar coche»): eliges entre catálogos de motor, turbo, ruedas, neumáticos, suspensión, tracción, chasis, equipo (snorkel, baca de expedición) y pintura, y ves la pieza al instante (rayos X); o conduces el coche de serie.
- **Modo carrera** (se puede quitar en Ajustes → Juego): las piezas cuestan créditos, que se ganan descubriendo lugares (1.500 cr) y conduciendo (1 cr cada 4 m). Lo comprado queda tuyo por modelo. En modo libre todo el taller es gratis.
- **Barro en la carrocería**: el coche se ensucia según el suelo, lo mojado, la velocidad y el patinaje (salpica más alto cuanto más rápido), el agua de los vados lo lava y el taller lo deja limpio.

- **Conducción realista**: neumático con curva de deslizamiento combinada, ruedas con inercia propia resueltas a la vez con los diferenciales (abiertos o bloqueados), motor con curva de par y turbo con retraso, convertidor de par, cambio automático o manual, reductora, ABS y control de tracción desactivables, barras estabilizadoras y contacto de la carrocería (vuelcos, panza).
- **Terreno que se nota**: hierba, hierba alta, tierra, grava, arena, barro, roca, nieve y hielo, cada uno con su agarre, su pico de deslizamiento, su resistencia y su hundimiento; el patinaje excava y el coche se puede atascar. La misma máscara horneada (`T_SurfaceMask` / `Content/Data/world.bin`) pinta el material y alimenta la física.
- **Agua**: ríos excavados en el terreno a partir del caudal real de la erosión, lagos en el fondo de las cuencas y el mar; flotación por columnas, arrastre con la corriente, inundación del habitáculo y motor ahogado si la toma de aire queda bajo el agua.
- **Vadear**: el nivel y la corriente se miran en cada rueda y en cada columna de la carrocería (a caballo de la orilla una rueda vadea y la otra no). El fondo cambia según el agua: cantos con verdín si corre, limo blando si está quieta y honda. Al entrar rápido se levanta una ola de proa que puede meter agua por la toma (motor calado, o hidrobloqueado si ibas acelerado: hay que pasar por el taller); a paso de hombre la ola deja un seno que protege el motor. El agua empuja el coche aguas abajo, le quita peso a las ruedas (menos agarre) y, si es honda, lo hace flotar y lo arrastra; las ruedas giradas hacen de timón y el dibujo empuja un poco. Los gasolina fallan antes (encendido mojado), los frenos mojados frenan menos hasta que se secan frenando, y el agua limpia los neumáticos embotados de barro.
- **Control de tracción** que frena la rueda que patina sola (el par pasa por el diferencial abierto a las demás) y solo recorta motor si patinan todas.
- Pruebas automáticas: `-dpool=PROF,CORRIENTE -watertest=KMH` (charca de prueba delante del coche), `-dford=PROF` (busca un vado en el mapa).
- **Clima vivo, no seleccionable**: frentes de lluvia y nieve que cruzan el mapa con el viento, más fríos en la montaña y el norte, más secos en el este; el suelo se moja (barro y charcos), se seca, se cubre de nieve o se hiela celda a celda, y eso cambia el agarre.

- **Sonido procedural** (sin grabaciones): motor por armónicos de encendido según rpm, carga y cilindros (con golpeteo diésel), turbo y válvula de descarga, rodadura y suelo (grava, barro, nieve), derrapes, agua, lluvia y viento. Volumen en Ajustes → Juego.
- **Salpicaduras, terrones de barro, nieve en polvo y polvo** según la rueda y el suelo, y **rodadas** con la huella de los tacos que se van borrando.

- **Cada coche se conduce distinto**: reparto de pesos (el Panda lleva el morro pesado, el buggy el motor detrás, el pick-up la caja vacía), dirección más rápida o más lenta, suspensión blanda o firme, centro de balanceo, neumático más incisivo o más perezoso, frenos, inercia y tracción propia (4Matic 40/60 del Clase G, viscoacoplador del Panda, autoblocante trasero del buggy, 4x4 conectable rígido del pick-up que "rasca" en curvas cerradas).
- **Choques**: impulso en el punto del golpe (el coche gira si pega con una esquina), daños por zonas (delante resta potencia y puede calar el motor, los laterales tuercen la dirección), golpe sonoro y sacudida de cámara. El taller lo repara.
- **Cuestas y ayudas**: control de descenso con la reductora, retención en pendiente, inclinómetro en el HUD, ESC que frena una rueda si el coche se cruza (en los modelos que lo traen) y aquaplaning en llano encharcado a partir de ~50 km/h.

## Controles

`W/S` acelerar y frenar (parado, `S` mete la marcha atrás) · `A/D` girar · `Espacio` freno de mano · `Shift` turbo · `B` bloqueo de diferenciales · `L` reductora · `E/Q` subir/bajar marcha (cambio manual) · `R` enderezar · `C` cámara · `G` garaje · `M` mapa · `Esc` pausa y menú. Con teclado el gas y el volante son progresivos.

**Mando**: `RT/LT` gas y freno/marcha atrás (analógicos) · stick izquierdo volante · `A` freno de mano · `X` turbo · `Y` cámara · `B` bloqueos · cruceta ↑ mapa, ↓ reductora, → taller · `LB/RB` cambio manual · `View` enderezar · `Menu` pausa.

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
