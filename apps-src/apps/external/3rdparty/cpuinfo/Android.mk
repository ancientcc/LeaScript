SUB_PATH := external/3rdparty/cpuinfo

LOCAL_SRC_FILES += \
	$(SUB_PATH)/src/api.c \
	$(SUB_PATH)/src/cache.c \
	$(SUB_PATH)/src/init.c \
	$(SUB_PATH)/src/log.c \
	$(SUB_PATH)/src/linux/processors.c \
	$(SUB_PATH)/src/linux/smallfile.c \
	$(SUB_PATH)/src/linux/multiline.c \
	$(SUB_PATH)/src/linux/cpulist.c \
	$(SUB_PATH)/src/arm/uarch.c \
	$(SUB_PATH)/src/arm/cache.c \
	$(SUB_PATH)/src/arm/linux/init.c \
	$(SUB_PATH)/src/arm/linux/cpuinfo.c \
	$(SUB_PATH)/src/arm/linux/clusters.c \
	$(SUB_PATH)/src/arm/linux/chipset.c \
	$(SUB_PATH)/src/arm/linux/midr.c \
	$(SUB_PATH)/src/arm/linux/hwcap.c \
	$(SUB_PATH)/src/arm/android/properties.c

ifneq ($(filter $(NDK_KNOWN_DEVICE_ABI64S),$(TARGET_ARCH_ABI)),)
# 64-bit ABIs
LOCAL_SRC_FILES += $(SUB_PATH)/src/arm/linux/aarch64-isa.c
else
# 32-bit ABIs
LOCAL_SRC_FILES += $(SUB_PATH)/src/arm/linux/aarch32-isa.c
endif