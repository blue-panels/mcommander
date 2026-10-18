# debian:bookworm-slim, pinned so that a run is the same run next month
FROM debian@sha256:88200866dfff7ea7f5cbcb6ec7c8a701889efe6fe859fe64d6990e4b07ea4171

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        meson ninja-build pkg-config gettext \
        libglib2.0-dev libncurses-dev libslang2-dev \
        libssh2-1-dev libcurl4-openssl-dev libarchive-dev libmagic-dev libsqlite3-dev libsmbclient-dev \
        check rsync file openssh-client curl sshpass smbclient ca-certificates \
        zsh dash mksh tcsh busybox fish \
        libarchive-tools procps unzip zip tmux \
        valgrind \
        liblua5.4-dev chafa sqlite3 \
        locales \
    && sed -i 's/^# *\(en_US.UTF-8\|ru_RU.UTF-8\|ru_RU.KOI8-R\)/\1/' /etc/locale.gen \
    && locale-gen \
    # CP866 is not in the list locale-gen walks; it is what mc draws with
    # when the terminal speaks the DOS codepage
    && localedef -f IBM866 -i ru_RU ru_RU.CP866 \
    && rm -rf /var/lib/apt/lists/*

# without a UTF-8 locale mc draws question marks instead of anything non-ASCII;
# run-cases.sh sets the locale per run
ENV LANG=ru_RU.UTF-8

COPY common/build-mc.sh common/check-remote.sh common/run-cases.sh common/features.ini /usr/local/bin/
RUN chmod +x /usr/local/bin/build-mc.sh /usr/local/bin/check-remote.sh /usr/local/bin/run-cases.sh

# mc drives its subshell from $SHELL; a fresh zsh would stop at its
# new-user questionnaire, so give it an rc file of its own.
COPY common/zshrc /root/.zshrc
ENV SHELL=/usr/bin/zsh

# The shells the "shells" subject is run under: busybox answers to the name mc
# knows its shell by, and fish greets every new session unless it is told not
# to, which would be on the screen a case reads.
RUN ln -s /bin/busybox /usr/local/bin/ash \
    && mkdir -p /root/.config/fish \
    && printf 'set fish_greeting ""\n' > /root/.config/fish/config.fish

ENV TERM=xterm-256color
WORKDIR /work

CMD ["/bin/bash"]
