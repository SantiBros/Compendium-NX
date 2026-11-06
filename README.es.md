<div align="center">

[![English](https://img.shields.io/badge/lang-English-1c2233?style=for-the-badge)](README.md)
[![Español](https://img.shields.io/badge/idioma-Espa%C3%B1ol-dc1e2e?style=for-the-badge)](README.es.md)

<img src="docs/images/banner.jpg" alt="Compendium, un gestor de mods de Persona para Nintendo Switch: homebrew no oficial que arma el CPK del juego en la consola" width="100%">

# Compendium: gestor de mods de Persona para Nintendo Switch

**Una app homebrew no oficial que instala y gestiona mods de los juegos Persona, desde la propia Switch.**<br>
Explora GameBanana, elige un mod y Compendium arma en la consola el archivo `.CPK` que lee Persona 5 Royal.
Sin PC, sin CriPakTools, sin copiar carpetas a mano.

[![Última versión](https://img.shields.io/github/v/release/SantiBros/Compendium-NX?include_prereleases&style=for-the-badge&label=descargar&color=dc1e2e&logo=github&logoColor=white)](../../releases)
[![Descargas](https://img.shields.io/github/downloads/SantiBros/Compendium-NX/total?style=for-the-badge&label=descargas&color=1c2233)](../../releases)
![Estado: en pruebas](https://img.shields.io/badge/estado-en%20pruebas-d29922?style=for-the-badge)
![Homebrew de Nintendo Switch](https://img.shields.io/badge/Nintendo%20Switch-homebrew-1c2233?style=for-the-badge)
![Persona 5 Royal](https://img.shields.io/badge/juego-Persona%205%20Royal-1c2233?style=for-the-badge)
![Atmosphère](https://img.shields.io/badge/CFW-Atmosph%C3%A8re-1c2233?style=for-the-badge)
[![Licencia: GPL v3](https://img.shields.io/badge/licencia-GPL%20v3-1c2233?style=for-the-badge)](LICENSE)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-apoya%20el%20proyecto-FF5E5B?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/S4I32A0WMU)

[Instalación](#cómo-instalar) · [Uso](#uso) · [Qué mods funcionan](#qué-mods-funcionan) · [Estado](#estado) · [Preguntas frecuentes](#preguntas-frecuentes) · [Solución de problemas](docs/INSTALL.es.md#solución-de-problemas) · [Cambios](CHANGELOG.md) · [Cómo funciona](docs/TECHNICAL.md)

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/S4I32A0WMU)

</div>

> [!IMPORTANT]
> **No se incluye ningún archivo del juego, ni en este repositorio ni en las versiones publicadas.** Necesitas
> tu propia copia de *Persona 5 Royal* para Nintendo Switch. Los mods son de sus autores: Compendium los
> descarga de donde se publican (GameBanana, GitHub) y no aloja ninguno. Por favor, no pidas archivos del juego
> en los issues.
>
> Compendium funciona en la **Switch** e instala mods en la **versión de Switch** del juego. Puede adaptar
> muchos mods hechos para la versión de PC ([cuáles](#qué-mods-funcionan)), pero no modifica el juego de PC.

> [!NOTE]
> **Primera versión de prueba (0.0.1).** Todo funciona en la versión de prueba para PC de la app, con mods de
> prueba, pero todavía no se ha probado en una consola. Espera errores, haz copia de tus partidas y usa emuMMC.
> Los reportes desde consolas son lo que más ayuda: mira [Se buscan testers](#se-buscan-testers).

## Capturas

<p align="center"><sub>Tomadas en la versión de prueba para PC de la app, a los 1280×720 de la Switch, con
publicaciones de ejemplo en lugar de las de GameBanana: en la consola, cada mod muestra sus propias imágenes y
su descripción.</sub></p>

<table>
  <tr>
    <td width="50%"><img src="docs/images/screenshots/browse.jpg" alt="La página Explorar: los mods de Persona 5 Royal en GameBanana, con la imagen y los detalles del mod elegido"></td>
    <td width="50%"><img src="docs/images/screenshots/pc-mods.jpg" alt="La pestaña de mods de PC de la página Explorar"></td>
  </tr>
  <tr>
    <td align="center"><sub>Explora los mods de Persona 5 Royal en GameBanana, con imágenes y detalles</sub></td>
    <td align="center"><sub>Mods hechos para la versión de PC, adaptados a la Switch al instalarlos</sub></td>
  </tr>
  <tr>
    <td><img src="docs/images/screenshots/conflict.jpg" alt="Un aviso antes de instalar: el mod cambia archivos que también cambian dos mods instalados"></td>
    <td><img src="docs/images/screenshots/my-mods.jpg" alt="Mis mods: el orden de carga con avisos de conflictos e interruptores"></td>
  </tr>
  <tr>
    <td align="center"><sub>Antes de instalar, te dice cuáles de tus mods cambian los mismos archivos</sub></td>
    <td align="center"><sub>Mis mods: orden de carga, conflictos, y activar o desactivar sin borrar nada</sub></td>
  </tr>
  <tr>
    <td><img src="docs/images/screenshots/add-mods.jpg" alt="Agregar mods: archivos en la tarjeta SD, mods de SimpleModManager, un número de mod, un enlace desde el móvil"></td>
    <td><img src="docs/images/screenshots/options.jpg" alt="Elegir qué versiones de un mod instalar"></td>
  </tr>
  <tr>
    <td align="center"><sub>Agrega mods desde la SD, SimpleModManager, un número de mod o tu móvil</sub></td>
    <td align="center"><sub>Mods que vienen en varias versiones: elige las que quieras</sub></td>
  </tr>
  <tr>
    <td><img src="docs/images/screenshots/phone-link.jpg" alt="Un código QR para enviar un enlace de descarga desde el móvil"></td>
    <td><img src="docs/images/screenshots/settings.jpg" alt="Ajustes: cómo llegan los mods al juego, los archivos del propio juego, p5rcbt"></td>
  </tr>
  <tr>
    <td align="center"><sub>¿Encontraste un mod en el móvil? Escanea el código y envía el enlace a la Switch</sub></td>
    <td align="center"><sub>Ajustes: cómo llegan los mods al juego, p5rcbt, complementos en segundo plano</sub></td>
  </tr>
</table>

<sub>La interfaz de la app está en inglés por ahora.</sub>

## Características

- **Arma el CPK en la consola.** Persona 5 Royal lee sus archivos desde archivos `.CPK`, así que sus mods tienen
  que ir dentro de uno. Compendium lo hace en la Switch: tus mods van al `PATCH1.CPK` del juego, junto con los
  archivos PATCH1 del propio juego, cada vez que agregas, quitas o desactivas uno.
- **GameBanana integrado.** Explora y busca los mods de Persona 5 Royal con sus imágenes, descripciones y
  archivos, e instala uno con **A**. El archivo que corresponde a tu versión del juego aparece destacado.
- **Mods de PC adaptados.** Los mods hechos para la versión de PC con Reloaded-II (P5R Essentials) se
  convierten en mods de Switch al instalarlos: sus archivos del juego van al CPK.
- **Muchos formatos de mods:** paquetes de Aemulus (el formato habitual de los mods de Switch), archivos
  `PATCH1.CPK` ya hechos, carpetas de Atmosphère y de SimpleModManager, carpetas de p5rcbt y archivos sueltos
  del juego ([detalles](#qué-mods-funcionan)).
- **Varios mods a la vez, con orden de carga.** Cuando dos mods cambian el mismo archivo, gana el que está más
  arriba en Mis mods, y la app te dice qué archivos chocan antes de instalar.
- **Activar y desactivar sin borrar.** Se guarda una copia de cada mod, así que desactivar uno solo vuelve a
  armar el CPK sin él. Mantén **L** al abrir el juego para jugar una vez sin ningún mod.
- **Mods desde cualquier lado:** GameBanana, el número de un mod, proyectos de GitHub (su última versión), un
  enlace enviado desde tu móvil con un código QR, o archivos que copies en `switch/Compendium/inbox`.
- **Trae lo que ya tienes:** los mods guardados para **SimpleModManager** (`sd:/mods/…`) y un `PATCH1.CPK` que
  hayas copiado a mano.
- **Compatible con p5rcbt.** Instala el plugin de Skyline p5rcbt desde Ajustes y cambia sus opciones (60 FPS,
  el menú de ajustes de la versión de PC).
- **Cuida tu tarjeta SD.** Los archivos que ya estaban donde escribe la app se guardan aparte y vuelven a su
  lugar cuando desactivas tus mods, y la app explica el cierre de Atmosphère al cargar mods (Program ID
  010041544D530000) si ocurre.
- **Un registro para reportar errores** (`switch/Compendium/log.txt`): qué se instaló y armó, y cualquier error.

## Qué necesitas

| | Qué |
|---|---|
| **Consola** | Una Nintendo Switch, Switch OLED o Switch Lite con [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere) y el Homebrew Menu. Usa emuMMC y guarda una copia de tu tarjeta SD y tus partidas. |
| **Juego** | Tu propio *Persona 5 Royal* para Switch (ID de título `01005CA01580E000`). Cualquier versión funciona con el modo predeterminado; el plugin p5rcbt necesita la versión **1.0.2**. |
| **Internet** | Wi-Fi, para explorar y descargar mods. Sin él, copia los archivos de los mods en `switch/Compendium/inbox`. |
| **Tarjeta SD** | Espacio para tus mods dos veces: la app guarda una copia de cada uno y arma el CPK a partir de ellas. |
| **PC** | No hace falta. |

## Cómo instalar

> [!TIP]
> **¿Nuevo en el homebrew de Switch?** Primero prepara tu consola con firmware personalizado (Atmosphère) y el
> Homebrew Menu: la guía de la comunidad [switch.hacks.guide](https://switch.hacks.guide/) lo explica paso a
> paso. ¿No sabes qué son emuMMC, LayeredFS o un CPK? Mira el [glosario](docs/INSTALL.es.md#glosario).

<img src="docs/images/install-steps.es.png" alt="Instalar en 4 pasos: descarga Compendium.nro, cópialo en switch/Compendium de la SD, ábrelo con R + cualquier juego, pulsa A en un mod y abre Persona 5 Royal" width="100%">

1. **Descarga.** En la página de [Releases](../../releases), en **Assets**, descarga **`Compendium.nro`** (no
   "Source code", que es solo para desarrolladores).
2. **Copia a la tarjeta SD.** Ponlo en `switch/Compendium/` en tu tarjeta SD (crea la carpeta si no está).
3. **Abre.** Mantén **R** mientras abres cualquier juego para que aparezca el Homebrew Menu y elige
   **Compendium**.
4. **Elige mods.** En **Browse** (Explorar), pulsa **A** en un mod y luego **A** en su archivo. Pulsa **+** para
   cerrar la app y abre Persona 5 Royal: tus mods ya están.

> [!WARNING]
> Abre el Homebrew Menu con **R + un juego** (title takeover) siempre que puedas. Desde el Álbum, el homebrew
> recibe mucha menos memoria: Compendium funciona ahí, pero con mods grandes se le puede acabar.

### Paso único: los archivos del propio juego (recomendado)

`PATCH1.CPK` viene con la actualización del juego y tiene algunos de sus archivos. Para que la app los conserve
al armar el de tus mods, deja que los lea una vez (y otra vez después de cada actualización del juego):

1. Pulsa **+** para cerrar la app.
2. Mantén **L y R** y abre Persona 5 Royal. Sigue manteniendo hasta que aparezca el Homebrew Menu.
   (R abre el Homebrew Menu en lugar del juego; L desactiva los mods en ese inicio.)
3. Abre Compendium. Lee los archivos solo y te avisa cuando termina.

También lee la lista de archivos del juego, que usa para ubicar los archivos de los mods que no traen su carpeta
del CPK.

<details>
<summary><b>Cómo queda la tarjeta SD</b></summary>

```
sd:/
├── switch/Compendium/
│   ├── Compendium.nro       la app
│   ├── library/             una copia de cada mod instalado
│   ├── inbox/               pon archivos aquí para instalarlos desde Add mods
│   ├── original/            el PATCH1.CPK y la lista de archivos del juego (el paso único)
│   ├── backup/              tus propios archivos que la app guardó aparte (vuelven cuando los mods están desactivados)
│   ├── settings.json        los ajustes de la app
│   ├── state.json           qué está instalado y armado
│   └── log.txt              qué hizo la app y cualquier error: adjúntalo a los reportes de errores
├── atmosphere/contents/01005CA01580E000/
│   └── romfs/CPK/PATCH1.CPK el CPK armado con tus mods (modo predeterminado)
└── p5r/bind/Compendium.cpk  el CPK armado con tus mods (modo p5rcbt)
```

</details>

La [guía de instalación completa](docs/INSTALL.es.md) explica cómo actualizar, hacer copias, conseguir mods sin
Wi-Fi y la [solución de problemas](docs/INSTALL.es.md#solución-de-problemas).

### Actualizar

Reemplaza `Compendium.nro` por el nuevo. Tus mods, el orden de carga y los ajustes se conservan.

## Uso

La app está en inglés; entre paréntesis, qué significa cada página.

| Página | Qué hace | Botones |
|---|---|---|
| **Browse** (Explorar) | Los mods de Persona 5 Royal en GameBanana | **L / R** mods de Switch o de PC · **Y** ordenar · **X** buscar · **A** instalar |
| **Search** (Buscar) | Busca por nombre o autor, o escribe el número de un mod (el final de su enlace de GameBanana) | **X** buscar · **A** abrir |
| **Add mods** (Agregar mods) | Archivos en `switch/Compendium/inbox`, los mods de SimpleModManager, un `PATCH1.CPK` copiado a mano, un enlace desde tu móvil, un número de mod | **A** instalar · **X** borrar el archivo |
| **My mods** (Mis mods) | Tu orden de carga: gana el mod de más arriba | **A** activar/desactivar · **L / R** subir/bajar · **X** quitar |
| **Settings** (Ajustes) | Cómo llegan los mods al juego, los archivos del propio juego, p5rcbt, complementos en segundo plano, almacenamiento | **A** cambiar |

**+** cierra la app desde cualquier página, y **B** vuelve atrás.

## Qué mods funcionan

| Cómo viene el mod | Qué hace la app |
|---|---|
| **Paquete de Aemulus** (una carpeta con `Package.xml`), el formato habitual de los mods de P5R para Switch | Sus archivos van al CPK en las rutas que tienen, como hace Aemulus |
| **Un `PATCH1.CPK` ya hecho** o cualquier `.cpk` de un mod | Sus archivos se usan tal cual, combinados con tus otros mods |
| **Carpetas de Atmosphère o de SimpleModManager** (`atmosphere/contents/01005CA01580E000/…`) | El CPK de dentro se combina; los trucos, parches de exefs y plugins van a la tarjeta SD |
| **Archivos sueltos del juego** (`BASE/…`, `EN/…`, `PATCH1/…`, `romfs/CPK/PATCH1/…` de Aemulus, `PATCH1.CPK_unpacked` de CriPakTools) | Se empaquetan en el CPK |
| **Carpetas de p5rcbt** (`p5r/bind/…`) | Se empaquetan en el CPK |
| **Mods de PC para Reloaded-II** (`ModConfig.json`, `P5REssentials/CPK/…`) | Se adaptan: sus archivos del juego se empaquetan en el CPK. La mayoría de los archivos de los mods de PC también funcionan en Switch, pero no todos |

Todavía no se pueden (la app lo avisa al instalar): las partes que Aemulus combina en un PC (`.flow`, `.msg`,
`.tblpatch`, sprites y parches binarios), la combinación de FEmulator de Reloaded-II, y los mods de código de PC
(`.dll`), que no pueden funcionar en la Switch. El resto de ese mod sí se instala.

### Cómo llegan los mods al juego

Elige en **Settings → Load mods with**:

- **PATCH1.CPK (sin plugin)**, el predeterminado. La app arma `atmosphere/contents/01005CA01580E000/romfs/CPK/PATCH1.CPK`
  con tus mods y los archivos PATCH1 del propio juego, y el LayeredFS de Atmosphère lo carga. Funciona con
  cualquier versión del juego.
- **p5rcbt (plugin de Skyline).** La app arma `p5r/bind/Compendium.cpk`, que [p5rcbt](https://github.com/Raytwo/p5rcbt)
  agrega a los archivos del juego. El PATCH1.CPK del juego no se toca. p5rcbt solo funciona con la versión
  **1.0.2** del juego; instálalo desde Ajustes.

## Estado

> [!NOTE]
> **En pruebas.** La versión 0.0.1 es la primera. El núcleo de la app (el armado del CPK, los formatos de mods,
> el orden de carga) se comprueba con pruebas automáticas en cada cambio, y la app completa funciona en una
> versión de prueba para PC, pero ninguna consola ha cargado sus mods todavía. Haz copia de tus partidas y usa
> emuMMC.

| Parte | Estado |
|---|---|
| Persona 5 Royal | 🧪 Primera versión: esperando los primeros reportes desde consolas |
| Armado de PATCH1.CPK (modo predeterminado) | 🧪 Comprobado con pruebas; el juego aún no lo ha cargado en una consola |
| Explorar, buscar y descargar de GameBanana | 🧪 Funciona en la versión de prueba para PC |
| Paquetes de Aemulus, CPKs, carpetas de Atmosphère / SimpleModManager / p5rcbt, archivos sueltos | 🧪 Funciona en la versión de prueba para PC |
| Mods de PC (Reloaded-II) | 🧪 Se adaptan los archivos del juego; ❌ los mods de código `.dll` no pueden funcionar en la Switch |
| Modo p5rcbt y sus ajustes | 🧪 Solo la versión 1.0.2 del juego |
| Combinación de Aemulus (`.flow`, `.msg`, `.tblpatch`, sprites, parches binarios) | ❌ Todavía no |
| Persona 4 Golden, Persona 3 Portable, Persona 5 Strikers, Persona 5 Tactica, Persona 4 Arena Ultimax | ❌ Previstos para versiones futuras |

**Problemas conocidos**
- Ninguno reportado todavía: los primeros reportes desde consolas darán forma a la 0.0.2.

Mira el [registro de cambios](CHANGELOG.md) para saber qué cambió en cada versión.

## Se buscan testers

Lo que más ayuda ahora son reportes desde consolas reales, en español o en inglés, con el `log.txt` de
`switch/Compendium/`:

| Qué | Cómo ayudar |
|---|---|
| 🎮 **¿Aparece un mod en el juego?** | Instala un mod pequeño (uno de interfaz o de texturas es lo más fácil de ver), abre Persona 5 Royal y fíjate. ¿Apareció? |
| 📦 **Los archivos del propio juego** | Haz el [paso único](#paso-único-los-archivos-del-propio-juego-recomendado) (L + R al abrir el juego). ¿La app dijo que leyó el PATCH1.CPK? |
| 🖥️ **Mods de PC** | Instala un mod de la pestaña **PC mods**. ¿Funciona en el juego? ¿Cuál era? |
| 🧩 **Formatos de mods** | Un mod que la app no pudo leer, o que se instaló pero no funcionó: envía el enlace a su página. |
| 🔌 **p5rcbt** (versión 1.0.2 del juego) | Instala p5rcbt desde Ajustes y cambia **Load mods with** a p5rcbt. ¿Funcionan tus mods y la opción de 60 FPS? |
| 📱 **Enlace desde el móvil** | Envía el enlace de un mod desde tu móvil con el código QR. ¿Llegó? |

Abre un [issue](../../issues/new/choose): el formulario pide todo lo necesario.

## Preguntas frecuentes

<details>
<summary><b>¿Necesito un PC?</b></summary>

No. Compendium busca, descarga, descomprime e instala los mods en la Switch, y también arma ahí el CPK del juego.
Un PC (o un móvil) solo sirve para buscar mods en una pantalla más grande: envía el enlace a la Switch con el
código QR de la página **Add mods**.
</details>

<details>
<summary><b>¿Por qué tiene que armar un CPK? ¿No basta con copiar la carpeta del mod?</b></summary>

Persona 5 Royal guarda sus archivos dentro de grandes archivos `.CPK`. El LayeredFS de Atmosphère puede
reemplazar archivos completos de un juego, pero no archivos sueltos dentro de otro archivo, así que los mods de
P5R para Switch vienen como un `PATCH1.CPK` con sus archivos dentro. Pero solo hay un `PATCH1.CPK`, así que dos
mods que traen el suyo se pisarían.

Compendium resuelve las dos cosas: toma los archivos de cada mod activado, en tu orden de carga, agrega los
archivos PATCH1 del propio juego y escribe un solo `PATCH1.CPK` con todos. En un PC, eso es lo que hacen Aemulus
o CriPakTools.
</details>

<details>
<summary><b>¿Puedo usar varios mods al mismo tiempo?</b></summary>

Sí. Todos los mods activados van al mismo CPK. Si dos cambian el mismo archivo, solo uno puede ganar: el que
está más arriba en **My mods**. La app te avisa antes de instalar, y My mods marca los mods que chocan. Sube un
mod con **L** para que gane.
</details>

<details>
<summary><b>¿Es Aemulus o Reloaded-II para Switch?</b></summary>

No exactamente. Hace en la Switch lo que Aemulus hace en un PC con los mods de P5R para Switch: pone los
archivos de los mods en un CPK, en un orden de carga. La combinación de scripts y textos del juego de Aemulus
(`.flow`, `.msg`, `.tblpatch`) todavía no está. Los mods de Reloaded-II para la versión de PC se adaptan: se usan
sus archivos del juego; los mods de código (`.dll`) no pueden funcionar en la Switch.
</details>

<details>
<summary><b>Ya tengo mods instalados a mano o con SimpleModManager. ¿Qué pasa con ellos?</b></summary>

No se pierde nada. En **Add mods**, la app muestra los mods de SimpleModManager (`sd:/mods/…`) y un
`PATCH1.CPK` que hayas copiado a mano, y puede agregarlos a la lista para combinarlos con tus otros mods. Los
archivos que ya estaban donde escribe la app se mueven a `switch/Compendium/backup/` y vuelven a su lugar
cuando desactivas tus mods.
</details>

<details>
<summary><b>¿Qué versión del juego necesito?</b></summary>

Cualquiera, con el modo predeterminado (PATCH1.CPK). El plugin p5rcbt solo funciona con la versión 1.0.2.
Algunos mods están hechos para una versión concreta: en la página Browse, el archivo que corresponde a la tuya
aparece marcado con **Matches your game**.
</details>

<details>
<summary><b>Un mod no aparece, o el juego se cierra. ¿Qué hago?</b></summary>

Mira la [solución de problemas](docs/INSTALL.es.md#solución-de-problemas). En resumen: comprueba que el mod esté
activado y por encima de los mods que cambian los mismos archivos, desactiva uno a uno los últimos mods que
agregaste, y mantén **L** al abrir el juego para comprobar que funciona sin mods. Si el juego muestra un error
de Atmosphère con Program ID `010041544D530000`, desactiva los complementos en segundo plano en Ajustes. Luego,
por favor, [abre un issue](../../issues/new/choose) con el `log.txt`.
</details>

<details>
<summary><b>¿Y los otros juegos de Persona?</b></summary>

Están previstos. La app está hecha alrededor de una lista de juegos, y Persona 5 Royal es el primero; Persona 4
Golden, Persona 3 Portable y los demás de Switch vendrán después, cada uno cuando su formato de mods esté listo y
probado.
</details>

<details>
<summary><b>¿Me pueden banear la consola?</b></summary>

Los mods solo cambian los archivos del juego en tu tarjeta SD, pero conectarse a internet con firmware
personalizado puede hacer que una consola sea baneada de los servicios de Nintendo. Usa los mods en un emuMMC y
bloquea los servidores de Nintendo (por ejemplo con la redirección de DNS de Atmosphère o 90DNS). Esa decisión
es tuya.
</details>

<details>
<summary><b>¿Por qué "Compendium"?</b></summary>

En los juegos de Persona, el Compendio es donde se guardan tus Personas, listas para volver a invocarlas. Aquí
guarda tus mods: cada uno que instalas se queda en la biblioteca, listo para volver a activarlo.
</details>

## Cómo funciona

1. **Busca y descarga los mods** con la API de GameBanana, las versiones publicadas en GitHub o un enlace desde
   tu móvil.
2. **Mira dentro de cada archivo** y reconoce qué es el mod (un paquete de Aemulus, un CPK, una carpeta de
   Atmosphère, un mod de PC…) y cuáles de sus versiones sirven para tu juego.
3. **Guarda una copia** del mod en su biblioteca, con las rutas del CPK de sus archivos.
4. **Arma un solo CPK** con los archivos de cada mod activado (gana el que está más arriba en la lista) y los
   archivos PATCH1 del propio juego, con un lector y escritor de CPK hecho para la app a partir del formato de
   CRI (nombres de archivo, alineación de 2 KB, descompresión CRILAYLA).
5. **El LayeredFS de Atmosphère** le da al juego ese CPK en lugar de su propio `PATCH1.CPK` (o p5rcbt lo carga).

Más en [docs/TECHNICAL.md](docs/TECHNICAL.md) (en inglés): el formato CPK, cómo se reconocen los formatos de
mods, la biblioteca y la organización del código.

## Compilar desde el código fuente

Necesitas [devkitPro](https://devkitpro.org/wiki/Getting_Started) con `switch-dev` y `switch-portlibs`, y CMake.
Luego:

```sh
git clone --recursive https://github.com/SantiBros/Compendium-NX
cd Compendium-NX
cmake -S . -B build-switch -DPLATFORM_SWITCH=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch --target Compendium.nro
```

El resultado es `build-switch/Compendium.nro`. [docs/BUILDING.md](docs/BUILDING.md) (en inglés) tiene los
detalles, las pruebas, la versión de prueba para PC y cómo se publica una versión.

## Apoyo

Si te gusta este proyecto, puedes apoyar mi trabajo en Ko-fi. ¡Me ayuda a seguir actualizando mis proyectos y crear nuevos!

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/S4I32A0WMU)

## Créditos

Sobre los hombros de otros:

- **ATLUS** y **SEGA**, que hicieron los juegos de Persona. Por favor, apoya los originales.
- **Los modders** que comparten su trabajo en GameBanana y en otros sitios: Compendium solo instala lo que ellos
  hacen.
- [**Patchwork**](https://github.com/TheRomeoAlfa/patchwork-nx) de TheRomeoAlfa: Compendium está basado en él
  (cliente de GameBanana, descargas, archivos comprimidos, interfaz).
- [**SimpleModManager**](https://github.com/nadrino/SimpleModManager) de nadrino: sus carpetas de mods se pueden
  traer, y su forma de gestionar mods de LayeredFS.
- [**p5rcbt**](https://github.com/Raytwo/p5rcbt) de Raytwo: el plugin opcional.
- [**ShrineFox**](https://docs.shrinefox.com), [**Aemulus**](https://github.com/TekkaGB/AemulusModManager) de
  TekkaGB y [**PyCriCodecs**](https://github.com/Youjose/PyCriCodecs): documentación de los formatos de mods de
  P5R y del formato CPK de CRI.
- [**Borealis**](https://github.com/xfangfang/borealis) (la versión de xfangfang), [**libnx**](https://github.com/switchbrew/libnx),
  [**devkitPro**](https://devkitpro.org) y [**Atmosphère**](https://github.com/Atmosphere-NX/Atmosphere).
- **libcurl**, **libarchive**, **Mbed TLS**, **zlib**, **nlohmann/json** y las demás bibliotecas de
  [THIRD_PARTY.md](THIRD_PARTY.md).
- **Barlow Condensed** de Jeremy Tribby (SIL Open Font License), usada en el banner y el icono.

## Licencia

Compendium es software libre bajo la [Licencia Pública General de GNU v3.0](LICENSE), como Patchwork y
SimpleModManager, en los que se basa. El código de terceros conserva sus propias licencias
([THIRD_PARTY.md](THIRD_PARTY.md)). Los juegos y los mods no se incluyen y esta licencia no los cubre de ninguna
manera.

## Aviso legal

Este es un proyecto no oficial hecho por fans. No está afiliado, respaldado ni patrocinado por ATLUS, SEGA,
Nintendo ni GameBanana. *Persona* es una marca registrada de ATLUS; Nintendo Switch es una marca registrada de
Nintendo. Aquí no se distribuye código ni datos del juego: necesitas tu propia copia. Los mods son obra de sus
autores y se descargan de donde ellos los publican; respeta sus condiciones.

Usar homebrew requiere firmware personalizado, lo que va contra las condiciones de servicio de Nintendo y puede
hacer que una consola sea baneada de los servicios en línea; esa decisión es tuya. El software se ofrece tal
cual, sin garantía (consulta la [LICENCIA](LICENSE)).

---

<div align="center">

Un proyecto de **SantiBros** · Si te gusta, dale una estrella al repositorio ⭐ y compártelo

</div>
