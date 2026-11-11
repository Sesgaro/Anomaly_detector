#!/bin/sh
# Crea el entorno virtual .venv (Python 3.12) con TensorFlow + CUDA y Jupyter,
# y registra el kernel "Anomaly detector (TF)" para Jupyter.
# Uso: sh setup_env.sh   (desde pc/model/)
set -e
cd "$(dirname "$0")"
uv venv --python 3.12 .venv
uv pip install --python .venv/bin/python -r requirements.txt
sh cuda_paths.sh      # rutas de CUDA + kernel de Jupyter
