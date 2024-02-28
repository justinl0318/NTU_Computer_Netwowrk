FROM ubuntu:22.04

RUN apt-get update && \
    DEBAIN_FRONTEND=noninteractive apt-get install -qy build-essential zlib1g-dev libssl-dev tmux

CMD ["bash"]