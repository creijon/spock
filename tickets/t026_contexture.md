[t026] Contexture

### Goal

A rethinking of the octree texturing (octex) concept using contrees (64-tree), hence "contexture"

- Far more memory and compute efficient due to the shallower tree
- More suitable to filtering etc, since neighbouring texels will more likely be in the same leaf.

Ensure that there is support for:

- Filtering
- Sparse representation
- Thin surfaces with direction splitting the nodes

### References

#### Octree texturing

- [Octree Textures](https://dl.acm.org/doi/10.1145/566570.566652) - Benson & Davis, SIGGRAPH 2002.  Texture stored in an octree around the surface; no parameterization, adaptive detail, no seams.
- [Painting and Rendering Textures on Unparameterized Models](https://dl.acm.org/doi/10.1145/566570.566649) - DeBry, Gibbs, Petty & Robins, SIGGRAPH 2002.  Sparse adaptive octree for painting and rendering.
- [Octree Textures on the GPU](https://developer.nvidia.com/gpugems/gpugems2/part-v-image-oriented-computing/chapter-37-octree-textures-gpu) - Lefebvre, Hornus & Neyret, GPU Gems 2, 2005.  Tree stored in a texture and traversed in the fragment shader.  Closest template for the Contexture renderer.
- TileTrees - Lefebvre & Dachsbacher, I3D 2007.  2D texture tiles in the octree leaves, so lookups use hardware filtering.
- [An Irradiance Atlas for Global Illumination in Complex Production Scenes](https://graphics.pixar.com/library/IrradianceAtlas/paper.pdf) - Christensen & Batali, EGSR 2004 (brick maps).  Sparse octree with an 8x8x8 brick per node.  Closest in spirit to 4x4x4 contree nodes.

#### Wide-branching sparse trees

- [VDB: High-Resolution Sparse Volumes with Dynamic Topology](https://dl.acm.org/doi/10.1145/2487228.2487235) - Museth, ACM TOG 2013.  Shallow, wide tree with fast random access (OpenVDB).
- [A Micro 64-Tree Structure for Accelerating Ray Tracing on a GPU](https://dl.acm.org/doi/10.5555/2532129.2532158)
- [A guide to fast voxel ray tracing using sparse 64-trees](https://dubiousconst282.github.io/2024/10/03/voxel-ray-tracing/) - 2024 blog post ([code](https://github.com/dubiousconst282/VoxelRT)).  64-bit occupancy masks and traversal.  Also relevant to the raytracing story.

#### Painting into sparse trees

- [Dynamic Deep Octree for High-resolution Volumetric Painting in Virtual Reality](https://onlinelibrary.wiley.com/doi/abs/10.1111/cgf.13558) - Kim et al., Pacific Graphics 2018.  CPU edits the octree, GPU renders it.
- [CanvoX: High-resolution VR Painting in Large Volumetric Canvas](https://arxiv.org/abs/1704.02724) - earlier work from the same line.

#### UV-free alternatives (comparison baselines)

- [Ptex: Per-Face Texture Mapping for Production Rendering](https://onlinelibrary.wiley.com/doi/abs/10.1111/j.1467-8659.2008.01253.x) - Burley & Lacewell, EGSR 2008.  Per-face textures plus adjacency; the production standard.
- [Mesh Colors](https://www.cemyuksel.com/research/meshcolors/) - Yuksel, Keyser & House, ACM TOG 2010.  Colours on vertices, edges and faces; the follow-up "Mesh Color Textures" adds hardware filtering.
- [Beyond UV Mapping: Mesh Texture Compression via Surface-Aligned Texture Fields](https://arxiv.org/html/2609.23606) - recent arXiv paper, not yet read.

#### UV-based baseline

- [Iso-Charts: Stretch-Driven Mesh Parameterization using Spectral Analysis](https://www.microsoft.com/en-us/research/publication/iso-charts-stretch-driven-mesh-parameterization-using-spectral-analysis-2/) - Zhou, Snyder, Guo & Shum, SGP 2004 (Microsoft Research).
- [UVAtlas](https://github.com/microsoft/uvatlas) - Microsoft's library implementing Iso-Charts (archived).
