# Instalación

WRAM es un único ejecutable portable. No tiene instalador ni escribe fuera de tu perfil de usuario.

## Requisitos

| | |
|---|---|
| Sistema | Windows 10 versión 1903 o posterior, o Windows 11 |
| Arquitectura | x64 |
| Permisos | Usuario estándar, sin administrador |
| Ventana de configuración | Microsoft Edge WebView2 Runtime |

WebView2 viene con Windows 11 y con Windows 10 actualizado. Si falta, WRAM muestra un aviso al abrir la configuración; la franja de la barra funciona igual. Se instala desde la página oficial de Microsoft: <https://developer.microsoft.com/microsoft-edge/webview2/> (opción "Evergreen Bootstrapper").

La temperatura de GPU necesita un driver WDDM 2.4 o posterior (Windows 10 1803 en adelante con drivers actuales de NVIDIA, AMD o Intel).

## Pasos

1. Descarga `WRAM.exe` desde la última versión en <https://github.com/Ju4nMarin/WRAM/releases/latest>.
2. Crea una carpeta fija para el programa, por ejemplo:

   ```
   %LOCALAPPDATA%\Programs\WRAM
   ```

   El inicio con Windows guarda la ruta del ejecutable, así que conviene no dejarlo en Descargas.
3. Mueve `WRAM.exe` a esa carpeta y ejecútalo.
4. La primera vez se abre la ventana de configuración y aparece la franja junto a la bandeja.

### Aviso de SmartScreen

El ejecutable no lleva firma de código con un certificado comercial, así que la primera ejecución puede mostrar "Windows protegió tu PC". Para continuar: "Más información" y luego "Ejecutar de todas formas". Las propiedades del archivo (clic derecho, Propiedades, Detalles) muestran el autor y la versión.

Para comprobar que el archivo es el publicado, compara su hash con el que figura en la página de la versión:

```powershell
Get-FileHash .\WRAM.exe -Algorithm SHA256
```

Con GitHub CLI también se puede verificar que lo compiló el CI de este repositorio:

```
gh attestation verify WRAM.exe --repo Ju4nMarin/WRAM
```

## Scoop

```powershell
scoop install https://github.com/Ju4nMarin/WRAM/releases/latest/download/wram.json
```

El manifiesto se publica con cada versión y siempre apunta a la última: `scoop update wram` la actualiza y `scoop uninstall wram` la quita. Scoop agrega un acceso directo de WRAM al menú Inicio.

## Iniciar con Windows

En la configuración, sección Configuración, activa "Iniciar con Windows". Se registra en `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` solo para tu usuario.

## Actualizar

1. Clic derecho en la franja o en el ícono de la bandeja y "Salir de WRAM".
2. Reemplaza `WRAM.exe` por la versión nueva en la misma carpeta.
3. Ejecútalo. La configuración se conserva.

## Desinstalar

1. En Configuración, desactiva "Iniciar con Windows".
2. Sal de WRAM desde el menú de la franja o de la bandeja.
3. Borra `WRAM.exe` y, si quieres eliminar también la configuración, estas carpetas:

   | Carpeta | Contenido |
   |---|---|
   | `%APPDATA%\WRAM` | `config.ini` |
   | `%LOCALAPPDATA%\WRAM` | Datos de la ventana de configuración (WebView2) |

## Compilar desde el código

Ver [desarrollo.md](desarrollo.md).
