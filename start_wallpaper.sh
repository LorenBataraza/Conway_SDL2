#!/bin/bash
# start_wallpaper.sh - Conway's Game of Life como fondo de escritorio
# 
# Esta versión renderiza directamente en la root window de X11.
# Funciona con cualquier window manager.
# No requiere xwinwrap.
#
# Uso:
#   ./start_wallpaper.sh           # Iniciar como wallpaper
#   ./start_wallpaper.sh windowed  # Modo ventana (testing)
#   ./start_wallpaper.sh stop      # Detener
#   ./start_wallpaper.sh restart   # Reiniciar

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WALLPAPER_BIN="${SCRIPT_DIR}/build/life_wallpaper"
LOG_FILE="/tmp/life_wallpaper.log"
PID_FILE="/tmp/life_wallpaper.pid"

check_binary() {
    if [[ ! -x "$WALLPAPER_BIN" ]]; then
        echo "Error: No se encuentra $WALLPAPER_BIN"
        echo ""
        echo "Compilá primero:"
        echo "  cd build && cmake .. && make life_wallpaper"
        exit 1
    fi
}

stop_wallpaper() {
    echo "Deteniendo wallpaper..."
    
    # Intentar con el PID guardado
    if [[ -f "$PID_FILE" ]]; then
        kill $(cat "$PID_FILE") 2>/dev/null
        rm -f "$PID_FILE"
    fi
    
    # Por si acaso, matar por nombre
    pkill -f "life_wallpaper" 2>/dev/null
    sleep 0.3
    
    # Forzar si sigue corriendo
    if pgrep -f "life_wallpaper" > /dev/null; then
        pkill -9 -f "life_wallpaper" 2>/dev/null
    fi
    
    echo "✓ Wallpaper detenido"
}

start_wallpaper() {
    check_binary
    
    # Detener instancia anterior
    if pgrep -f "life_wallpaper" > /dev/null; then
        stop_wallpaper
    fi

    echo "Iniciando Conway's Game of Life Wallpaper..."
    echo ""
    
    # Detectar desktop environment
    DE="${XDG_CURRENT_DESKTOP:-unknown}"
    echo "[INFO] Desktop Environment: $DE"
    
    # Advertencia para GNOME
    if [[ "$DE" == *"GNOME"* ]]; then
        echo "[NOTA] En GNOME, si Nautilus cubre el wallpaper, ejecutá:"
        echo "       gsettings set org.gnome.desktop.background show-desktop-icons false"
        echo ""
    fi
    
    # Ejecutar en background
    nohup "$WALLPAPER_BIN" > "$LOG_FILE" 2>&1 &
    echo $! > "$PID_FILE"
    
    sleep 1
    
    if pgrep -f "life_wallpaper" > /dev/null; then
        echo "✓ Wallpaper iniciado"
        echo "  PID: $(cat $PID_FILE)"
        echo "  Log: $LOG_FILE"
        echo ""
        echo "Para detener: $0 stop"
    else
        echo "✗ Error al iniciar"
        echo "Ver log: cat $LOG_FILE"
        rm -f "$PID_FILE"
        exit 1
    fi
}

start_windowed() {
    check_binary
    echo "Iniciando en modo ventana..."
    "$WALLPAPER_BIN" --windowed
}

show_status() {
    if pgrep -f "life_wallpaper" > /dev/null; then
        PID=$(pgrep -f "life_wallpaper")
        echo "✓ Wallpaper corriendo"
        echo "  PID: $PID"
        if [[ -f "$LOG_FILE" ]]; then
            echo "  Log: $LOG_FILE"
            echo ""
            echo "Últimas líneas del log:"
            tail -5 "$LOG_FILE"
        fi
    else
        echo "✗ Wallpaper no está corriendo"
    fi
}

show_log() {
    if [[ -f "$LOG_FILE" ]]; then
        tail -f "$LOG_FILE"
    else
        echo "No hay log disponible"
    fi
}

case "${1:-start}" in
    start)
        start_wallpaper
        ;;
    windowed|test|w)
        start_windowed
        ;;
    stop)
        stop_wallpaper
        ;;
    restart)
        stop_wallpaper
        sleep 0.5
        start_wallpaper
        ;;
    status)
        show_status
        ;;
    log)
        show_log
        ;;
    *)
        echo "Conway's Game of Life - Wallpaper v3"
        echo ""
        echo "Uso: $0 {start|stop|restart|windowed|status|log}"
        echo ""
        echo "Comandos:"
        echo "  start     Iniciar como wallpaper (default)"
        echo "  stop      Detener"
        echo "  restart   Reiniciar"
        echo "  windowed  Modo ventana (testing)"
        echo "  status    Ver si está corriendo"
        echo "  log       Ver log en tiempo real"
        exit 1
        ;;
esac
