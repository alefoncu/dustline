# Dustline — revisión crítica completa

*Versión revisada: 0.2 (commit 7492148, 28 nov 2026). Recorrido hecho sobre el juego real: menú, ajustes, pausa, conducción, taller, clima, agua y un muestreo automático de todo el mapa (504 × 504 puntos, uno cada 8 m).*

Imágenes de apoyo en esta carpeta: `antes_mapa_transitable.jpg`, `antes_mundo.jpg`, `antes_interfaz.jpg`.

---

## Veredicto

**Nota: 5,5 / 10.** Dustline tiene un motor de conducción bastante mejor que lo que lo rodea. Lo mejor del juego es la física propia:
- cada rueda resuelta por separado, con diferenciales;
- suelos que se notan distintos;
- un clima que moja, embarra y nieva el mapa por zonas;
- agua que empuja, levanta olas y ahoga motores.

El problema es que **el mapa no está pensado para conducir**. Es un terreno procedural bonito de lejos pero hostil de cerca:
- No hay ni un camino.
- Solo el 6,6 % del mapa es terreno cómodo (menos de 20° de pendiente) conectado con la salida.
- No hay nada que hacer ni ningún sitio al que ir.

Un juego off-road vive de la ruta, de "a ver si puedo subir eso", y aquí nadie te propone esa ruta.

---

## 1. El mapa: ¿está pensado para conducir? No

Medido sobre el juego (`-mapstats`):

| Dato | Valor | Lectura |
|---|---|---|
| Tierra con pendiente < 10° | 15,8 % | Muy poca llanura |
| Tierra con pendiente < 20° | 34,0 % | Casi dos tercios del mapa son cuestas fuertes |
| Tierra con pendiente < 30° | 53,2 % | La mitad del mapa está por encima del límite cómodo de un 4x4 |
| Transitable (< 32°, sin agua honda ni obstáculo) | 55,9 % | — |
| …y **conectado con la salida** | 43,7 % | Más de la mitad de lo transitable queda aislado por cortados |
| Cómodo (< 20°) conectado con la salida | **6,6 %** | La zona "jugable" de verdad es una mancha |
| Orillas de río con pendiente < 15° | 23,9 % | 3 de cada 4 orillas son taludes: casi no hay vados |
| Zonas "de mar" bajo z = 0 | 7 % del mapa | **Se puede conducir por encima del agua** (fallo) |

**Fallos:**
1. **Crítico: el coche anda sobre el mar.** Las cuencas más bajas (7 % del mapa) quedan bajo el plano de agua a cota 0. Ese plano frena las trazas, y la física no lo conoce como agua. El coche se posa encima como si fuera grava (captura `antes_mundo.jpg`, arriba a la izquierda).
2. **No hay caminos, pistas ni sendas.** Todo es campo a través sobre ruido fractal y erosión. No hay ritmo: o subes una ladera de 30°, o bajas, o chocas.
3. **No hay vados.** Los ríos se excavan en V con taludes, así que cruzarlos es casi siempre volcar o quedarse atascado.
4. **El borde del mapa es un anillo de cortados** (rojo en el mapa): está bien como límite, pero ocupa mucho y no se ve venir.
5. **No hay puntos de interés** (miradores, refugios, collados, lagos con playa), así que el mundo no tiene objetivos.
6. **La salida está al pie de rocas enormes:** la cámara del menú y la del taller quedan tapadas por una roca (capturas del menú y del taller).
7. **No hay mapa ni brújula:** en 4 × 4 km de lomas iguales es fácil perderse y no hay forma de orientarse.

**Mejoras propuestas:**
- Una **red de pistas** que una varios puntos de interés, trazada con búsqueda de caminos que penalice la pendiente (rampa máxima ~15 %, con zigzag si hace falta). Las pistas se excavan en el terreno, se pintan de tierra y grava y se limpian de árboles y rocas.
- **Vados de verdad** donde una pista cruza un río: fondo de grava, 30-50 cm de agua y rampas suaves.
- La **salida en un claro** al principio de una pista, con buena vista.
- **Puntos de interés** que descubrir, con marcadores, brújula y un **mapa (tecla M)** con las pistas, los puntos y tu posición.
- El **mar** como agua de verdad para la física (vadeo, flotación, ahogarse) y sin colisión en el plano.

## 2. Física de conducción

