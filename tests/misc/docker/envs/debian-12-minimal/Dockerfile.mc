# The same Debian 12 as debian-12, without the programs mc.ext.ini and the
# extfs helpers call: file(1), unzip, zip, 7z, bzip2.  bsdtar stays for the
# fixtures; the arcmc plugin uses the libarchive library, not the program.
# debian:bookworm-slim, pinned so that a run is the same run next month
FROM debian@sha256:88200866dfff7ea7f5cbcb6ec7c8a701889efe6fe859fe64d6990e4b07ea4171

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        meson ninja-build pkg-config gettext \
        libglib2.0-dev libncurses-dev libslang2-dev \
        libssh2-1-dev libcurl4-openssl-dev libarchive-dev libmagic-dev libsqlite3-dev libsmbclient-dev \
        check rsync openssh-client curl sshpass smbclient ca-certificates \
        zsh libarchive-tools procps tmux \
        locales \
    && sed -i 's/^# *\(en_US.UTF-8\|ru_RU.UTF-8\|ru_RU.KOI8-R\)/\1/' /etc/locale.gen \
    && locale-gen \
    # CP866 is not in the list locale-gen walks; it is what mc draws with
    # when the terminal speaks the DOS codepage
    && localedef -f IBM866 -i ru_RU ru_RU.CP866 \
    && rm -rf /var/lib/apt/lists/*

# The packages stay (other packages depend on them), the programs go.
RUN rm -f /usr/bin/file /usr/bin/bzip2 /usr/bin/bunzip2 /usr/bin/unzip /usr/bin/zip /usr/bin/7z /usr/bin/7za /usr/bin/7zr

# without a UTF-8 locale mc draws question marks instead of anything non-ASCII;
# run-cases.sh sets the locale per run
ENV LANG=ru_RU.UTF-8

COPY common/build-mc.sh common/check-remote.sh common/run-cases.sh common/features.ini /usr/local/bin/
RUN chmod +x /usr/local/bin/build-mc.sh /usr/local/bin/check-remote.sh /usr/local/bin/run-cases.sh

# mc drives its subshell from $SHELL; a fresh zsh would stop at its
# new-user questionnaire, so give it an rc file of its own.
COPY common/zshrc /root/.zshrc
ENV SHELL=/usr/bin/zsh

ENV TERM=xterm-256color
WORKDIR /work

CMD ["/bin/bash"]
