FROM  ghcr.io/nixmark/neomutt-base:latest

ENV NEOMUTT_TEST_DIR="/neomutt/neomutt-test-files"

WORKDIR neomutt
COPY . ./

RUN git clone --depth 1 https://github.com/neomutt/neomutt-test-files.git
RUN ./configure
RUN make
RUN make test 