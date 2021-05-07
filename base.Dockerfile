FROM  ubuntu

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
    apt upgrade -y && \ 
    ln -fs /usr/share/zoneinfo/Europe/Berlin /etc/localtime && \
    apt install -y \
     autopoint docbook-simple docbook-xsl gettext gpgsm \
     graphviz jimsh libdb-dev libgdbm-dev libgnutls28-dev \
     libgpgme11-dev libgss-dev libidn2-0-dev libkyotocabinet-dev \
     liblmdb-dev liblua5.2-dev liblz4-dev libncursesw5-dev \ 
     libnotmuch-dev libqdbm-dev librocksdb-dev libsasl2-dev \
     libslang2-dev libssl-dev libtdb-dev libtokyocabinet-dev \
     libxml2-utils libzstd-dev lua5.2 lynx xsltproc git make && \
    dpkg-reconfigure --frontend noninteractive tzdata && \
    apt autoclean && apt autoremove