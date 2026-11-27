# Política de privacidad

WRAM no recopila, guarda ni envía datos personales. No tiene telemetría, analítica, cuentas ni publicidad.

## Qué lee WRAM

Las métricas salen de Windows en el mismo equipo: contadores de rendimiento, estado de la memoria, contadores de red y disco, estado de la batería y las temperaturas que exponen ACPI y el controlador gráfico. Los valores se muestran en la barra de tareas y no se guardan.

## Qué guarda WRAM

La configuración (métricas, etiquetas, umbrales y apariencia) queda en un archivo `config.ini` en el perfil del usuario:

| Versión | Ubicación |
|---|---|
| Ejecutable de GitHub, Scoop o winget | `%APPDATA%\WRAM\config.ini` |
| Microsoft Store | `%LOCALAPPDATA%\Packages\<paquete>\LocalState\config.ini` |

La ventana de configuración funciona sobre Microsoft Edge WebView2 y guarda su caché en el perfil del usuario. El archivo nunca sale del equipo.

## Red

La versión de Microsoft Store no abre conexiones de red: las actualizaciones llegan desde la tienda.

El ejecutable de GitHub solo se conecta a la API de GitHub y a las descargas de las versiones de GitHub, para buscar e instalar versiones nuevas: al pulsar "Buscar actualizaciones" y, con "Avisarme de versiones nuevas" activado, como mucho una vez al día. La consulta no lleva datos personales. Detalle en [seguridad.md](seguridad.md#actualizaciones-desde-la-app).

## Contacto

Preguntas sobre privacidad: [issues en GitHub](https://github.com/Ju4nMarin/WRAM/issues).
