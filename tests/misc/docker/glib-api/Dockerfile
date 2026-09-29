# Debian 10 provides the project's minimum GLib, 2.58.3.
FROM debian@sha256:bb3dc79fddbca7e8903248ab916bb775c96ec61014b3d02b4f06043b604726dc

RUN printf 'deb http://archive.debian.org/debian buster main\ndeb http://archive.debian.org/debian-security buster/updates main\n' > /etc/apt/sources.list \
    && apt-get -o Acquire::Check-Valid-Until=false update \
    && apt-get install -y --no-install-recommends \
        build-essential autoconf automake libtool pkg-config gettext autopoint \
        libglib2.0-dev libncursesw5-dev libslang2-dev libacl1-dev \
        libssh2-1-dev libcurl4-openssl-dev libarchive-dev libmagic-dev \
        libsqlite3-dev libsmbclient-dev check file unzip perl \
    && rm -rf /var/lib/apt/lists/*

# Keep the test framework current while GLib stays at the supported minimum.
ADD https://deb.debian.org/debian/pool/main/c/check/check_0.15.2.orig.tar.gz /tmp/check.tar.gz
RUN echo '8451b68ac5d6f3157b24f22eceff575bcf566264f6d78f3852f89d4e08cf42e1  /tmp/check.tar.gz' | sha256sum -c - \
    && tar -xzf /tmp/check.tar.gz -C /tmp \
    && cd /tmp/check-0.15.2 \
    && ./configure --disable-static \
    && make -j4 \
    && make install \
    && ldconfig \
    && rm -rf /tmp/check-0.15.2 /tmp/check.tar.gz

ENV LANG=C.UTF-8
