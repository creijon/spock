# Run at build time by the sample_assets target. file(COPY) skips files whose timestamps haven't changed.
file(COPY "${SOURCE_DIR}/" DESTINATION "${DEST_DIR}")
