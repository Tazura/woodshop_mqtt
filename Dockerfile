# Dockerfile for building and running a C++ application with dependencies
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update
RUN apt-get install -y \
		libssl-dev \
		ca-certificates \
		libboost-filesystem-dev \
		libboost-program-options-dev \
		libmosquittopp-dev \
		build-essential \
		cmake

# Copy source code
COPY src /src

# Build
RUN    mkdir /build \
	&& cd /build \
	&& cmake /src \
	&& cmake --build /build -j \
	&& cmake --build /build -j --target install \
	&& rm -rf /build

CMD ["/usr/local/bin/woodshop_mqtt"]
