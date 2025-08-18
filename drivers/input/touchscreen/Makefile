# Makefile for the Himax touchscreen drivers.

ifneq ($(filter y, $(CONFIG_KALLSYMS_ALL)),)
	ccflags-y += -D__KERNEL_KALLSYMS_ALL_ENABLED__
endif

ifneq ($(filter y, $(CONFIG_FB)),)
	ccflags-y += -DHX_CONFIG_FB
endif

ifneq ($(filter y, $(CONFIG_DRM)),)
	ccflags-y += -DHX_CONFIG_DRM
endif

ifneq ($(filter y, $(CONFIG_PM)),)
	ccflags-y += -DHX_CONFIG_PM
endif

# obj-y += himax_modular_table.o

# Object definitions moved to main CONFIG_TOUCHSCREEN_HIMAX_CHIPSET block below

# All IC-specific configurations and module build logic removed
# Only built-in kernel configuration is used below
# Simple test: direct object inclusion without composite
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET) += himax_platform.o himax_common.o himax_ic_core.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_DEBUG) += himax_debug.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INSPECT) += himax_inspection.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102E) += himax_ic_HX83102e.o

