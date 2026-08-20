# Cooked asset packs

Raw Iron's mandatory cooked asset packs are mounted archives, not self-extracting folders.

## Runtime invariant

- `RipakArchive::Open` indexes validated STORE-entry byte ranges without writing files.
- `CookedTexturePack::Open` reads the compact logical texture index directly from the archive.
- `ReadPng` seeks to one deduplicated PNG blob, enforces a caller byte budget, and verifies CRC-32.
- Decoded pixels belong in bounded renderer memory caches. They must not be persisted as a second loose asset tree.
- Runtime rejects compression, encryption, ZIP64, data descriptors, unsafe paths, duplicate portable names,
  inconsistent headers, overlapping payload ranges, excessive entry counts, and excessive byte totals.

STORE is deliberate for the current texture pack: PNG payloads are already compressed, and storing them verbatim
allows direct range reads. A future GPU-native cooker may replace PNG blobs with platform texture blocks while
preserving the same logical mount contract.

Both software preview and native Vulkan accept an optional `CookedTexturePack`. They resolve logical material
paths through the mounted package before loose-file fallback, decode the selected PNG directly from memory, and
cache decoded/software or uploaded/GPU images by package and logical path. Cube Test exercises this path with a
small continuously spinning cube whose animated material cycles through RAWIRONX32 textures.

## RAWIRONX32

`Scripts/cook_texture_pack.py` cooks a PNG source tree into a deterministic, content-addressed `.ripak`:

- identical PNG files share one SHA-256-addressed blob;
- the runtime index maps every original logical path to its blob and image metadata;
- the adjacent `.files.txt` lists every archive entry and every source-to-blob mapping;
- `--force` validates a new archive before atomically replacing generated outputs;
- the human inventory is not duplicated inside the runtime pack.

Example:

```powershell
py -3 Scripts/cook_texture_pack.py `
  --source O:\Assets\Textures `
  --output O:\Assets\RAWIRONX32.ripak `
  --inventory O:\Assets\RAWIRONX32.files.txt `
  --package-id RAWIRONX32 `
  --force
```

Do not remove a loose source library merely because cooking succeeds. Removal is safe only after every editor and
renderer consumer of that library has switched from filesystem paths to the mounted asset-source interface. Source
assets retained for authoring should live with their project or in separately distributed source packs, not beside a
shipped mandatory cooked runtime copy.
