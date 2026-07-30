# Ubuntu 26.04 with sudo-rs as "sudo" -- the environment issue #208 is about.
#
# 26.04 ships both implementations and gives sudo-rs the higher update-alternatives
# priority (50 vs 40), so a fresh installation uses it. That matters because
# sudo-rs does not preserve the whole environment: it accepts -E, prints
# "preserving the entire environment is not supported, '-E' is ignored" and drops
# it silently. openfortiGUI used to rely on that flag to hand XDG_RUNTIME_DIR to
# the root child process, which is why the GUI never saw its VPN again.
#
# The image contains no openfortiGUI: the package under test is installed at run
# time so every run checks the current build.
FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        sudo sudo-rs ppp iproute2 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Make the choice explicit rather than relying on the priority.
RUN update-alternatives --set sudo /usr/lib/cargo/bin/sudo

# uid 1000 matches the host user, so the mounted lab home and the profiles in it
# stay readable. Group "sudo" is what the shipped sudoers rule grants. The base
# image already occupies uid 1000 with a user named "ubuntu", so rename that one
# instead of adding a second.
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
