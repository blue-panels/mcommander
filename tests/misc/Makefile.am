SUBDIRS = ext.d

# mcommander in pseudo-terminals: a session outlives its terminal and is taken by another. It
# takes minutes, so it is not part of make check; CI runs it for a pull request with the label
# 'sandbox', as the cases of docker/.
if ENABLE_SESSION_PLUGIN
check-session:
	MC_BIN='$(abs_top_builddir)/src/mcommander' \
	SESSION_PLUGIN='$(abs_top_builddir)/src/session/.libs/session.so' \
	MC_SESSION_HELPER='$(abs_top_builddir)/src/session/session_helper' \
	MC_DATADIR='$(abs_top_srcdir)/misc' \
	$(srcdir)/session.py
else
check-session:
	@echo "the session plugin is not built"
endif

.PHONY: check-session

EXTRA_DIST = \
	session.py \
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
