"""Train v2 architecture (3x3 s=2 / 12f) and export 30 demo images at 16x16.

The CNN input is 8x8 — unchanged. But demo images are STORED at 16x16 so the
Game Boy can display them sharply (no blocky 2x upscale). At inference time
the GB downsamples 16x16 -> 8x8 by 2x2 averaging.

To keep the stored 16x16 faithful, we:
  1. Take sklearn digits (8x8).
  2. Upscale to 16x16 with bicubic interpolation (smooth, not blocky).
  3. Quantize to 0..16 for storage.
The 2x2-average downsample of that 16x16 closely matches the original 8x8,
so accuracy is preserved.

NOTE: real MNIST (28x28) would be sharper, but it is not reachable from this
build environment (network is restricted to package hosts). The pipeline is
identical — only the image source in this script would change.
"""
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader, TensorDataset
from sklearn.datasets import load_digits
from sklearn.model_selection import train_test_split
import numpy as np

torch.manual_seed(42); np.random.seed(42)

N_FILTERS = 12
KERNEL = 3
STRIDE = 2
OUT_SIZE = 3
FEAT_LEN = N_FILTERS * OUT_SIZE * OUT_SIZE
N_CLASSES = 10
N_PER_DIGIT = 3
N_TEST_IMAGES = 10 * N_PER_DIGIT

class M(nn.Module):
    def __init__(self):
        super().__init__()
        self.conv = nn.Conv2d(1, N_FILTERS, KERNEL, stride=STRIDE, bias=True)
        self.fc = nn.Linear(FEAT_LEN, N_CLASSES, bias=True)
    def forward(self, x):
        return self.fc(F.relu(self.conv(x)).view(x.size(0), -1))

# Data
d = load_digits()
X = d.images.astype(np.float32) / 16.0
y = d.target.astype(np.int64)
X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
loader = DataLoader(TensorDataset(torch.from_numpy(X_tr).unsqueeze(1), torch.from_numpy(y_tr)), batch_size=64, shuffle=True)
X_te_t = torch.from_numpy(X_te).unsqueeze(1)
y_te_t = torch.from_numpy(y_te)

# Train
m = M()
opt = torch.optim.Adam(m.parameters(), lr=2e-3)
sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=50)
for ep in range(50):
    m.train()
    for x, yb in loader:
        opt.zero_grad(); F.cross_entropy(m(x), yb).backward(); opt.step()
    sched.step()
m.eval()
with torch.no_grad():
    acc = (m(X_te_t).argmax(1) == y_te_t).float().mean().item()
print(f"Float test acc: {100*acc:.2f}%")

# Quantize
def qs(t, n=8):
    a = t.abs().max().item()
    if a == 0: return torch.zeros_like(t, dtype=torch.int32), 1.0
    qmax = 2**(n-1)-1
    s = a / qmax
    return torch.round(t/s).clamp(-qmax, qmax).to(torch.int32), s

q_cw, s_cw = qs(m.conv.weight.detach())
q_fw, s_fw = qs(m.fc.weight.detach())
s_in = 1.0/16.0
s_ca = s_cw * s_in
q_cb = torch.round(m.conv.bias.detach() / s_ca).clamp(-32768, 32767).to(torch.int32)
s_fo = s_ca * s_fw
q_fb = torch.round(m.fc.bias.detach() / s_fo).clamp(-(2**30), 2**30-1).to(torch.int32)

def qfwd_one(img8):
    """Integer forward pass on a single 8x8 uint8 image -> logits."""
    qw = q_cw.numpy().astype(np.int32)
    qb = q_cb.numpy().astype(np.int32)
    fcw = q_fw.numpy().astype(np.int32)
    fcb = q_fb.numpy().astype(np.int32)
    img = img8.astype(np.int32)
    feat = np.zeros((N_FILTERS, OUT_SIZE, OUT_SIZE), dtype=np.int32)
    for f in range(N_FILTERS):
        for i in range(OUT_SIZE):
            for j in range(OUT_SIZE):
                s = qb[f]
                for ki in range(3):
                    for kj in range(3):
                        s += qw[f, 0, ki, kj] * img[i*STRIDE+ki, j*STRIDE+kj]
                if s < 0: s = 0
                feat[f, i, j] = s
    flat = feat.reshape(-1)
    logits = np.zeros(10, dtype=np.int32)
    for c in range(10):
        s = fcb[c]
        for k in range(FEAT_LEN):
            s += fcw[c, k] * flat[k]
        logits[c] = s
    return logits

# ---- Upscale helper: 8x8 -> 16x16 bicubic ----
def upscale_16(img8_float):
    """img8_float: 8x8 in [0,1]. Returns 16x16 in [0,1]."""
    t = torch.from_numpy(img8_float).view(1, 1, 8, 8)
    up = F.interpolate(t, size=(16, 16), mode='bicubic', align_corners=False)
    return up.clamp(0, 1).view(16, 16).numpy()

