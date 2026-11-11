# Modelo: creación y entrenamiento del autoencoder

Entorno de Python 3.12 con TensorFlow (GPU por CUDA) y JupyterLab para crear,
entrenar y exportar el autoencoder que corre en el M7 de la STM32H745.

## Crear el entorno

```bash
cd pc/model
sh setup_env.sh
```

Crea `.venv/` (no se sube a git), instala `requirements.txt` (TensorFlow con
CUDA y cuDNN vía pip; solo hace falta el driver de NVIDIA) y registra el
kernel **"Anomaly detector (TF)"** para Jupyter.

`cuda_paths.sh` (lo llama `setup_env.sh`) agrega a `LD_LIBRARY_PATH` las
carpetas de las bibliotecas de CUDA del entorno, en `.venv/bin/activate` y en
el kernel de Jupyter; sin eso TensorFlow no encuentra `libcusolver.so.11` y
no usa la GPU. Si el entorno ya existe, basta con `sh cuda_paths.sh`.

## Usar

```bash
cd pc/model
.venv/bin/jupyter lab
```

Abrir `notebooks/00_verificar_gpu.ipynb` para comprobar que TensorFlow ve la
GPU (RTX 4050, 6 GB).

`notebooks/01_autoencoder.ipynb` entrena el autoencoder con los WAV de
`data/normal/` (motor sano; `data/anomalia/` es opcional, solo para evaluar),
calcula los umbrales y exporta a `models/` el `.tflite` (float32, el recomendado, e int8) y
`autoencoder_params.h` (features, normalización y umbrales para el firmware).
Sin datos, usa audio sintético de demostración.

Con `DATASET = 'toycar'` entrena con ToyCar del DCASE 2020 Task 2 (el dataset
del benchmark de anomalías de MLPerf Tiny), para probar el flujo con datos
reales. Descarga (1.8 GB):

```bash
cd pc/model/data
curl -L -o ToyCar.zip https://zenodo.org/api/records/3678171/files/dev_data_ToyCar.zip/content
unzip -q ToyCar.zip && rm ToyCar.zip      # crea data/ToyCar/{train,test}
```

## Carpetas

| Carpeta | Contenido |
|---|---|
| `notebooks/` | Notebooks de exploración y entrenamiento |
| `data/` | Grabaciones y datasets (no se suben a git) |
| `models/` | Modelos exportados (`.tflite` float32 e int8 para X-CUBE-AI / TFLite Micro) |
