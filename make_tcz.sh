#!/bin/sh
# By IamJony
set -e

D=$(cd "$(dirname "$0")" && pwd)
APP=RamenTV
VER=1.0.0
OUT="$D/tcz"
STAGE="$D/tcz-stage"
# Carpeta destino: mismo layout que el proyecto (binario + asset/ + A-CLI/)
PKG_DIR="/usr/local/share/RamenTV"

# --- Verificaciones previas ---
[ -x "$D/RamenTV" ] || { echo "ERROR: compila primero (make)"; exit 1; }
[ -f "$D/A-CLI/A-CLI.sh" ] || { echo "ERROR: falta A-CLI/A-CLI.sh"; exit 1; }
[ -f "$D/A-CLI/modules/search.sh" ] || { echo "ERROR: falta A-CLI/modules/search.sh"; exit 1; }

# Instalar squashfs-tools si falta
if ! command -v mksquashfs >/dev/null 2>&1; then
    tce-load -wi squashfs-tools >/dev/null 2>&1 || true
fi
command -v mksquashfs >/dev/null 2>&1 || { echo "ERROR: falta mksquashfs"; exit 1; }

# Detectar soporte -no-xattrs (mksquashfs viejo no lo tiene)
MKSQ_OPTS="-all-root -noappend -no-progress -b 4096"
if mksquashfs 2>&1 | grep -q -- "-no-xattrs"; then
    MKSQ_OPTS="$MKSQ_OPTS -no-xattrs"
fi

# --- Preparar stage ---
rm -rf "$STAGE"
mkdir -p "$STAGE/usr/local/bin"
mkdir -p "$STAGE$PKG_DIR/asset"
mkdir -p "$STAGE$PKG_DIR/A-CLI/modules"
mkdir -p "$STAGE/usr/local/share/pixmaps"
mkdir -p "$STAGE/usr/local/share/applications"

# *** Binario DENTRO de la carpeta del paquete, junto a asset/ y A-CLI/ ***
install -m 755 "$D/RamenTV" "$STAGE$PKG_DIR/RamenTV"

# CLI principal
install -m 755 "$D/A-CLI/A-CLI.sh" "$STAGE$PKG_DIR/A-CLI/A-CLI.sh"

# Módulos CLI
if [ -d "$D/A-CLI/modules" ]; then
    for f in "$D/A-CLI/modules/"*.sh; do
        [ -f "$f" ] || continue
        install -m 755 "$f" "$STAGE$PKG_DIR/A-CLI/modules/"
    done
fi

# Asset (home.jpeg)
if [ -f "$D/asset/home.jpeg" ]; then
    install -m 644 "$D/asset/home.jpeg" "$STAGE$PKG_DIR/asset/home.jpeg"
else
    echo "WARN: no existe asset/home.jpeg (la GUI podría quejarse)"
fi

# --- Wrappers en /usr/local/bin ---
# CLI: cd a A-CLI/ y ejecuta el script (útil si lo llamas desde terminal manualmente)
cat > "$STAGE/usr/local/bin/RamenTV-cli-launcher" <<'WRAPPER'
#!/bin/sh
PKG=/usr/local/share/RamenTV
cd "$PKG/A-CLI" || exit 1
exec bash ./A-CLI.sh "$@"
WRAPPER
chmod 755 "$STAGE/usr/local/bin/RamenTV-cli-launcher"

# GUI: cd a la carpeta del paquete (donde vive el binario junto a asset/ y A-CLI/)
cat > "$STAGE/usr/local/bin/RamenTV-launcher" <<'WRAPPER'
#!/bin/sh
PKG=/usr/local/share/RamenTV
cd "$PKG" || exit 1
exec "$PKG/RamenTV" "$@"
WRAPPER
chmod 755 "$STAGE/usr/local/bin/RamenTV-launcher"

# --- Icono ---
[ -f "$D/data/RamenTV.png" ] && \
    install -m 644 "$D/data/RamenTV.png" "$STAGE/usr/local/share/pixmaps/RamenTV.png"

# --- .desktop (SOLO GUI) ---
if [ -f "$D/RamenTV.desktop" ]; then
    install -m 644 "$D/RamenTV.desktop" "$STAGE/usr/local/share/applications/RamenTV.desktop"
else
    cat > "$STAGE/usr/local/share/applications/RamenTV.desktop" <<'DESK'
[Desktop Entry]
Encoding=UTF-8
Type=Application
Name=RamenTV
Comment=Mira anime subtitulado en español
Exec=/usr/local/bin/RamenTV-launcher
Icon=RamenTV
X-FullPathIcon=/usr/local/share/pixmaps/RamenTV.png
Terminal=false
Categories=Video;AudioVideo;
DESK
fi

# --- Permisos ---
find "$STAGE" -type d -exec chmod 755 {} +

# --- Empaquetar ---
mkdir -p "$OUT"
mksquashfs "$STAGE" "$OUT/$APP.tcz" $MKSQ_OPTS >/dev/null

# Lista de archivos (todo el árbol)
(cd "$STAGE" && find . -type f | sed 's|^\./|/|' | sort) > "$OUT/$APP.tcz.list"

# Limpiar stage
rm -rf "$STAGE"

# Dependencias
printf '%s\n' fltk-1.3.tcz jq.tcz bash.tcz curl.tcz wget.tcz yt-dlp.tcz mplayer-cli.tcz > "$OUT/$APP.tcz.dep"

# Checksum
(cd "$OUT" && md5sum "$APP.tcz" > "$APP.tcz.md5.txt")

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
Comments:       Incluye GUI (RamenTV) y CLI (RamenTV-cli-launcher).
Change-log:     $(date +%Y/%m/%d) $VER primera version
Current:        $(date +%Y/%m/%d) $VER
INFO

echo "OK: $OUT/$APP.tcz (${SIZE}K)"
