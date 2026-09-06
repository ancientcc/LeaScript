SUB_PATH := external/3rdparty/zstd

LOCAL_SRC_FILES += \
	$(SUB_PATH)/lib/common/debug.c \
	$(SUB_PATH)/lib/common/entropy_common.c \
	$(SUB_PATH)/lib/common/error_private.c \
	$(SUB_PATH)/lib/common/fse_decompress.c \
	$(SUB_PATH)/lib/common/pool.c \
	$(SUB_PATH)/lib/common/threading.c \
	$(SUB_PATH)/lib/common/xxhash.c \
	$(SUB_PATH)/lib/common/zstd_common.c \
	$(SUB_PATH)/lib/compress/fse_compress.c \
	$(SUB_PATH)/lib/compress/hist.c \
	$(SUB_PATH)/lib/compress/huf_compress.c \
	$(SUB_PATH)/lib/compress/zstdmt_compress.c \
	$(SUB_PATH)/lib/compress/zstd_compress.c \
	$(SUB_PATH)/lib/compress/zstd_compress_literals.c \
	$(SUB_PATH)/lib/compress/zstd_compress_sequences.c \
	$(SUB_PATH)/lib/compress/zstd_compress_superblock.c \
	$(SUB_PATH)/lib/compress/zstd_double_fast.c \
	$(SUB_PATH)/lib/compress/zstd_fast.c \
	$(SUB_PATH)/lib/compress/zstd_lazy.c \
	$(SUB_PATH)/lib/compress/zstd_ldm.c \
	$(SUB_PATH)/lib/compress/zstd_opt.c \
	$(SUB_PATH)/lib/compress/zstd_preSplit.c \
	$(SUB_PATH)/lib/decompress/huf_decompress.c \
	$(SUB_PATH)/lib/decompress/zstd_ddict.c \
	$(SUB_PATH)/lib/decompress/zstd_decompress.c \
	$(SUB_PATH)/lib/decompress/zstd_decompress_block.c \
	$(SUB_PATH)/lib/dictBuilder/cover.c \
	$(SUB_PATH)/lib/dictBuilder/divsufsort.c \
	$(SUB_PATH)/lib/dictBuilder/fastcover.c \
	$(SUB_PATH)/lib/dictBuilder/zdict.c