# ---- Downsample helper: 16x16 -> 8x8 by 2x2 average ----
def downsample_8(img16_uint8):
    """img16_uint8: 16x16 values 0..16. Returns 8x8 values 0..16 (2x2 mean)."""
    out = np.zeros((8, 8), dtype=np.uint8)
    for r in range(8):
        for c in range(8):
            block = img16_uint8[r*2:r*2+2, c*2:c*2+2].astype(np.int32)
            out[r, c] = (block.sum() + 2) >> 2   # rounded average
    return out

# ---- Validate: store at 16x16, downsample, check accuracy ----
test_uint8 = np.clip(np.round(X_te * 16), 0, 16).astype(np.uint8)
correct = 0
for i in range(len(test_uint8)):
    img16f = upscale_16(X_te[i])
    img16u = np.clip(np.round(img16f * 16), 0, 16).astype(np.uint8)
    img8 = downsample_8(img16u)
    pred = qfwd_one(img8).argmax()
    if pred == y_te[i]:
        correct += 1
acc_pipeline = correct / len(test_uint8)
print(f"Quantized acc (16x16 store -> 8x8 downsample): {100*acc_pipeline:.2f}%")

# ---- Pick 3 distinct correctly-classified samples per digit ----
chosen16 = {dd: [] for dd in range(10)}
for i in range(len(test_uint8)):
    dv = int(y_te[i])
    if len(chosen16[dv]) >= N_PER_DIGIT:
        continue
    img16f = upscale_16(X_te[i])
    img16u = np.clip(np.round(img16f * 16), 0, 16).astype(np.uint8)
    img8 = downsample_8(img16u)
    if qfwd_one(img8).argmax() != dv:
        continue
    # distinct check on the 16x16
    distinct = True
    for ex in chosen16[dv]:
        if np.sum(np.abs(img16u.astype(int) - ex.astype(int))) < 80:
            distinct = False
            break
    if distinct:
        chosen16[dv].append(img16u)
print("Samples per digit:", {dd: len(v) for dd, v in chosen16.items()})

all_imgs16 = []
all_labels = []
for dd in range(10):
    for s in range(N_PER_DIGIT):
        all_imgs16.append(chosen16[dd][s])
        all_labels.append(dd)
img_arr16 = np.stack(all_imgs16).astype(np.uint8)   # (30,16,16)
label_arr = np.array(all_labels, dtype=np.uint8)
print(f"Demo images stored at: {img_arr16.shape}")

# ---- Export weights.h ----
def fmt_array(name, arr, ctype):
    flat = arr.flatten()
    lines = [f"const {ctype} {name}[{flat.size}] = {{"]
    for i in range(0, flat.size, 16):
        chunk = ", ".join(f"{int(v):>5d}" for v in flat[i:i+16])
        lines.append("    " + chunk + ",")
    lines.append("};")
    return "\n".join(lines)

H = []
H.append("// Auto-generated: CNN weights for Game Boy digit demo (v6)")
H.append("// Conv(12 @ 3x3, stride 2) -> ReLU -> 108 -> FC -> 10")
H.append("// Demo images STORED at 16x16, downsampled to 8x8 for inference.")
H.append("#ifndef WEIGHTS_H")
H.append("#define WEIGHTS_H")
H.append("#include <stdint.h>")
H.append("")
H.append("#define IMG_SIZE 8")
H.append("#define IMG_PIXELS 64")
H.append("#define IMG16_SIZE 16")
H.append("#define IMG16_PIXELS 256")
H.append("#define CONV_OUT_SIZE 3")
H.append("#define CONV_STRIDE 2")
H.append("#define CONV_FILTERS 12")
H.append("#define CONV_KERNEL 3")
H.append("#define FEAT_LEN 108")
H.append("#define NUM_CLASSES 10")
H.append(f"#define NUM_TEST_IMAGES {len(all_labels)}")
H.append("")
H.append(fmt_array("conv_w", q_cw.numpy().astype(np.int8).reshape(N_FILTERS, KERNEL, KERNEL), "int8_t"))
H.append("")
H.append(fmt_array("conv_b", q_cb.numpy().astype(np.int16), "int16_t"))
H.append("")
H.append(fmt_array("fc_w", q_fw.numpy().astype(np.int8), "int8_t"))
H.append("")
H.append(fmt_array("fc_b", q_fb.numpy().astype(np.int32), "int32_t"))
H.append("")
H.append("// test_images: 30 images, each 16x16 = 256 bytes, values 0..16")
H.append(fmt_array("test_images16", img_arr16, "uint8_t"))
H.append("")
H.append(fmt_array("test_labels", label_arr, "uint8_t"))
H.append("")
H.append("#endif")

with open("weights.h", "w") as f:
    f.write("\n".join(H))

print(f"\nWritten weights.h")
print(f"  Image data: 30 x 256 = {30*256} bytes (16x16 storage)")
print(f"  vs v5 8x8:  30 x 64  = {30*64} bytes")
print(f"  Extra ROM:  {30*256 - 30*64} bytes (~{(30*256-30*64)/1024:.1f} KB)")
