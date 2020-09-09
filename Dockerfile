# To Build: docker build --no-cache -t pbui/cse-30341-fa20-project02 . < Dockerfile

FROM	    ubuntu:20.04
MAINTAINER  Peter Bui <pbui@nd.edu>

ENV         DEBIAN_FRONTEND=noninteractive

RUN	    apt update -y

# Run-time dependencies
RUN	    apt install -y build-essential python3 python3-tornado gawk valgrind iproute2
