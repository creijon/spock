[t003] Gaussian Splatting

### Goal

Efficient 3DGS rendering using compute to bin, project and rasterise the gaussian splats.

### References

  - https://github.com/nvpro-samples/vk_gaussian_splatting
  - https://github.com/jaesung-cs/vkgs
  - https://github.com/shg8/3DGS.cpp
  - https://github.com/dendenxu/fast-gaussian-rasterization

3DGS is the most closely related to what I'm trying to do, but it has some inefficiencies with the use of per-frame fences that read back the entry count.

### Steps

0. A sample that runs a compute shader to fill the contents of a surface and then displays it with the graphics pipeline.  You could base this on the Quad sample.  This will combine compute and graphics queues.

OK, done.  Now combine this with splats:

1. A sample to implement indirect dispatch, see T007

### 4DGS

- 4D Gaussian Splatting for Real-Time Dynamic Scene Rendering (Wu et al., CVPR 2024), canonical splats plus deformation field: https://arxiv.org/abs/2310.08528
- Real-time Photorealistic Dynamic Scene Representation and Rendering with 4D Gaussian Splatting (Yang et al., ICLR 2024), native 4D primitives: https://arxiv.org/abs/2310.10642
- Spacetime Gaussian Feature Splatting (Li et al., CVPR 2024), temporal opacity and parametric motion: https://arxiv.org/abs/2312.16812
- Representing Long Volumetric Video with Temporal Gaussian Hierarchy (Xu et al., SIGGRAPH Asia 2024), near-constant GPU memory regardless of video length.  The closest match to this story: https://arxiv.org/abs/2412.09608
- V^3: Viewing Volumetric Videos on Mobiles via Streamable 2D Dynamic Gaussians (Wang et al., SIGGRAPH Asia 2024), splat attributes as 2D video for hardware codecs: https://arxiv.org/abs/2409.13648 (code: https://github.com/AuthorityWang/VideoGS)
- 3DGStream (Sun et al., CVPR 2024), per-frame streaming via a Neural Transformation Cache: https://arxiv.org/abs/2403.01444
- 4DGC: Rate-Aware 4D Gaussian Compression for Efficient Streamable Free-Viewpoint Video (Hu et al., CVPR 2025): https://arxiv.org/abs/2503.18421
- 4DGCPro: hierarchical 4D Gaussian compression for progressive volumetric video streaming: https://arxiv.org/abs/2509.17513
- Compact 3D Scene Representation via Self-Organizing Gaussian Grids (Morgenstern et al., ECCV 2024), sorting splat attributes into 2D grids for image codecs: https://arxiv.org/abs/2312.13299
- MPEG Gaussian Splat Coding (GSC), standardisation work including dynamic splats: https://mpeg.expert/gsc/index.html
- Vulkan Video decode (VK_KHR_video_decode_queue): https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_video_decode_queue.html


### Related

t001
t020


### Claude

__Me:__

Compare this against sample projects on github and tell me if there are any similar solutions.  I think https://github.com/nvpro-samples/vk_gaussian_splatting doesn't partition the splats into buckets?

__Claude:__ (2026-11-20)

Right, nvpro doesn't bin. The design above is the original 3DGS tile-based rasterizer, and the closest Vulkan example is 3DGS.cpp. The other three repos use one global depth sort and draw quads with the hardware rasterizer.

| | Tile binning | Sort | Drawing | Blend order |
|---|---|---|---|---|
| **nvpro vk_gaussian_splatting** | None | Global 32-bit depth key; GPU radix sort (vrdx) or async CPU sort | Mesh shader (vertex shader optional), one quad per splat | Back-to-front, fixed-function |
| **vkgs** | None | Global 32-bit depth key; vrdx radix sort | Indirect draw of instanced quads | Back-to-front, fixed-function |
| **fast-gaussian-rasterization** | None | Global depth sort (PyTorch on the GPU) | OpenGL geometry shader, points to quads | Back-to-front, fixed-function |
| **3DGS.cpp** | 16x16 tiles | **One global** radix sort on a 64-bit `tile << 32 \| depth` key | Compute, one workgroup per tile | **Front-to-back**, stops once a pixel is opaque |

nvpro also has ray-traced (3DGRT), hybrid splat/mesh, fisheye/distortion (3DGUT) and stochastic no-sort modes; none of them bin. Its key-generation shader (`dist.comp.slang`) also does frustum and size culling and writes an indirect draw count.

**How the three jobs compare to 3DGS.cpp and the original**
1. **Job 1 is three passes.** A splat covers a variable number of tiles, so: count tiles per splat (with projection, 2D covariance and SH colour), prefix sum, then write one `(tile, depth)` entry per covered tile.
2. **Job 2 is one global sort, not one per tile.** Sorting on `tile << 32 | depth` leaves each tile's splats contiguous and depth-ordered; a small pass then finds each tile's start and end. Per-tile sorts would be badly unbalanced. Only the needed bits need sorting: 1080p at 16 px tiles is ~8k tiles, so a 13-bit tile index gives a 45-bit key and 6 radix passes instead of 8.
3. **Sort front-to-back if job 3 is compute.** That lets each pixel stop once it's nearly opaque, which is much of the win. Back-to-front only matters for fixed-function blending.
4. **Don't copy from 3DGS.cpp:** its per-frame fence wait to read back the entry count (use worst-case buffers and indirect dispatch), and its per-tile shader reading splats from global memory (the original CUDA batches them into shared memory).

