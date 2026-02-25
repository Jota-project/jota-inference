#!/usr/bin/env python3
import json
import time
import sys
import os

LOG_FILE = "logs/inference.log"

# Códigos ANSI para colores en la terminal
COLORS = {
    "INFO": "\033[92m",   # Verde
    "WARN": "\033[93m",   # Amarillo
    "ERROR": "\033[91m",  # Rojo
    "DEBUG": "\033[94m",  # Azul
    "RESET": "\033[0m",
    "GRAY": "\033[90m",
    "CYAN": "\033[96m"
}

def format_log(line):
    try:
        data = json.loads(line)
        ts = data.get("timestamp", "")
        level = data.get("level", "INFO")
        msg = data.get("message", "")
        file_name = data.get("file", "")
        line_num = data.get("line", "")
        thread_id = data.get("thread_id", "")
        
        color = COLORS.get(level, COLORS["RESET"])
        
        # Formateo visualmente agradable
        formatted = f"{COLORS['GRAY']}[{ts}]{COLORS['RESET']} "
        formatted += f"{color}[{level:5}]{COLORS['RESET']} "
        formatted += f"{msg} "
        formatted += f"{COLORS['GRAY']}({file_name}:{line_num} | Thread: {thread_id}){COLORS['RESET']}"
        
        # Si hay metadata extra (el campo 'extra')
        if "extra" in data:
            formatted += f"\n    {COLORS['CYAN']}↳ Extra:{COLORS['RESET']} {json.dumps(data['extra'])}"
            
        return formatted
    except json.JSONDecodeError:
        return line.strip()

def follow(file):
    # Ir al final del archivo si solo queremos ver los nuevos
    # file.seek(0, os.SEEK_END) 
    # (Comentado para que lea desde el principio al arrancar, descomentar si solo quieres logs en vivo)
    
    while True:
        line = file.readline()
        if not line:
            time.sleep(0.1)
            continue
        yield line

if __name__ == "__main__":
    if not os.path.exists(LOG_FILE):
        print(f"{COLORS['ERROR']}Error: No se encontró el archivo {LOG_FILE}{COLORS['RESET']}")
        print("Asegúrate de ejecutar este script desde la raíz del proyecto (donde está la carpeta logs/).")
        sys.exit(1)

    print(f"{COLORS['CYAN']}=== Observando logs en {LOG_FILE} (Presiona Ctrl+C para salir) ==={COLORS['RESET']}\n")
    try:
        with open(LOG_FILE, "r") as f:
            for line in follow(f):
                print(format_log(line))
    except KeyboardInterrupt:
        print(f"\n{COLORS['GRAY']}Saliendo del visor de logs...{COLORS['RESET']}")
        sys.exit(0)
