FROM golang:latest AS gobuild

ENV CODE_DIR /go/src/github.com/Comcast/littlesheens
ENV LD_LIBRARY_PATH ${CODE_DIR}

# COPY . ${CODE_DIR}/
# COPY .git ${CODE_DIR}/.git
WORKDIR ${CODE_DIR}/

# extldflags needs to be static to run in scratch
# This will get set in `make app`
RUN echo "GOROOT=${GOPATH}"

# RUN ls ${CODE_DIR}

RUN go install github.com/bronze1man/yaml2json@latest
RUN go install github.com/tdewolff/minify/cmd/minify@latest