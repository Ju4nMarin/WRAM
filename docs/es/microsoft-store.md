# Microsoft Store

La versión de la tienda es el mismo `WRAM.exe`, empaquetado como MSIX. Al arrancar, WRAM detecta que corre desde un paquete (`GetCurrentPackageFullName`) y cambia tres cosas:

| | Ejecutable de GitHub | Microsoft Store |
|---|---|---|
| Actualizaciones | Actualizador propio (GitHub) | Las instala la tienda; el botón de actualizar y el aviso diario no aparecen |
| Inicio con Windows | `HKCU\...\Run` | Tarea de inicio del paquete `WRAMStartup` (`windows.startupTask`) |
| Archivo de configuración | `%APPDATA%\WRAM\config.ini` | `%LOCALAPPDATA%\Packages\<paquete>\LocalState\config.ini` |

La tienda firma el paquete al publicarlo, así que la versión de la tienda no muestra el aviso de SmartScreen.

## Paquete

`tools/package-msix.ps1` arma el paquete a partir de `build/WRAM.exe`:

- `AppxManifest.xml` con la identidad de Partner Center, x64, Windows 10 2004 (10.0.19041) o posterior, los seis idiomas de la interfaz, la tarea de inicio y la capacidad `runFullTrust`.
- Logos generados desde `res/wram.ico`: `StoreLogo`, `Square44x44Logo` (con tamaños 16, 24, 32, 48 y 256, con y sin placa), `Square150x150Logo`, `Wide310x150Logo` y `Square310x310Logo`.
- `MakeAppx pack` del Windows SDK más reciente instalado. El paquete sale sin firmar.

```powershell
build.cmd
powershell -ExecutionPolicy Bypass -File tools\package-msix.ps1 -Version 1.7.4 -IdentityName "<Package/Identity/Name>" -Publisher "CN=<Package/Identity/Publisher>" -OutDir build\msix
```

La versión tiene tres partes; el paquete le agrega `.0`, como pide la tienda.

## Prueba local

Con el modo de desarrollador activado, la carpeta del paquete se registra sin firmar:

```powershell
powershell -ExecutionPolicy Bypass -File tools\package-msix.ps1 -Version 1.7.4 -IdentityName Test.WRAM -Publisher "CN=WRAM Test" -LayoutOnly
Add-AppxPackage -Register build\msix\layout\AppxManifest.xml
# WRAM aparece en Inicio; para quitarlo:
Get-AppxPackage Test.WRAM | Remove-AppxPackage
```

Solo corre un WRAM a la vez: hay que cerrar el ejecutable portable antes de abrir el empaquetado.

## CI

El job de compilación arma un paquete con identidad de prueba en cada push, para detectar errores de empaquetado. En el job de publicación, cuando existen las variables del repositorio `MSIX_IDENTITY_NAME` y `MSIX_PUBLISHER`, arma el paquete con la identidad de la tienda y lo sube como artefacto `WRAM-msix-<tag>`. Ese `.msix` es el que se sube a Partner Center.

## Publicación en Partner Center

1. Crear la cuenta de desarrollador en [Microsoft Partner Center](https://partner.microsoft.com/dashboard/registration/apps), de tipo individual (sin costo).
2. Reservar el nombre WRAM en Aplicaciones y juegos > Nuevo producto > Aplicación MSIX o PWA.
3. En Administración del producto > Identidad del producto, copiar `Package/Identity/Name` y `Package/Identity/Publisher` en las variables del repositorio `MSIX_IDENTITY_NAME` y `MSIX_PUBLISHER`.
4. Publicar una etiqueta de versión, descargar el artefacto `WRAM-msix-<tag>` de la ejecución del workflow y subir el `.msix` en Paquetes.
5. Completar Precios (gratis), Propiedades (categoría Utilidades y herramientas), Clasificación por edades y Descripciones de Store.
6. En Opciones de envío, justificar `runFullTrust` con el texto de abajo, en inglés.

Justificación de `runFullTrust`:

> WRAM is a native Win32 desktop app (C++, Direct2D) that draws a small metrics strip next to the notification area of the Windows taskbar. It needs full trust to read system performance counters (PDH), GPU information through D3DKMT and ACPI thermal zones, and to place its window on the taskbar. It does not require administrator rights, installs no drivers or services and makes no network connections in the Store version.

URL de la política de privacidad: `https://github.com/Ju4nMarin/WRAM/blob/main/docs/es/privacidad.md`.

## Pendiente

- Cuenta de Partner Center y reserva del nombre WRAM.
- Variables del repositorio `MSIX_IDENTITY_NAME` y `MSIX_PUBLISHER`.
- Primer envío y certificación.
