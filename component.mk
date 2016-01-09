# Component makefile for extras/spiflash
#
# See examples/spiflash for usage

INC_DIRS += $(ROOT)extras/spiflash

# args for passing into compile rule generation
extras/spiflash_INC_DIR =  $(ROOT)extras/spiflash
extras/spiflash_SRC_DIR =  $(ROOT)extras/spiflash

$(eval $(call component_compile_rules,extras/spiflash))
