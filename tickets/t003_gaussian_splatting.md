[t003] Gaussian Splatting

#### Goal

##### GPU compute-based renderer

- Make a little sample that renders tiles of different colours into a surface and then displays it.  You could base this on the Quad sample.  This will combine compute and graphics queues.
- Then combine this with the splat data and bin the splats into the tiles.
- Read through the samples in:
  - https://github.com/nvpro-samples/vk_gaussian_splatting
  - https://github.com/jaesung-cs/vkgs
  - https://github.com/shg8/3DGS.cpp
  - https://github.com/dendenxu/fast-gaussian-rasterization

Overview of GS Algo:

- First GPU job: for all splats, project bounds onto screen and bucket into tiles.
- Second GPU job: for all tiles, sort splats back-to-front
- Third GPU jon: for all splats in tiles: render

#### Related

t001
t020


#### Claude
