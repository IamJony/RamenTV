#!/bin/sh
# By IamJony
set -e

D=$(cd "$(dirname "$0")" && pwd)
APP=RamenTV
VER=1.0.0
OUT="$D/tcz"
STAGE="$D/tcz-stage"

# Verificar binario y scripts
[ -x "$D/RamenTV" ] || { echo "ERROR: compila primero (make)"; exit 1; }
[ -f "$D/A-CLI/A-CLI.sh" ] || { echo "ERROR: falta A-CLI/A-CLI.sh"; exit 1; }
[ -f "$D/A-CLI/modules/search.sh" ] || { echo "ERROR: falta A-CLI/modules/search.sh"; exit 1; }

# Instalar squashfs-tools si falta
if ! command -v mksquashfs >/dev/null 2>&1; then
    tce-load -wi squashfs-tools >/dev/null 2>&1 || true
fi
command -v mksquashfs >/dev/null 2>&1 || { echo "ERROR: falta mksquashfs"; exit 1; }

# Preparar stage (raíz falsa)
rm -rf "$STAGE"
mkdir -p "$STAGE/usr/local/bin/RamenTV-cli"
mkdir -p "$STAGE/usr/local/share/RamenTV/asset"
mkdir -p "$STAGE/usr/local/share/pixmaps"
mkdir -p "$STAGE/usr/local/share/applications"

# Binario GUI (renombrado a RamenTV-fltk dentro del paquete)
install -m 755 "$D/RamenTV" "$STAGE/usr/local/bin/RamenTV-fltk"

# Scripts CLI en /usr/local/bin/RamenTV-cli/
install -m 755 "$D/A-CLI/A-CLI.sh" "$STAGE/usr/local/bin/RamenTV-cli/A-CLI.sh"
if [ -d "$D/A-CLI/modules" ]; then
    for f in "$D/A-CLI/modules/"*.sh; do
        [ -f "$f" ] || continue
        install -m 755 "$f" "$STAGE/usr/local/bin/RamenTV-cli/"
    done
fi

# Asset (imagen de fondo que busca la GUI)
[ -f "$D/asset/home.jpeg" ] && \
    install -m 644 "$D/asset/home.jpeg" "$STAGE/usr/local/share/RamenTV/asset/home.jpeg"

# Wrapper CLI en /usr/local/bin/RamenTV-cli-launcher (no choca con el directorio)
cat > "$STAGE/usr/local/bin/RamenTV-cli-launcher" <<'WRAPPER'
#!/bin/sh
export RAMENTV_HOME=/usr/local/share/RamenTV
cd /usr/local/bin/RamenTV-cli/ && exec bash ./A-CLI.sh "$@"
WRAPPER
chmod 755 "$STAGE/usr/local/bin/RamenTV-cli-launcher"

# Icono y .desktop (condicionales)
# IMPORTANTE: APP=RamenTV → los .desktop DEBEN llamarse RamenTV.desktop y RamenTV-cli.desktop
[ -f "$D/data/RamenTV.png" ] && \
    install -m 644 "$D/data/RamenTV.png" "$STAGE/usr/local/share/pixmaps/RamenTV.png"
[ -f "$D/RamenTV.desktop" ] && \
    install -m 644 "$D/RamenTV.desktop" "$STAGE/usr/local/share/applications/RamenTV.desktop"


# Permisos
find "$STAGE" -type d -exec chmod 755 {} +

# Empaquetar
mkdir -p "$OUT"
mksquashfs "$STAGE" "$OUT/$APP.tcz" -all-root -noappend -no-progress -b 4096 -no-xattrs >/dev/null
(cd "$STAGE" && find usr -type f | sed 's|^|/|' | sort) > "$OUT/$APP.tcz.list"

# Limpiar stage
rm -rf "$STAGE"

# Dependencias
printf '%s\n' fltk-1.3.tcz jq.tcz bash.tcz curl.tcz wget.tcz yt-dlp.tcz mplayer-cli.tcz > "$OUT/$APP.tcz.dep"

# Checksum
(cd "$OUT" && md5sum $APP.tcz > $APP.tcz.md5.txt)

# Info
SIZE=$(du -k "$OUT/$APP.tcz" | cut -f1)
cat > "$OUT/$APP.tcz.info" <<INFO
Title:          $APP.tcz
Description:    RamenTV: ver anime subtitulado en espanol
Version:        $VER
Author:         IamJony
Original-site:  https://github.com/IamJony/RamenTV
Copying-policy: MIT
Size:           ${SIZE}K
Extension_by:   IamJony
Tags:           anime video fltk streaming cli
Comments:       Incluye GUI (RamenTV-fltk) y CLI (RamenTV-cli-launcher).
Change-log:     $(date +%Y/%m/%d) $VER primera version
Current:        $(date +%Y/%m/%d) $VER
INFO

echo "OK: $OUT/$APP.tcz (${SIZE}K)"
