SUB_PATH := external/3rdparty/ruy

LOCAL_SRC_FILES += \
	$(SUB_PATH)/ruy/profiler/instrumentation.cc \
	$(SUB_PATH)/ruy/allocator.cc \
	$(SUB_PATH)/ruy/apply_multiplier.cc \
	$(SUB_PATH)/ruy/block_map.cc \
	$(SUB_PATH)/ruy/blocking_counter.cc \
	$(SUB_PATH)/ruy/context.cc \
	$(SUB_PATH)/ruy/context_get_ctx.cc \
	$(SUB_PATH)/ruy/cpuinfo.cc \
	$(SUB_PATH)/ruy/ctx.cc \
	$(SUB_PATH)/ruy/denormal.cc \
	$(SUB_PATH)/ruy/frontend.cc \
	$(SUB_PATH)/ruy/have_built_path_for_avx.cc \
	$(SUB_PATH)/ruy/have_built_path_for_avx2_fma.cc \
	$(SUB_PATH)/ruy/have_built_path_for_avx512.cc \
	$(SUB_PATH)/ruy/kernel_arm32.cc \
	$(SUB_PATH)/ruy/kernel_arm64.cc \
	$(SUB_PATH)/ruy/kernel_avx.cc \
	$(SUB_PATH)/ruy/kernel_avx2_fma.cc \
	$(SUB_PATH)/ruy/kernel_avx512.cc \
	$(SUB_PATH)/ruy/pack_arm.cc \
	$(SUB_PATH)/ruy/pack_avx.cc \
	$(SUB_PATH)/ruy/pack_avx2_fma.cc \
	$(SUB_PATH)/ruy/pack_avx512.cc \
	$(SUB_PATH)/ruy/prepacked_cache.cc \
	$(SUB_PATH)/ruy/prepare_packed_matrices.cc \
	$(SUB_PATH)/ruy/system_aligned_alloc.cc \
	$(SUB_PATH)/ruy/thread_pool.cc \
	$(SUB_PATH)/ruy/trmul.cc \
	$(SUB_PATH)/ruy/tune.cc \
	$(SUB_PATH)/ruy/wait.cc