**Lo bueno:**
- Ruedas implícitas con diferenciales (abierto, autoblocante, bloqueado, central rígido).
- Convertidor y caja automática que deciden con la velocidad real.
- Reductora con control de descenso.
- ABS limitado por el agarre.
- Control de tracción que frena la rueda que patina sola.
- ESC.
- Neumático por superficie y por tipo.
- Hundimiento y excavado.
- Choques por impulso con daños por zonas.
- Volcado correcto: de lado ya no tracciona.
- Cada coche tiene carácter propio.

**Fallos y carencias:**
1. **No hay soporte de mando.** Para un juego de conducción es lo primero que echa en falta un jugador. Con teclado el acelerador y el volante son todo o nada: imposible dosificar en barro, nieve o trial.
2. **Con teclado el gas es binario.** Pisar a fondo en barro entierra el coche, y sin control de tracción no hay forma de "acariciar" el pedal.
3. En lomas boscosas el coche choca a menudo con árboles invisibles desde la cámara cercana. La cámara no se aparta lo suficiente en cuestas.
4. Las pruebas de manejo dependen del terreno; no hay una zona plana de pruebas (se resuelve con las pistas y el claro de salida).

**Mejoras:**
- Mando completo: stick o gatillos analógicos, botones para freno de mano, reductora, bloqueos, cámara y reinicio.
- Con teclado, acelerador y volante progresivos (rampa al pulsar).

## 3. Vegetación y objetos

**Lo bueno:**
- Bosques de abeto densos, arbustos, tocones, troncos muertos y rocas de varios tamaños.
- Hierba alta que aparece alrededor del coche.
- La colisión de los árboles es solo el tronco (correcto).

**Fallos:**
1. **Los abetos se ven grises, como cartón apilado** (`antes_mundo.jpg`, arriba a la derecha). Las hojas usan un material sin tinte verde suficiente y bajo la luz quedan pardo-grisáceas. Es lo primero que salta a la vista en cualquier captura.
2. **Las rocas grandes tienen la textura estirada** y forman muros que tapan cámaras (menú y taller).
3. **El suelo base es un ocre uniforme:** le falta variación de color y detalle (de cerca parece arena de playa).
4. Los árboles están en todas partes por igual: no hay claros donde descansar la vista ni corredores naturales.

**Mejoras:**
- Tinte verde oscuro y variado en el follaje.
- Claros y pistas sin árboles.
- Ninguna roca grande pegada a la salida.

## 4. Coches y componentes

**Lo bueno:**
- Seis modelos con carácter distinto.
- Taller con motor, turbo, ruedas, neumáticos, suspensión, tracción, chasis y pintura.
- Piezas visibles (rayos X del motor).
- Cifras que cambian al momento (potencia, peso, velocidad, altura libre).

**Fallos:**
1. Las carrocerías se ven planas y limpias al lado de un terreno con texturas PBR: no se manchan nunca y no hay equipamiento todoterreno. *(Corrección posterior: en esta revisión escribí que eran modelos Kenney. No es así: el juego usa las carrocerías procedurales propias con cotas reales. Kenney está desactivado desde noviembre.)*
2. El taller deja combinaciones absurdas sin avisar (un motor de 7 L con turbo 3 da 1.001 CV en un Clase G). No hay coste ni límite, así que no hay decisiones.
3. La lista de piezas se corta por abajo ("V8 todoterreno" queda a medias). Se puede desplazar, pero no se nota.
4. Pintura oscura de serie en casi todos: el coche se pierde contra el terreno en sombra y bajo la lluvia.

## 5. Interfaz

**Lo bueno:**
- Estilo limpio y coherente (cristal, tipografía clara).
- Ajustes bien ordenados (gráficos, juego, controles, acerca de).
- HUD discreto con velocímetro, marcha, rpm y etiquetas de estado.
- Pausa con el clima y la temperatura.

**Fallos:**
1. El menú principal tiene de fondo una roca a pantalla completa.
2. El HUD encadena etiquetas en una sola línea que crece sin límite ("Subida 5° / lateral 81° · Volcado: pulsa R · …").
3. No hay brújula, mapa ni objetivos en el HUD.
4. La pantalla de controles no menciona el mando (no existe).
5. El taller no indica que la lista se puede desplazar.

## 6. Clima y agua

**Lo bueno:**
- El clima va por zonas, no se elige, y moja, embarra, nieva y hiela el suelo celda a celda.
- Lluvia y nieve visibles con niebla.
- El agua: nivel por rueda, ola de proa, hidrobloqueo, corriente, flotación estable, frenos mojados.

**Fallos:**
- El mar no es agua para la física (ver mapa).
- Casi no hay vados para disfrutar del agua (ver mapa).

---

# Plan de acción