**Compute or graphics for job 3**
- **Compute:** the original design. Graphics is then used to present the result and draw UI.
- **Graphics:** binning gains almost nothing, because the hardware already blends each pixel in draw order, so a global depth sort is enough (the nvpro/vkgs approach). Keeping tiles would mean drawing each splat once per covered tile, clipped to it. No project found combines tile binning with hardware rasterization.

For shaderlab_compute, the fit is compute for every job up to and including the per-tile render, and graphics for presenting and UI.

Sources:
- https://github.com/nvpro-samples/vk_gaussian_splatting and https://nvpro-samples.github.io/vk_gaussian_splatting/deep-dives/rasterization_of_3d_gaussian_splatting/
- https://github.com/jaesung-cs/vkgs
- https://github.com/shg8/3DGS.cpp
- https://github.com/dendenxu/fast-gaussian-rasterization
- 3D Gaussian Splatting (Kerbl et al.), global sort and tile ranges: https://arxiv.org/abs/2308.04079
- VkSplat, Vulkan compute with tile binning: https://arxiv.org/abs/2605.00219
- Sort-free Gaussian Splatting via Weighted Sum Rendering: https://arxiv.org/abs/2410.18931

__Me:__

The global sort can use the VkRadixSort code that I've imported into the project.  But another question; to handle very large data sets is it advisable to spatially partition the data?  With a grid or other structure.  Sort the regions by distance from view and then sort within each region.

__Claude:__ (2026-11-21)

Partition, yes, but for culling, LOD and streaming. The two-level sort barely helps a radix sort and adds ordering errors at region boundaries.

**Why the two-level sort doesn't help much**
- **Radix sort cost is linear in the entry count.** k regions of n/k entries cost the same as one sort of n. Sorting regions then their contents is a sort on a `(regionRank, depth)` key, which is what an MSD radix sort's first pass does anyway.
- **It's only faster if each region fits one workgroup's shared-memory sort** (one global read and write instead of 4-8 passes). A grid can't guarantee that because splat density varies hugely; an octree split to a maximum splat count per leaf can.
- **Boundary splats get misordered.** Regions must be disjoint and visited in visibility order: an octree gives an exact order (nearest octant first), sorting grid cell centres by distance is close but not exact. A splat whose 3σ extent crosses a boundary is ordered by region, not depth, which can show as seams that pop when the region order flips. The global centre-depth sort is also approximate, but its errors aren't tied to fixed world-space boundaries.
- **The tile pipeline's key is `(tile, depth)` in screen space,** so world-space regions don't plug into the sort directly.

**Where partitioning pays off**
1. **Coarse culling.** Offline, group splats into fixed-size clusters (~256-1024, Morton-ordered so each is compact) with a bounding box each. A first compute pass culls clusters against the frustum; projection runs only on splats in visible clusters, so most of a large scene is never read.
2. **LOD**, the main technique for very large scenes. Distant regions draw fewer, merged splats, which cuts the visible count and the `(tile, depth)` entry count, and so the sort. Hierarchical 3DGS, LODGE and A LoD of Gaussians all build on spatial chunks plus a hierarchy.
3. **Streaming.** Only chunks near the camera need to be in VRAM.
4. **Bounded memory in the tile pipeline.** Entries are several times the visible splat count; rendering regions in batches caps the buffer. Batches rendered front-to-back with a transmittance buffer kept between them could also skip already-opaque tiles (an idea, not seen implemented).

Recommendation: start with a flat cluster list, cluster culling and one global sort of the survivors. An LOD hierarchy is built from the same clusters later.

**Changes needed to the imported VkRadixSort**
- **Keys only, 32-bit, no value buffer**, so nowhere to carry the splat index. 3DGS.cpp adapted the same code to 64-bit keys plus values; its `sort/` shaders are a reference.
- **Element count is a push constant** (`samples/src/s12_compute_sort.cpp:274`), so the CPU must know it before recording; this is what causes 3DGS.cpp's per-frame fence wait. Have the cull pass write the count to a buffer and use indirect dispatch.
- **Work per pass grows with the square of the workgroup count:** every workgroup loops over every workgroup's histogram (`samples/assets/shaders/VkRadixSort/multi_radixsort.comp:56`). At 8,192 elements per workgroup (`BLOCKS_PER_WORKGROUP = 32` x 256 threads), 50M entries (10M splats x ~5 tiles) is ~6,100 workgroups and ~9.5 billion histogram reads per pass. Raise `BLOCKS_PER_WORKGROUP` with the entry count to keep workgroups in the hundreds.
- **The tile key fits in 32 bits.** 1080p at 16 px tiles is ~8,160 tiles (13 bits), leaving 19 bits for depth scaled to the visible near/far range; that gives 4 passes instead of 8.

Sources:
- A Hierarchical 3D Gaussian Representation for Real-Time Rendering of Very Large Datasets (Kerbl et al. 2024): https://arxiv.org/abs/2406.12080
- LODGE: Level-of-Detail Large-Scale Gaussian Splatting with Efficient Rendering (NeurIPS 2025): https://arxiv.org/abs/2505.23158v2
- A LoD of Gaussians: Out-of-Core Training and Rendering for Ultra-Large Scenes: https://arxiv.org/html/2507.01110
