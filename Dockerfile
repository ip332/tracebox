FROM ubuntu:24.04@sha256:786a8b558f7be160c6c8c4a54f9a57274f3b4fb1491cf65146521ae77ff1dc54

ARG DEV_UID=1000
ARG DEV_GID=1000

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        bash \
        build-essential \
        ca-certificates \
        clang \
        cmake \
        gdb \
        gcovr \
        libgflags-dev \
        libgtest-dev \
        libprotobuf-dev \
        ninja-build \
        pkg-config \
        protobuf-compiler \
        git \
    && rm -rf /var/lib/apt/lists/* \
    && if getent passwd "${DEV_UID}" >/dev/null; then \
         existing_user="$(getent passwd "${DEV_UID}" | cut -d: -f1)"; \
         usermod --login developer --home /home/developer --move-home "${existing_user}"; \
       else \
         if ! getent group "${DEV_GID}" >/dev/null; then groupadd --gid "${DEV_GID}" developer; fi; \
         useradd --uid "${DEV_UID}" --gid "${DEV_GID}" --create-home --shell /bin/bash developer; \
       fi

WORKDIR /workspace

USER developer

CMD ["bash"]