Ordenado por impacto. ✅ = se hace en esta tanda; ⏸ = necesita una decisión o recursos tuyos.

| # | Tarea | Área | Estado |
|---|---|---|---|
| 1 | Mar = agua real en la física y sin colisión en el plano | Mapa / física | ✅ |
| 2 | Red de pistas entre puntos de interés: rampa limitada, excavada, pintada de tierra y grava, sin árboles ni rocas | Mapa | ✅ |
| 3 | Vados de grava poco hondos donde las pistas cruzan ríos | Mapa / agua | ✅ |
| 4 | Salida en un claro al inicio de una pista, sin rocas que tapen las cámaras | Mapa / UI | ✅ |
| 5 | Puntos de interés que descubrir, con brújula y distancia en el HUD | Juego | ✅ |
| 6 | Mapa a pantalla completa (tecla M) con terreno, pistas, agua, puntos y posición | UI | ✅ |
| 7 | Follaje de los abetos verde y variado | Vegetación | ✅ |
| 8 | Mando analógico (volante, gas, freno y botones) + teclado progresivo | Conducción | ✅ |
| 9 | HUD: etiquetas en dos líneas como máximo y por prioridad | UI | ✅ |
| 10 | Taller: avisar que la lista sigue (degradado y barra) | UI | ✅ |
| 11 | Pantalla de controles con el mando | UI | ✅ |
| 12 | Pintura de serie más visible | Coches | ✅ |
| 13 | Carrocerías con más vida: barro y equipamiento | Coches | ✅ (segunda tanda) |
| 14 | Economía del taller: modo carrera con créditos y modo libre | Juego | ✅ (segunda tanda) |
| 15 | Variación de color del suelo y rocas sin textura estirada | Arte | ✅ (segunda tanda) |

*El resultado de cada punto se anota abajo al terminar.*

## Resultado de la ejecución

*28 nov 2026, commit f794655, instalado en /Applications/Dustline.app. Capturas: `despues_conduccion.jpg` y `despues_mapa.jpg`.*

| # | Tarea | Resultado |
|---|---|---|
| 1 | Mar como agua | ✅ El plano del agua ya no tiene colisión. Las zonas bajo cota 0 son agua para la física: se vadea, se flota y el motor se ahoga. En el muestreo, las zonas con impacto "falso" pasaron del 7 % al 0 %. |
| 2 | Red de pistas | ✅ 10 puntos de interés y 11 pistas (19,4 km). Detalle de la verificación debajo de esta tabla. |
| 3 | Vados | ✅ Detalle debajo de esta tabla. |
| 4 | Salida en un claro | ✅ La salida es el Campamento base: claro de 25 m llano y sin rocas, mirando hacia la primera pista. |
| 5 | Puntos de interés + brújula | ✅ La brújula indica el tipo y la distancia al punto sin descubrir más cercano. Al llegar a 35 m se descubre (aviso "Descubierto: …  ·  k de 10"). El progreso se guarda. |
| 6 | Mapa (M) | ✅ Relieve, agua, pistas, puntos (descubiertos en dorado) y una flecha con tu posición y rumbo. Se ve correctamente en la app instalada. |
| 7 | Follaje verde | ✅ Tinte verde oscuro aplicado a los dos materiales de hojas. ⚠️ No he podido comprobar el efecto de cerca en la app instalada. |
| 8 | Mando + teclado progresivo | ✅ Gatillos y stick analógicos, todos los botones, gas y volante del teclado con rampa. ⚠️ Probado sin mando físico, solo compilado y por lógica. |
| 9 | HUD por prioridad | ✅ Como mucho 4 avisos en dos líneas, lo urgente primero. |
| 10 | Taller: barra de desplazamiento | ✅ |
| 11 | Controles con mando | ✅ Sección «Mando» en Ajustes → Controles y en el README. |
| 12 | Pintura visible | ✅ Clase G caqui, Defender verde más claro y Pick-up azul más vivo. |
| 13–15 | Carrocerías, economía, texturas | ✅ En la segunda tanda (abajo). |

**Verificación de las pistas (punto 2):**
- Midiéndolas sobre el terreno real del juego, el 95 % de cada pista está a ≤ 18-20 % de rampa.
- La inclinación lateral supera el 20 % en solo 0-32 m por pista. Antes eran cientos de metros.
- Calzada de 9 m de tierra con grava, sin árboles ni rocas, y taludes suaves (1:2,6).
- Curvas abiertas a un radio mínimo de unos 6-7 m (antes había radios de 3-4 m).

