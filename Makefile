# The board and everything else is chosen in the configuration, which is made
# with one of the targets below and read from here.
#
#     make menuconfig                 choose from a menu
#     make stm32f429disc_defconfig    start from what a board is usually built as
#     make savedefconfig              write back only what differs from a default
#
include makefiles/kconfig.mk
include makefiles/common.mk
