# The helper scripts, named once. The list is read by the meson.build of the
# shell link plugin, which installs them and compiles them into shelldef.h.

SHELL_LINK_HELPERS = \
	ls mkdir fexists unlink chown chmod rmdir ln mv hardlink \
	get send append info utime putat cksumrange blockdigests
