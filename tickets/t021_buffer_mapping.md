[t021] Buffer Mapping

#### Goal

Buffer mapping is inconsistent.

- upload() and copyToDevice() call mapMemory() even when the buffer is already persistently mapped via map(), which is invalid.
- The vector upload() and copyToDevice() don't flush, so they are wrong for non-coherent memory.

#### Related

t004


#### Claude
