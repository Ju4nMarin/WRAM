# Prepara settings.html para incrustarlo en el ejecutable:
# reemplaza los marcadores de logo por los PNG en base64 de assets/ y quita sangría,
# líneas vacías y comentarios de línea completa (// y /* */). No reescribe CSS ni JS.
file(READ "${IN}" html)
file(READ "${ASSETS}/logo-horizontal-light.b64" logo_h)
file(READ "${ASSETS}/logo-mark.b64" logo_m)
string(STRIP "${logo_h}" logo_h)
string(STRIP "${logo_m}" logo_m)
string(REPLACE "__LOGO_HORIZONTAL__" "data:image/png;base64,${logo_h}" html "${html}")
string(REPLACE "__LOGO_MARK__" "data:image/png;base64,${logo_m}" html "${html}")
string(REPLACE "\r" "" html "${html}")
string(REGEX REPLACE "\n[ \t]+" "\n" html "${html}")
string(REGEX REPLACE "\n//[^\n]*" "" html "${html}")
string(REGEX REPLACE "\n/\\*[^\n]*\\*/" "" html "${html}")
string(REGEX REPLACE "\n\n+" "\n" html "${html}")
file(WRITE "${OUT}" "${html}")
