FROM ubuntu:18.04

RUN apt-get update && apt-get install -y --no-install-recommends \
		build-essential \
		make \
		pkg-config \
		curl \
		cmake \
	&& rm -rf /var/lib/apt/lists/*

VOLUME /code
VOLUME /artifacts
WORKDIR /artifacts

CMD ["/bin/bash"]
