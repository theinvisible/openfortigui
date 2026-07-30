# One image per distribution the package is supposed to work on.
#
# What varies between them is exactly what the test is about: which sudo is in
# charge, and which Qt the package is built against. Ubuntu 26.04 ships sudo-rs
# with the higher update-alternatives priority, so a fresh installation uses it;
# everywhere else the classic sudo is in charge. sudo-rs does not preserve the
# environment: it accepts -E, prints "preserving the entire environment is not
# supported, '-E' is ignored" and drops it. openfortiGUI used to rely on that flag
# to hand XDG_RUNTIME_DIR to the root child process, which is why the GUI never
# saw its VPN again (issue #208).
#
# The image contains no openfortiGUI. The package is built and installed at run
# time so every run tests the current source.
#
# Build arguments:
#   BASE_IMAGE       ubuntu:24.04, ubuntu:26.04, debian:bookworm, debian:trixie …
#   USE_SUDO_RS      yes -> install sudo-rs and make it "sudo"
#   WITH_BUILD_DEPS  yes -> put the package's Build-Depends into the image so a
#                           run only has to compile

ARG BASE_IMAGE=ubuntu:26.04
FROM ${BASE_IMAGE}

ARG USE_SUDO_RS=no
ARG WITH_BUILD_DEPS=yes

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        sudo ppp iproute2 ca-certificates dpkg-dev \
    && if [ "$USE_SUDO_RS" = yes ]; then \
           apt-get install -y --no-install-recommends sudo-rs; \
           update-alternatives --set sudo /usr/lib/cargo/bin/sudo || true; \
       fi \
    && rm -rf /var/lib/apt/lists/*

# The build dependencies come from the debian/control files, not from a second
# list kept in here. Only those are copied in, so a source change does not
# invalidate this layer. The KRunner plugin needs KF6 and is skipped where the
# distribution has none -- WITH_RUNNER decides.
ARG WITH_RUNNER=no
COPY control /tmp/deps-app/debian/control
COPY control-runner /tmp/deps-runner/debian/control
RUN if [ "$WITH_BUILD_DEPS" = yes ]; then \
        apt-get update \
        && apt-get build-dep -y --no-install-recommends /tmp/deps-app \
        && if [ "$WITH_RUNNER" = yes ]; then \
               apt-get build-dep -y --no-install-recommends /tmp/deps-runner; \
           fi \
        && rm -rf /var/lib/apt/lists/*; \
    fi

# uid 1000 matches the host user, so the mounted lab home and the profiles in it
# stay readable. Group "sudo" is what the shipped sudoers rule grants. The Ubuntu
# base images already occupy uid 1000 with a user named "ubuntu", the Debian ones
# leave it free -- hence the condition.
RUN set -eu; \
    if getent passwd 1000 >/dev/null; then \
        old="$(getent passwd 1000 | cut -d: -f1)"; \
        usermod --login labuser --home /home/labuser --move-home \
                --shell /bin/bash --append --groups sudo "$old"; \
    else \
        useradd --uid 1000 --create-home --shell /bin/bash --groups sudo labuser; \
    fi

# Test scaffolding, not part of the product: two ways to run "env" through sudo,
# one with the SETENV: tag and one without. That contrast is the whole point --
# the classic sudo refuses -E without the tag, sudo-rs ignores -E either way, and
# openfortiGUI's own rule never carried the tag. The shipped rule only permits
# "openfortigui --start-vpn *", which is asserted separately.
RUN printf '#!/bin/sh\nexec /usr/bin/env "$@"\n' >/usr/local/bin/envprobe \
    && chmod 0755 /usr/local/bin/envprobe \
    && printf '%%sudo ALL=NOPASSWD: /usr/bin/env\n%%sudo ALL=NOPASSWD:SETENV: /usr/local/bin/envprobe\n' \
        >/etc/sudoers.d/lab-env-probe \
    && chmod 0440 /etc/sudoers.d/lab-env-probe

COPY in-container.sh /usr/local/bin/in-container.sh
RUN chmod 0755 /usr/local/bin/in-container.sh

ENTRYPOINT ["/usr/local/bin/in-container.sh"]
