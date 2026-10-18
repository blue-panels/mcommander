# The oldest GLib the project accepts: meson.build asks for glib-2.0 >= 2.58,
# and Debian 10 ships 2.58.3.  Everything else is old too: libarchive 3.3,
# curl 7.64 (the s3 plugin stays out), libssh2 1.8, gcc 8, gettext 0.19.
# Buster is archived, so apt goes to archive.debian.org.
# debian:buster-slim, pinned so that a run is the same run next month
FROM debian@sha256:bb3dc79fddbca7e8903248ab916bb775c96ec61014b3d02b4f06043b604726dc

RUN printf 'deb http://archive.debian.org/debian buster main\ndeb http://archive.debian.org/debian-security buster/updates main\n' \
        > /etc/apt/sources.list \
    && apt-get -o Acquire::Check-Valid-Until=false update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        pkg-config gettext python3-pip \
        libglib2.0-dev libncursesw5-dev libslang2-dev \
        libssh2-1-dev libcurl4-openssl-dev libarchive-dev libmagic-dev libsqlite3-dev libsmbclient-dev \
        check rsync file openssh-client curl sshpass smbclient ca-certificates \
        zsh libarchive-tools procps unzip zip tmux \
        locales \
    && sed -i 's/^# *\(en_US.UTF-8\|ru_RU.UTF-8\|ru_RU.KOI8-R\)/\1/' /etc/locale.gen \
    && locale-gen \
    # CP866 is not in the list locale-gen walks; it is what mc draws with
    # when the terminal speaks the DOS codepage
    && localedef -f IBM866 -i ru_RU ru_RU.CP866 \
    && rm -rf /var/lib/apt/lists/*

# The meson of Debian 10 is older than the build needs; the Python there runs a newer one.
RUN pip3 install --no-cache-dir 'meson==1.3.2' 'ninja==1.11.1.1'

ENV LANG=ru_RU.UTF-8

COPY common/build-mc.sh common/check-remote.sh common/run-cases.sh common/features.ini /usr/local/bin/
RUN chmod +x /usr/local/bin/build-mc.sh /usr/local/bin/check-remote.sh /usr/local/bin/run-cases.sh

COPY common/zshrc /root/.zshrc
ENV SHELL=/usr/bin/zsh

ENV TERM=xterm-256color
WORKDIR /work

CMD ["/bin/bash"]
