OpenWrt-FL2440 开发笔记
===
---------------------------------
同步于openwrt官方嵌入式开发版本分支，用于添加对 fl2440 开发板的 porting 工作。

相关说明请参见官方文档，该仓储仅供参考学习使用。 开发过程可参考 [Wiki](https://github.com/iAlios/openwrt-fl2440/wiki)。

官方源如下：

SVN仓：[svn://svn.openwrt.org.cn/dreambox/branches/openosom](https://dev.openwrt.org.cn/browser/branches/openosom)

GIT仓：[git://github.com/openosom/backfire_10.03.1](https://github.com/openosom/backfire_10.03.1)

下面是主要的修改过程及分析

1. 安装编译环境
---------------------------------
    
    $ sudo apt-get install subversion build-essential libncurses5-dev zlib1g-dev gawk git ccache gettext libssl-dev xsltproc file

2. 简单的编译过程分析
---------------------------------

    1) 安装 device, packages, managements, xwrt, luci 等相关组建

        a) 同步所有组建，即 feeds update 的过程

                $ ./scripts/feeds update -a

            在这个过程中，feeds 通过 feeds.conf.default 文件中配置的各个组件的获取方式及获取源来同步相应的内容，同步方式主要有 src-svn, src-cpy, src-link, src-git, src-gitsvn, src-bzr, src-hg 共 7 中方式(详细参见 scritps/feeds 文件)。将其下载到 feeds 目录下进行存放，同时会产生一些临时文件，比如 devices 会有 devices.tmp, devices.index。

        b) 安装所有组建，即 feeds install 的过程

                $ ./scripts/feeds install -a

            该过程实际上是将 feeds update 同步下的组建 copy 到 package/feeds 目录下进行存放。在安装这些文件之前，创建了编译时候需要的一些必要的文件夹，具体如下：

                mkdir -p ./staging_dir/toolchain-arm_v4t_gcc-4.3.3+cs_eglibc-2.8_eabi
                cd ./staging_dir/toolchain-arm_v4t_gcc-4.3.3+cs_eglibc-2.8_eabi
                mkdir -p stamp lib usr/include usr/lib

            这样为之后编译交叉编译工具和文件系统做准备。
            
    2) 修改编译菜单
        
        make menuconfig

    3) 执行编译

        make V=99   # 打印所有编译过程
        或者
        make V=s  # 不打印任何编译过程消息( s 是 silent 的意思)

        对 V 这个参数的定义，其实是在 openwrt 编译系统中的一个必要参数，其使用是在 include/verbose.mk 文件中进行处理的，如下：

            ifeq ("$(origin V)", "command line")
                KBUILD_VERBOSE:=$(V)
            endif
        
        通过上述方式将 V 参数传递给 KBUILD_VERBOSE 变量，然后在下文中进行判断，如果是 99 就会定义如下内容：
  
            SUBMAKE=$(MAKE) -w
            define MESSAGE
                printf "%s\n" "$(1)"
            endef
            
     4) 编译并将其日志存入到文件中

        make V=99 2>&1 |tee build.log |grep -i error

2. 添加 Boot Loader 
---------------------------------
    
    在 openwrt 中主要是通过 feeds/device 来管理新设备的相关的内容，其中包括了 qemu 虚拟机相关的内容，(其最开始是将 uboot 相关东西也是放在 menuconfig 菜单里面的 Devices 子菜单中，方便个人查看已将相应内容移动到 Boot Loaders 菜单中) 

    为了方便起见，最好先自己创建一个 git server 用来做为 device feeds 获取的源，然后通过修改 feed.conf 文件来将 ./scripts

    1) 在 feeds/device 目录下创建 uboot-fl2440 目录, 并添加相应的 Makefile 文件。
    2) 修改相应的 Makefile 文件从而实现自定义的 boot loader 的装载过程，以及编译配置，具体如下：
        
        a) 源码配置

            # 包名称  
            PKG_NAME:=uboot-fl2440
            # 版本信息
            PKG_VERSION:=2010.09
            PKG_RELEASE:=1

            # 下载压缩文件
            PKG_SOURCE:=u-boot-2010.03-fl2440.tar.bz2
            # 解压缩文件
            PKG_SOURCE_SUBDIR:=u-boot-2010.09-fl2440
            # 编译目录
            PKG_BUILD_DIR=$(KERNEL_BUILD_DIR)/$(PKG_SOURCE_SUBDIR)
            # 代码同步路径
            PKG_SOURCE_URL:=git://github.com/iAlios/fl2440-uboot-2010.09.git
            # 代码同步方式
            PKG_SOURCE_PROTO:=git
            # 代码版本(指定的提交的节点 commit ID)
            PKG_SOURCE_VERSION:=7877290e987a95dc834e686a059ab5ceb04fb714

        b) menuconfig 配置
                    
            define Package/$(PKG_NAME)
              # 设置菜单名称
              TITLE:=for fl2440(iAlios)
              # 设置根菜单中名称
              CATEGORY:=Boot Loaders
              # 设置分区名称
              SECTION:=uboot
              # 设置编译依赖
              DEPENDS:=@TARGET_s3c24xx 
              URL:=http://www.denx.de/wiki/U-Boot
            endef

        c) 编译配置

            define Build/Prepare
                # 解压文件
                $(PKG_UNPACK)
                # 安装 patch
                $(Build/Patch)
                # 删除不必要的中间文件
                $(FIND) $(PKG_BUILD_DIR) -name .svn | $(XARGS) rm -rf
            endef

            define Build/Compile
                # 编译配置
                $(MAKE) -C $(PKG_BUILD_DIR) fl2440_config
                # 编译
                $(MAKE) -C $(PKG_BUILD_DIR) CROSS_COMPILE=$(TARGET_CROSS)
            endef

        d) 安装配置

            define Package/$(PKG_NAME)/install
                $(INSTALL_DIR) $(BIN_DIR)
                dd if=$(PKG_BUILD_DIR)/u-boot.bin of=$(BIN_DIR)/$(PKG_NAME).bin bs=128k conv=sync
            endef

3. 添加新 CPU 架构
---------------------------------

   所有与 kernel 相关的内容都是存放在 target/linux 目录下。

    1) 在 openwrt 编译系统中是通过 Config.in 文件来进行配置 menuconfig 菜单的。而在 openwrt 体系中是通过在 Makefile 中定义如下内容来进行创建菜单的：
       
    target/linux/s3c24xx/Makefile 文件中添加子目标目录：
	   
        SUBTARGETS:=dev-s3c2440 dev-s3c2410 dev-fs2410 dev-mini2440 dev-fl2440 dev-gec2410 dev-gec2440 dev-qq2440
   
    2) 在 target/linux/s3c24xx 文件夹下面建立 dev-fl2440 目录，并添加相关的修改文件，其中最主要的文件是 target.mk ，它是用来声明当前开发板用的，如下所示：

	BOARDNAME:=FL2440 Development Board

	define Target/Description
		FL2440 Development Board
        endef
       
    3) 添加开发板自定义 config 文件，个性化配置设备 kernel
    
    在根目录下执行如下指令：
    
	$ make kernel_menuconfig

    这样在对应的设备文件中创建自己的 config 配置
    
    	$ git status
    	......
    	 modified:   target/linux/s3c24xx/dev-fl2440/config-2.6.32
    	......
    	
    4) 添加驱动程序(未完)
    
---------------------------------
##### Copyright 2015 (C) i.lufei([m.lufei@qq.com](mail.qq.com)) #####

如需要使用官方相关内容，请通过上面官方链接进行获取，谢谢合作。

##### 2015-09-05 #####
