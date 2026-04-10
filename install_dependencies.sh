#!/bin/bash
 
set -e
 
echo "Instalando dependencias para Conway Life Game..."
 
sudo apt update
 
sudo apt install -y \
    libsdl2-dev \
    libsdl2-ttf-dev \
    libsdl2-image-dev \
    libncurses-dev
 
echo "Instalación completada."
