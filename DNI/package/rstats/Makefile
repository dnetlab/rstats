
include $(TOPDIR)/rules.mk

PKG_NAME:=rstats
PKG_VERSION:=1.0

PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/rstats
	SECTION  := net
 	CATEGORY := Network
	TITLE    := rstats for traffic meter 
	DEPENDS  := +libadapter
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/rstats/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(CP) ./files/rstats.init $(1)/etc/init.d/rstats
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/rstats $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,rstats))
