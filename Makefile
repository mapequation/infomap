.DEFAULT_GOAL := help

MK_DIR := mk

include $(MK_DIR)/common.mk
include $(MK_DIR)/native.mk
include $(MK_DIR)/python.mk
include $(MK_DIR)/js.mk
include $(MK_DIR)/test.mk
include $(MK_DIR)/ci.mk
include $(MK_DIR)/advanced.mk
include $(MK_DIR)/legacy.mk
