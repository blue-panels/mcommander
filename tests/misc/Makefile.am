SUBDIRS = ext.d

# mcommander run in pseudo-terminals: a lost terminal is taken over by a new one
TESTS = resurrect.py
AM_TESTS_ENVIRONMENT = \
	MC_BIN='$(abs_top_builddir)/src/mcommander'; export MC_BIN; \
	MC_DATADIR='$(abs_top_srcdir)/misc'; export MC_DATADIR;

EXTRA_DIST = \
	resurrect.py \
	docker/glib-api/Dockerfile \
	docker/README.md \
	docker/sandbox.sh \
	docker/reports/.gitignore \
	docker/cases/archives/fixtures.sh \
	docker/common/build-mc.sh \
	docker/common/check-remote.sh \
	docker/common/features.ini \
	docker/common/run-cases.sh \
	docker/common/toggles.ini \
	docker/common/ui.py \
	docker/common/ui.sh \
	docker/common/zshrc \
	docker/common/keymaps/shift-tab-complete.keymap \
	docker/common/remote/Dockerfile.remote \
	docker/common/remote/Dockerfile.remote-alpine \
	docker/common/remote/entrypoint-remote.sh \
	docker/common/remote/smb.conf \
	docker/common/remote/vsftpd.conf \
	docker/envs/debian-12/Dockerfile.mc \
	docker/envs/debian-12/docker-compose.yml \
	docker/envs/debian-12/README.md \
	docker/envs/debian-12/expect.tsv \
	docker/envs/debian-12-minimal/Dockerfile.mc \
	docker/envs/debian-12-minimal/docker-compose.yml \
	docker/envs/debian-12-minimal/README.md \
	docker/envs/debian-12-minimal/expect.tsv \
	docker/envs/alpine-remote/docker-compose.yml \
	docker/envs/alpine-remote/README.md \
	docker/envs/alpine-remote/expect.tsv \
	docker/envs/debian-10/Dockerfile.mc \
	docker/envs/debian-10/docker-compose.yml \
	docker/envs/debian-10/README.md \
	docker/envs/debian-10/expect.tsv
