BOARDNAME:=FL2440 Development Board
FEATURES:=squashfs jffs2 yaffs2

define Target/Description
	FL2440 Development Board
endef

define Image/Build/yaffs2
       $(STAGING_DIR_HOST)/bin/mkyaffs2image $(STAGING_DIR_ROOT)/ $(BIN_DIR)/openwrt-$(BOARD)-$(1).yaffs2
endef
