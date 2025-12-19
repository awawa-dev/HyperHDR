import numpy as np
import sys, io
import requests
import os
import argparse
from PIL import Image

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

parser = argparse.ArgumentParser()
parser.add_argument("--outdir", type=str, default=".", help="Output filder")
parser.add_argument("--input", type=str, default="", help="Input image (optional)")
args = parser.parse_args()

outdir = os.path.abspath(args.outdir)
os.makedirs(outdir, exist_ok=True)

width, height = 1920, 1080
DEFAULT_URL = "https://i.ibb.co/23JY5YzG/test-image.jpg"
base_name = "test_image"

# --------------------------------------
# Load image
# --------------------------------------
if args.input:
    try:
        img = Image.open(args.input).convert("RGB")
    except Exception:
        print(f"Cannot load the image: {args.input}")
        sys.exit(1)
else:
    print("Downloading default image...")
    resp = requests.get(DEFAULT_URL)
    if resp.status_code != 200:
        print("Cannot download the image")
        sys.exit(1)
    try:
        img = Image.open(io.BytesIO(resp.content)).convert("RGB")
    except Exception:
        print("Cannot load the image")
        sys.exit(1)

# --------------------------------------
# Scale to 1080p and konwert to RGB
# --------------------------------------
img = img.resize((width, height), Image.Resampling.LANCZOS)
rgb = np.array(img, dtype=np.uint8)
rgb_flat = rgb.flatten()

def outpath(name: str) -> str:
    return os.path.join(outdir, f"{base_name}_{name}.bin")

# --------------------------------------
# RGB24 (BGR bottom-up)
# --------------------------------------
bgr = np.flipud(rgb[..., ::-1]).flatten()
bgr.tofile(outpath("rgb24"))

# --------------------------------------
# XRGB32 (padding X=0)
# --------------------------------------
xbgr32 = np.zeros(width * height * 4, dtype=np.uint8)
xbgr = np.flipud(rgb[..., ::-1]).reshape(-1, 3)  # BGR, bottom-up

xbgr32[0::4] = xbgr[:, 0]   # B
xbgr32[1::4] = xbgr[:, 1]   # G
xbgr32[2::4] = xbgr[:, 2]   # R
xbgr32[3::4] = 0            # X

xbgr32.tofile(outpath("xrgb32"))

# --------------------------------------
# YUV444 (planar)
# --------------------------------------
rgbf = rgb.astype(np.float32)

M = np.array([[ 0.299,     0.587,     0.114   ],
              [-0.168736, -0.331264,  0.5     ],
              [ 0.5,      -0.418688, -0.081312]], dtype=np.float32)

yuv444 = rgbf @ M.T
yuv444[..., 1:] += 128.0  # offset to U/V
yuv444 = np.clip(yuv444, 0, 255).astype(np.uint8)

Y = yuv444[:, :, 0].astype(np.uint16)
U = yuv444[:, :, 1].astype(np.uint16)
V = yuv444[:, :, 2].astype(np.uint16)

Y = np.clip(((Y * 219) // 255) + 16, 16, 235)
U = np.clip(((U * 224) // 255) + 16, 16, 240)
V = np.clip(((V * 224) // 255) + 16, 16, 240)

Y = Y.flatten()
U = U.flatten()
V = V.flatten()

# --------------------------------------
# YUV420 / I420 (planar Y U V)
# --------------------------------------
yuv420 = np.zeros(width * height + (width // 2) * (height // 2) * 2, dtype=np.uint8)

# Y
yuv420[:width*height] = Y[:width*height].astype(np.uint8)

# U i V subsampling
uv_index = width * height
for y in range(0, height, 2):
    for x in range(0, width, 2):
        idx0 = y * width + x
        idx1 = idx0 + 1
        idx2 = idx0 + width
        idx3 = idx2 + 1
        u_avg = (U[idx0] + U[idx1] + U[idx2] + U[idx3]) // 4
        v_avg = (V[idx0] + V[idx1] + V[idx2] + V[idx3]) // 4
        yuv420[uv_index] = u_avg
        yuv420[uv_index + (width//2)*(height//2)] = v_avg
        uv_index += 1

yuv420.tofile(outpath("yuv420"))

# --------------------------------------
# NV12 (Y + UV interleaved)
# --------------------------------------
nv12 = np.zeros(height*width + (height//2)*width, dtype=np.uint8)
nv12[:height*width] = Y

for y in range(0, height, 2):
    for x in range(0, width, 2):
        idx0 = y*width + x
        idx1 = idx0 + 1
        idx2 = idx0 + width
        idx3 = idx2 + 1
        u = (U[idx0] + U[idx1] + U[idx2] + U[idx3]) // 4
        v = (V[idx0] + V[idx1] + V[idx2] + V[idx3]) // 4
        uv_index = height*width + (y//2)*width + x
        nv12[uv_index]   = u
        nv12[uv_index+1] = v

nv12.tofile(outpath("nv12"))

# --------------------------------------
# YUYV (4:2:2 packed)
# --------------------------------------
yuyv = np.zeros(height*width*2, dtype=np.uint8)
for y in range(height):
    for x in range(0, width, 2):
        idx0 = y*width + x
        idx1 = idx0 + 1
        Y0, Y1 = Y[idx0], Y[idx1]
        U0, V0 = U[idx0], V[idx0]
        base = (y*width + x) * 2
        yuyv[base+0] = Y0
        yuyv[base+1] = U0
        yuyv[base+2] = Y1
        yuyv[base+3] = V0
yuyv.tofile(outpath("yuyv"))

# --------------------------------------
# UYVY (4:2:2 packed)
# --------------------------------------
uyvy = np.zeros(height*width*2, dtype=np.uint8)
for y in range(height):
    for x in range(0, width, 2):
        idx0 = y*width + x
        idx1 = idx0 + 1
        Y0, Y1 = Y[idx0], Y[idx1]
        U0, V0 = U[idx0], V[idx0]
        base = (y*width + x) * 2
        uyvy[base+0] = U0
        uyvy[base+1] = Y0
        uyvy[base+2] = V0
        uyvy[base+3] = Y1
uyvy.tofile(outpath("uyvy"))

# --------------------------------------
# P010 (10-bit, little endian, NV12 layout)
# --------------------------------------
size_y  = width * height
size_uv = (width // 2) * (height // 2) * 2
p010 = np.zeros(size_y + size_uv, dtype=np.uint16)

Y10 = ((Y.astype(np.uint32) * 1023) // 255) << 6
p010[:size_y] = Y10.astype(np.uint16)

uv_index = size_y
for y in range(0, height, 2):
    for x in range(0, width, 2):
        idx0 = y * width + x
        idx1 = idx0 + 1
        idx2 = idx0 + width
        idx3 = idx2 + 1

        u_avg = (int(U[idx0]) + int(U[idx1]) + int(U[idx2]) + int(U[idx3])) // 4
        v_avg = (int(V[idx0]) + int(V[idx1]) + int(V[idx2]) + int(V[idx3])) // 4

        u10 = (u_avg * 1023 // 255) << 6
        v10 = (v_avg * 1023 // 255) << 6

        p010[uv_index]   = np.uint16(u10)
        p010[uv_index+1] = np.uint16(v10)
        uv_index += 2

p010.astype('<u2').tofile(outpath("p010"))

print("All binary frames generated correctly")
