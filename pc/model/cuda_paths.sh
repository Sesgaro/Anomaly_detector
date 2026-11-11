#!/bin/sh
# Hace visibles para TensorFlow las bibliotecas de CUDA instaladas por pip en
# .venv (algunas, como libcusolver.so.11, no las encuentra por sí solo):
#   - agrega sus carpetas a LD_LIBRARY_PATH en .venv/bin/activate
#   - registra el kernel de Jupyter con esa misma variable
# Uso: sh cuda_paths.sh   (desde pc/model/; setup_env.sh ya lo llama)
set -e
cd "$(dirname "$0")"
SITE=$(.venv/bin/python -c "import site; print(site.getsitepackages()[0])")
NV=$(find "$SITE/nvidia" -name 'lib*.so*' -printf '%h\n' | sort -u | paste -sd: -)

MARK="# rutas de CUDA (cuda_paths.sh)"
if ! grep -q "$MARK" .venv/bin/activate; then
    printf '\n%s\nexport LD_LIBRARY_PATH="%s${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n' \
        "$MARK" "$NV" >> .venv/bin/activate
fi

.venv/bin/python -m ipykernel install --user --name anomaly-tf \
    --display-name "Anomaly detector (TF)" --env LD_LIBRARY_PATH "$NV"

LD_LIBRARY_PATH="$NV" .venv/bin/python -c "import tensorflow as tf; print('GPU:', tf.config.list_physical_devices('GPU'))"
