#!/bin/sh
# Genera RamenTV.tcz para Tiny Core Linux
# Autor: IamJony
set -e

D=$(cd "$(dirname "$0")" && pwd)
APP=RamenTV
VER=1.0.0
OUT="$D/tcz"
STAGE="$D/tcz-stage"

# ── Verificaciones básicas ───────────────────────────────────
[ -x "$D/RamenTV" ]                 || { echo "ERROR: compila primero (make)"; exit 1; }
[ -f "$D/A-CLI/A-CLI.sh" ]          || { echo "ERROR: falta A-CLI/A-CLI.sh"; exit 1; }
[ -f "$D/A-CLI/modules/search.sh" ] || { echo "ERROR: falta A-CLI/modules/search.sh"; exit 1; }
[ -f "$D/RamenTV.desktop" ]         || { echo "ERROR: falta RamenTV.desktop"; exit 1; }

# ── Instalar squashfs-tools si falta ─────────────────────────
if ! command -v mksquashfs >/dev/null 2>&1; then
    tce-load -wi squashfs-tools >/dev/null 2>&1 || true
fi
command -v mksquashfs >/dev/null 2>&1 || { echo "ERROR: falta mksquashfs"; exit 1; }

# ── Preparar stage ───────────────────────────────────────────
rm -rf "$STAGE"
mkdir -p "$STAGE/usr/local/bin"
mkdir -p "$STAGE/usr/local/share/RamenTV/A-CLI/modules"
mkdir -p "$STAGE/usr/local/share/RamenTV/asset"
mkdir -p "$STAGE/usr/local/share/pixmaps"
mkdir -p "$STAGE/usr/local/share/applications"

# ── Binario GUI ──────────────────────────────────────────────
install -m 755 "$D/RamenTV" "$STAGE/usr/local/bin/RamenTV"

# ── CLI + módulos ────────────────────────────────────────────
install -m 755 "$D/A-CLI/A-CLI.sh" \
    "$STAGE/usr/local/share/RamenTV/A-CLI/A-CLI.sh"

if [ -d "$D/A-CLI/modules" ]; then
    for f in "$D/A-CLI/modules/"*.sh; do
        [ -f "$f" ] || continue
        install -m 755 "$f" \
            "$STAGE/usr/local/share/RamenTV/A-CLI/modules/"
    done
fi

# ── Assets (imagen de fondo que busca la GUI) ───────────────
[ -f "$D/asset/home.jpeg" ] && \
    install -m 644 "$D/asset/home.jpeg" \
        "$STAGE/usr/local/share/RamenTV/asset/home.jpeg"

# ── Icono (PNG prioritario: TCL usa PNG 48x48 en la wbar) ───
ICON_SRC=""
ICON_DST=""
if [ -f "$D/data/RamenTV.png" ]; then
    ICON_SRC="$D/data/RamenTV.png"
    ICON_DST="/usr/local/share/pixmaps/RamenTV.png"
elif [ -f "$D/data/RamenTV.jpeg" ]; then
    ICON_SRC="$D/data/RamenTV.jpeg"
    ICON_DST="/usr/local/share/pixmaps/RamenTV.jpeg"
fi

if [ -n "$ICON_SRC" ]; then
    install -m 644 "$ICON_SRC" "$STAGE$ICON_DST"
else
    echo "AVISO: no se encontró icono en data/ (RamenTV.png o RamenTV.jpeg)"
fi

# ── Lanzador .desktop (nombre = paquete, X-FullPathIcon dinámico) ──
# Se genera en el stage para garantizar que X-FullPathIcon apunte al icono real.
{
    echo "[Desktop Entry]"
    echo "Name=RamenTV"
    echo "Comment=Mira anime subtitulado en español"
    echo "Exec=RamenTV"
    echo "Icon=RamenTV"
    if [ -n "$ICON_DST" ]; then
        echo "X-FullPathIcon=$ICON_DST"
    fi
    echo "Terminal=false"
    echo "Type=Application"
    echo "Categories=Video;"
} > "$STAGE/usr/local/share/applications/RamenTV.desktop"
chmod 644 "$STAGE/usr/local/share/applications/RamenTV.desktop"

# ── Wrapper CLI: /usr/local/bin/RamenTV-cli ─────────────────
cat > "$STAGE/usr/local/bin/RamenTV-cli" <<'WRAPPER'
#!/bin/sh
export RAMENTV_HOME=/usr/local/share/RamenTV
exec bash /usr/local/share/RamenTV/A-CLI/A-CLI.sh "$@"
WRAPPER
chmod 755 "$STAGE/usr/local/bin/RamenTV-cli"

# ── Permisos de directorios ──────────────────────────────────
find "$STAGE" -type d -exec chmod 755 {} +

# ── Empaquetar ───────────────────────────────────────────────
mkdir -p "$OUT"
rm -f "$OUT/$APP.tcz" "$OUT/$APP.tcz.list" "$OUT/$APP.tcz.dep" \
      "$OUT/$APP.tcz.md5.txt" "$OUT/$APP.tcz.info"

mksquashfs "$STAGE" "$OUT/$APP.tcz" \
    -all-root -noappend -no-progress -b 4096 -no-xattrs >/dev/null

(cd "$STAGE" && find usr -type f | sed 's|^|/|' | sort) > "$OUT/$APP.tcz.list"
rm -rf "$STAGE"

# ── Dependencias ─────────────────────────────────────────────
# cJSON se compila dentro del binario, así que no es dependencia.
cat > "$OUT/$APP.tcz.dep" <<'DEP'
fltk-1.3.tcz
jq.tcz
bash.tcz
curl.tcz
wget.tcz
yt-dlp.tcz
mplayer-cli.tcz
DEP

# ── Checksum ─────────────────────────────────────────────────
(cd "$OUT" && md5sum "$APP.tcz" > "$APP.tcz.md5.txt")

# ── Info ─────────────────────────────────────────────────────
SIZE=$(du -k "$OUT/$APP.tcz" | cut -f1)
cat > "$OUT/$APP.tcz.info" <<INFO
Title:          $APP.tcz
Description:    RamenTV: ver anime subtitulado en español (GUI FLTK + CLI)
Version:        $VER
Author:         IamJony
Original-site:  https://github.com/IamJony/RamenTV
Copying-policy: MIT
Size:           ${SIZE}K
Extension_by:   IamJony
Tags:           anime video fltk streaming cli
Comments:       GUI: RamenTV  |  CLI: RamenTV-cli
Change-log:     $(date +%Y/%m/%d) $VER primera version
Current:        $(date +%Y/%m/%d) $VER
INFO

echo "OK: $OUT/$APP.tcz (${SIZE}K)"
echo "Listo. Copia a tu repo TCL:"
echo "  cp $OUT/$APP.tcz* /etc/sysconfig/tcedir/optional/"
echo "  tce-load -i $APP"