**Verificación de los vados (punto 3):** cada cruce es llano, con 40 cm de agua sobre grava firme. Alrededor hay un rápido de 80 cm como mucho, sin pozas al lado. El agua de la física se calcula ahora a 2 m de resolución, igual que el terreno.

**Prueba de conducción automática** (piloto automático por la pista, a 25 km/h, con el Clase G):
- **Recorridas sin incidentes durante la prueba:** pistas 1, 3, 4, 6, 7 y 9 (de 850 m a 1,2 km cada una en 200 s), cruzando vados y hielo.
- **Pista 2:** el piloto entra demasiado rápido en un vado hondo y el motor se hidrobloquea. Hay que revisarla a mano.
- **Puntos aislados:** quedan algunos de una sola muestra con rampa alta, en empalmes de las pistas 2, 3, 4, 6 y 10.
- **Pistas 5, 8, 10 y 11:** no se han recorrido con el piloto. Su medición sobre el terreno es buena.

**Lo que conviene probar a mano:**
- la pista 2;
- la sensación del mando;
- el follaje de cerca;
- y si la red de pistas es divertida de conducir, que solo se puede juzgar jugando.

## Segunda tanda (puntos 13-15)

*28 nov 2026. Capturas: `segunda_equipo_barro.jpg`, `segunda_taller.jpg` y `segunda_rocas.jpg`.*

**Hallazgo nuevo e importante: la vegetación salía con el material por defecto.**
- **Causa:** a los materiales de árboles, arbustos y rocas les faltaba el uso «Instanced Static Meshes», y el juego los dibuja como instancias. Unreal los cambiaba por el material gris por defecto. De ahí el aspecto de «cartón gris» que anoté en la revisión; el tinte verde del punto 7 no llegaba a verse.
- **Arreglo:** uso activado en los dos materiales base. Ahora los abetos se ven verdes y las rocas con su textura. El log ya no da ningún aviso de material.

| # | Tarea | Resultado |
|---|---|---|
| 13 | Coches | ✅ **Barro:** se acumula en la carrocería según el suelo, lo mojado, la velocidad y el patinaje. Sube más cuanto más salpica, el agua de un vado lo lava y el taller deja el coche limpio. Las ruedas se embarran del todo y los cristales y las luces, menos. ✅ **Equipo nuevo en el taller:** snorkel (Clase G, Panda y Pick-up), que sube la toma de aire al techo y deja vadear más hondo; y baca de expedición con bidones, rueda de repuesto, caja y barra de luz (+45 kg, más resistencia al aire y el centro de masas 3 cm más alto). ✅ El Defender ya traía snorkel dibujado pero la física no lo usaba: ahora sí. |
| 14 | Economía | ✅ **Modo carrera** (de serie, se puede quitar en Ajustes → Juego): empiezas con 4.000 cr, ganas 1.500 cr por cada lugar descubierto y 1 cr cada 4 m recorridos con las ruedas en el suelo. Cada pieza tiene precio (de 500 a 13.900 cr). La pieza de serie es gratis y lo comprado se queda tuyo por modelo, así que volver a montarlo no cuesta. Si no llega el dinero, la tarjeta dice cuánto falta. El saldo se ve en el taller y en el HUD, y hay un botón «Empezar de cero». ✅ **Modo libre:** todo gratis, como antes. |
| 15 | Texturas | ✅ **Roca de las laderas con proyección triplanar:** se acabaron las vetas estiradas en los cortados. ✅ **Variación a gran escala:** praderas secas y otras más verdes, manchas cálidas y frías de unos 100 m, y brillo variable de unos 600 m. ✅ **Repetición rota:** la mezcla de dos escalas de cada textura cambia por zonas de ~30 m. ✅ **Rocas grandes:** detalle de roca proyectado en el mundo, para que la textura de 1k no se vea estirada al escalarlas. Rendimiento medido igual que antes (27-33 fps). |

**Verificación:**
- **Prueba automática del modo carrera:** 8 de 8 comprobaciones bien. Se rechaza la compra sin dinero, se compra con él, se vuelve gratis a la pieza original y a la ya comprada, 1 km da 250 cr y en modo libre no se cobra.
- **Barro, con el piloto automático en la pista 1 bajo la lluvia:** pasa de 0,05 a 0,83 en 75 s a 22 km/h, salpicado hasta 67 cm. En el mismo tramo se ganan 114 cr.

**Sigue pendiente:**
- la pista 2;
- la sensación con mando físico;
- si los precios se sienten justos, que solo se sabe jugando una partida.
