#!/usr/bin/env python3

import argparse
import pickle
from utility_functions.ocg import keras_compile
import numpy as np

parser = argparse.ArgumentParser(description='Compare normal net with quantized.')

parser.add_argument('-b', '--database-path', dest='imgdb_path',
                    help='Path to the image database to use for training. '
                         'Default is img.db in current folder.')
parser.add_argument('-m', '--model-path', dest='model_path',
                    help='Store the trained model using this path. Default is model.h5.')


args = parser.parse_args()

imgdb_path = "img.db"
model_path = "model.h5"

if args.imgdb_path is not None:
    imgdb_path = args.imgdb_path

if args.model_path is not None:
    model_path = args.model_path


images = {}
with open(imgdb_path, "rb") as f:
    images["mean"] = pickle.load(f)
    images["y"] = pickle.load(f)
with open(imgdb_path + '.x', "rb") as f:
    images["images"] = np.load(f)

keras_compile(images, model_path, "", unroll_level=2, arch="ssse3", quantize=True, eval=True)
keras_compile(images, model_path, "", unroll_level=2, arch="general", quantize=False, eval=True)
