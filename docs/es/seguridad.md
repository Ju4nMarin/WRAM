# Seguridad

## Permisos

- Se ejecuta como el usuario (`requestedExecutionLevel asInvoker`). No pide administrador ni instala drivers ni servicios.
- Escribe solo en `%APPDATA%\WRAM`, `%LOCALAPPDATA%\WRAM` y, si se activa, en `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
- No abre conexiones de red. La ventana de configuración se carga desde el propio ejecutable.

## Ventana de configuración (WebView2)

- Contenido: un único HTML incrustado como recurso, con CSP `default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src data:`.
- Navegación: solo se permite la carga inicial de ese HTML; cualquier otra navegación se cancela.
- Ventanas nuevas y permisos (cámara, ubicación, notificaciones y demás) se rechazan.
- Deshabilitados: herramientas de desarrollo, menús contextuales, zoom, autocompletado, guardado de contraseñas, atajos del navegador y objetos del host.
- Los enlaces externos no salen de la página: la interfaz pide un destino por nombre (`repo`, `license`, `config`) y el programa abre solo URLs y carpetas fijas en el código.

## Entrada de datos

- Los mensajes de la interfaz tienen un tope de 32 KB.
- La configuración llega como INI y pasa por `ConfigCodec`: claves de métrica y fuentes con lista blanca, números con rango, colores con formato fijo, etiquetas sin caracteres de control ni separadores y como máximo 10 posiciones sin repetir.
- `config.ini` se rechaza si supera 64 KB y se escribe de forma atómica (archivo temporal y `MoveFileEx`).

## Endurecimiento del binario

| Opción | Efecto |
|---|---|
| `/GS`, `/sdl` | Comprobaciones de desbordamiento de búfer y advertencias de seguridad como errores |
| `/guard:cf` | Control Flow Guard |
| `/DYNAMICBASE`, `/HIGHENTROPYVA` | ASLR de 64 bits |
| `/NXCOMPAT` | DEP |
| `/CETCOMPAT` | Pila de sombra de hardware (CET) |
| `/DEPENDENTLOADFLAG:0x800` | Las DLL importadas se buscan solo en System32 |
| CRT estática | Sin dependencia de `vcruntime140.dll` |

La heap del proceso termina ante corrupción (`HeapEnableTerminationOnCorruption`) y una sola instancia corre por sesión.

## Dependencias y cadena de suministro

| Dependencia | Versión | Cómo se fija |
|---|---|---|
| SDK Microsoft.Web.WebView2 | 1.0.4370.26 | CMake lo descarga de NuGet y verifica su SHA-256 |
| actions/checkout | v7.0.1 | SHA completo del commit, sin credenciales persistidas |
| actions/upload-artifact | v7.0.2 | SHA completo del commit |
| actions/attest-build-provenance | v4.2.2 | SHA completo del commit |
| mikepenz/action-junit-report | v6.7.1 | SHA completo del commit |

Fijar las acciones por SHA de commit impide que una etiqueta movida o comprometida cambie lo que ejecuta el workflow. Dependabot (`.github/dependabot.yml`) revisa las acciones cada semana y abre un pull request hacia `develop` cuando hay una versión nueva. El token del workflow es de solo lectura, salvo en el trabajo de publicación, que recibe solo los permisos para publicar y atestar.

## Integridad del ejecutable

Las versiones las compila el workflow de CI a partir del commit etiquetado, no una máquina de desarrollo. Cada versión incluye:

- El hash SHA-256 de `WRAM.exe`.
- Una atestación de procedencia firmada con Sigstore a través de GitHub. Prueba que el archivo salió del workflow de este repositorio a partir de ese commit. Se verifica con GitHub CLI:

  ```
  gh attestation verify WRAM.exe --repo Ju4nMarin/WRAM
  ```

Las propiedades del archivo (pestaña Detalles) muestran los metadatos de versión:

| Campo | Valor |
|---|---|
| Nombre del producto | WRAM |
| Descripción | WRAM - monitor del sistema en la barra de tareas |
| Compañía | Juan Guillermo Marín Córdoba |
| Copyright | © 2026 Juan Guillermo Marín Córdoba. Licencia MIT. |
| Comentarios | Desarrollado por Juan Guillermo Marín Córdoba. https://github.com/Ju4nMarin/WRAM |
| Nombre original | WRAM.exe |

## Reporte de vulnerabilidades

Abre un aviso privado en <https://github.com/Ju4nMarin/WRAM/security/advisories/new> con los pasos para reproducirlo.

## Pendiente

- Firma Authenticode con un certificado de una autoridad reconocida.
