#!/bin/bash
echo "Compilando filosofos.c..."
gcc -o filosofos filosofos.c -lpthread
echo "Compilación completada."
echo ""
echo "Para ejecutar:"
echo "  Versión con interbloqueo:   ./filosofos naive"
echo "  Versión sin interbloqueo:   ./filosofos semaforo"