# syntax=docker/dockerfile:1.7

ARG UBUNTU_VERSION=24.04 # lts

FROM alpine:3.21 AS shatterspear-assets

ARG ALE_REPOSITORY=https://github.com/shatterspear/mod-eluna.git
ARG INSTANCE_RESET_REPOSITORY=https://github.com/shatterspear/mod-instance-reset.git
ARG TRANSMOG_REPOSITORY=https://github.com/shatterspear/mod-transmog.git
ARG CONFIG_REPOSITORY=git@github.com:shatterspear/ac-configs.git
ARG LUA_REPOSITORY=git@github.com:shatterspear/lua_scripts.git
ARG DBC_REPOSITORY=git@github.com:shatterspear/dbc.git

RUN apk add --no-cache git openssh-client \
    && mkdir -p -m 0700 /root/.ssh \
    && ssh-keyscan github.com > /root/.ssh/known_hosts

RUN git clone --depth 1 "$ALE_REPOSITORY" /assets/mod-ale \
    && git clone --depth 1 "$INSTANCE_RESET_REPOSITORY" /assets/mod-instance-reset \
    && git clone --depth 1 "$TRANSMOG_REPOSITORY" /assets/mod-transmog

RUN --mount=type=secret,id=github_key_ac_configs_b64,required=true \
    --mount=type=secret,id=github_key_lua_scripts_b64,required=true \
    --mount=type=secret,id=github_key_dbc_b64,required=true \
    base64 -d /run/secrets/github_key_ac_configs_b64 > /tmp/github_key_ac_configs \
    && base64 -d /run/secrets/github_key_lua_scripts_b64 > /tmp/github_key_lua_scripts \
    && base64 -d /run/secrets/github_key_dbc_b64 > /tmp/github_key_dbc \
    && chmod 0600 /tmp/github_key_ac_configs /tmp/github_key_lua_scripts /tmp/github_key_dbc \
    && GIT_SSH_COMMAND="ssh -i /tmp/github_key_ac_configs -o IdentitiesOnly=yes" \
        git clone --depth 1 "$CONFIG_REPOSITORY" /assets/configs \
    && GIT_SSH_COMMAND="ssh -i /tmp/github_key_lua_scripts -o IdentitiesOnly=yes" \
        git clone --depth 1 "$LUA_REPOSITORY" /assets/lua_scripts \
    && GIT_SSH_COMMAND="ssh -i /tmp/github_key_dbc -o IdentitiesOnly=yes" \
        git clone --depth 1 "$DBC_REPOSITORY" /assets/dbc \
    && rm -f /tmp/github_key_ac_configs /tmp/github_key_lua_scripts /tmp/github_key_dbc \
    && find /assets/configs -type f -name '*.bac' -delete \
    && find /assets/configs -type f -name '*.conf' -exec sed -i -E \
       '/^[[:space:]]*(LoginDatabaseInfo|WorldDatabaseInfo|CharacterDatabaseInfo|MySQLExecutable|DataDir)[[:space:]]*=/d' '{}' + \
    && rm -f /assets/lua_scripts/debug.log \
    && find /assets -name .git -type d -prune -exec rm -rf '{}' +

# This target lays out the general directory skeleton for AzerothCore,
# This target isn't intended to be directly used
FROM ubuntu:$UBUNTU_VERSION AS skeleton

# Note: ARG instructions defined after FROM are available in this build stage.
# Placing ARG TZ here (after FROM) ensures it is accessible for configuring the timezone below.
ARG TZ=Etc/UTC
ARG DOCKER=1
ARG DEBIAN_FRONTEND=noninteractive

ENV AC_FORCE_CREATE_DB=1

RUN mkdir -pv \
        /azerothcore/bin                   \
        /azerothcore/data                  \
        /azerothcore/deps                  \
        /azerothcore/env/dist/bin          \
        /azerothcore/env/dist/data/Cameras \
        /azerothcore/env/dist/data/dbc     \
        /azerothcore/env/dist/data/maps    \
        /azerothcore/env/dist/data/mmaps   \
        /azerothcore/env/dist/data/vmaps   \
        /azerothcore/env/dist/logs         \
        /azerothcore/env/dist/temp         \
        /azerothcore/env/dist/etc          \
        /azerothcore/modules               \
        /azerothcore/src                   \
        /azerothcore/build

# Configure Timezone
RUN apt-get update                                                       \
    && apt-get install -y --no-install-recommends tzdata ca-certificates \
    && ln -snf "/usr/share/zoneinfo/$TZ" /etc/localtime                  \
    && echo "$TZ" > /etc/timezone                                          \
    && dpkg-reconfigure --frontend noninteractive tzdata                 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /azerothcore

# This target builds the docker image
# This target can be useful to inspect the explicit outputs from the build,
FROM skeleton AS build

ARG CTOOLS_BUILD="all"
ARG CTYPE="RelWithDebInfo"
ARG CCACHE_CPP2="true"
ARG CSCRIPTPCH="OFF"
ARG CSCRIPTS="static"
ARG CMODULES="static"
ARG CSCRIPTS_DEFAULT_LINKAGE="static"
ARG CWITH_WARNINGS="ON"
ARG CMAKE_EXTRA_OPTIONS=""
ARG GIT_DISCOVERY_ACROSS_FILESYSTEM=1

ARG CCACHE_DIR="/ccache"
ARG CCACHE_MAXSIZE="1000MB"
ARG CCACHE_SLOPPINESS="pch_defines,time_macros,include_file_mtime"
ARG CCACHE_COMPRESS=""
ARG CCACHE_COMPRESSLEVEL="9"
ARG CCACHE_COMPILERCHECK="content"
ARG CCACHE_LOGFILE=""

RUN apt-get update                                                     \
    && apt-get install -y --no-install-recommends                         \
        build-essential ccache libtool cmake-data make cmake clang    \
        git lsb-base curl unzip default-mysql-client openssl                  \
        default-libmysqlclient-dev libboost-all-dev libssl-dev libmysql++-dev \
        libreadline-dev zlib1g-dev libbz2-dev libncurses5-dev liblzma-dev  \
    && rm -rf /var/lib/apt/lists/*

COPY CMakeLists.txt /azerothcore/CMakeLists.txt
COPY conf /azerothcore/conf
COPY deps /azerothcore/deps
COPY src /azerothcore/src
COPY modules /azerothcore/modules
COPY --from=shatterspear-assets /assets/mod-ale /azerothcore/modules/mod-ale
COPY --from=shatterspear-assets /assets/mod-instance-reset /azerothcore/modules/mod-instance-reset
COPY --from=shatterspear-assets /assets/mod-transmog /azerothcore/modules/mod-transmog

ARG CACHEBUST=1

WORKDIR /azerothcore/build

RUN --mount=type=cache,target=/ccache,sharing=locked \
    # This may seem silly (and it is), but AzerothCore wants the git repo at
    # build time. The git repo is _huge_ and it's not something that really
    # makes sense to mount into the container, but this way we can let the build
    # have the information it needs without including the hundreds of megabytes
    # of git repo into the container.
    --mount=type=bind,target=/azerothcore/.git,source=.git \
    git config --global --add safe.directory /azerothcore \
    && cmake /azerothcore \
       -DCMAKE_INSTALL_PREFIX="/azerothcore/env/dist"  \
       -DAPPS_BUILD="all"                              \
       -DTOOLS_BUILD="$CTOOLS_BUILD"                   \
       -DSCRIPTS="$CSCRIPTS"                           \
       -DMODULES="$CMODULES"                           \
       -DWITH_WARNINGS="$CWITH_WARNINGS"               \
       -DCMAKE_BUILD_TYPE="$CTYPE"                     \
       -DCMAKE_CXX_COMPILER="clang++"                  \
       -DCMAKE_C_COMPILER="clang"                      \
       -DCMAKE_CXX_COMPILER_LAUNCHER="ccache"          \
       -DCMAKE_C_COMPILER_LAUNCHER="ccache"            \
       -DBoost_USE_STATIC_LIBS="ON"                    \
    && cmake --build . --config "$CTYPE" -j $(($(nproc) + 1)) \
    && cmake --install . --config "$CTYPE"

#############################
# Base runtime for services #
#############################

FROM skeleton AS runtime

ARG USER_ID=1000
ARG GROUP_ID=1000
ARG DOCKER_USER=acore

ENV ACORE_COMPONENT=undefined

# Install base dependencies for azerothcore
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      libmysqlclient21 libreadline8 libicu74 libncurses5-dev \
      gettext-base default-mysql-client \
      adduser \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /azerothcore/env/dist/etc/ /azerothcore/env/ref/etc
COPY --from=shatterspear-assets /assets/configs /tmp/shatterspear-configs
COPY apps/docker/merge-custom-configs.sh /tmp/merge-custom-configs.sh

RUN bash /tmp/merge-custom-configs.sh /azerothcore/env/ref/etc /tmp/shatterspear-configs \
    && rm -rf /tmp/shatterspear-configs /tmp/merge-custom-configs.sh

ENV PATH="/azerothcore/env/dist/bin:$PATH"

# To use GID/UID 1000 in ubuntu > 23.04 the existing user must be deleted
# See https://bugs.launchpad.net/cloud-images/+bug/2005129
RUN userdel --remove ubuntu \
  && addgroup --gid "$GROUP_ID" "$DOCKER_USER" \
  && adduser --disabled-password --gecos '' --uid "$USER_ID" --gid "$GROUP_ID" "$DOCKER_USER" \
  && passwd -d "$DOCKER_USER" \
  && chown -R "$DOCKER_USER:$DOCKER_USER" /azerothcore \
  && install -d -o "$DOCKER_USER" -g "$DOCKER_USER" \
       /azerothcore/env/dist/etc \
       /azerothcore/env/dist/logs \
       /azerothcore/env/dist/temp

COPY --chown=$USER_ID:$GROUP_ID \
     --chmod=755 \
     apps/docker/entrypoint.sh /azerothcore/entrypoint.sh

USER $DOCKER_USER

ENTRYPOINT ["/usr/bin/env", "bash", "/azerothcore/entrypoint.sh"]

###############
# Auth Server #
###############

FROM runtime AS authserver
LABEL description="AzerothCore Auth Server"

ENV ACORE_COMPONENT=authserver
# Don't run database migrations. We can leave that up to the db-import container
ENV AC_UPDATES_ENABLE_DATABASES=0
# This disables user prompts. The console is still active, however
ENV AC_DISABLE_INTERACTIVE=1
ENV AC_CLOSE_IDLE_CONNECTIONS=0

COPY --chown=$DOCKER_USER:$DOCKER_USER \
     --from=build \
     /azerothcore/env/dist/bin/authserver /azerothcore/env/dist/bin/authserver


CMD ["authserver"]

################
# World Server #
################

FROM runtime AS worldserver

LABEL description="AzerothCore World Server"

ENV ACORE_COMPONENT=worldserver
# Don't run database migrations. We can leave that up to the db-import container
ENV AC_UPDATES_ENABLE_DATABASES=0
# This disables user prompts. The console is still active, however
ENV AC_DISABLE_INTERACTIVE=1
ENV AC_CLOSE_IDLE_CONNECTIONS=0

COPY --chown=$DOCKER_USER:$DOCKER_USER \
     --from=build \
     /azerothcore/env/dist/bin/worldserver /azerothcore/env/dist/bin/worldserver

COPY --chown=$DOCKER_USER:$DOCKER_USER \
     --from=shatterspear-assets \
     /assets/lua_scripts /azerothcore/env/dist/bin/lua_scripts

CMD ["worldserver"]

#############
# DB Import #
#############

FROM runtime AS db-import

LABEL description="AzerothCore Database Import tool"

USER $DOCKER_USER

ENV ACORE_COMPONENT=dbimport

COPY --chown=$DOCKER_USER:$DOCKER_USER \
    data data

COPY --chown=$DOCKER_USER:$DOCKER_USER \
    modules modules

COPY --chown=$DOCKER_USER:$DOCKER_USER \
     --from=shatterspear-assets \
     /assets/mod-ale /azerothcore/modules/mod-ale

COPY --chown=$DOCKER_USER:$DOCKER_USER \
     --from=shatterspear-assets \
     /assets/mod-instance-reset /azerothcore/modules/mod-instance-reset

COPY --chown=$DOCKER_USER:$DOCKER_USER \
     --from=shatterspear-assets \
     /assets/mod-transmog /azerothcore/modules/mod-transmog

COPY --chown=$DOCKER_USER:$DOCKER_USER\
     --from=build \
     /azerothcore/env/dist/bin/dbimport /azerothcore/env/dist/bin/dbimport

CMD [ "/azerothcore/env/dist/bin/dbimport" ]

###############
# Client Data #
###############

FROM skeleton AS client-data

LABEL description="AzerothCore client-data"

ENV DATAPATH=/azerothcore/env/dist/data

RUN apt-get update && \
    apt-get install -y curl unzip adduser && \
    rm -rf /var/lib/apt/lists/*

ARG USER_ID=1000
ARG GROUP_ID=1000
ARG DOCKER_USER=acore

# To use GID/UID 1000 in ubuntu > 23.04 the existing user must be deleted
# See https://bugs.launchpad.net/cloud-images/+bug/2005129
RUN userdel --remove ubuntu \
  && addgroup --gid "$GROUP_ID" "$DOCKER_USER" \
  && adduser --disabled-password --gecos '' --uid "$USER_ID" --gid "$GROUP_ID" "$DOCKER_USER" \
  && passwd -d "$DOCKER_USER" \
  && chown -R "$DOCKER_USER:$DOCKER_USER" /azerothcore

COPY --chown=$DOCKER_USER:$DOCKER_USER apps apps
COPY --chown=$DOCKER_USER:$DOCKER_USER \
     --from=shatterspear-assets \
     /assets/dbc /azerothcore/custom-dbc

VOLUME /azerothcore/env/dist/data

USER $DOCKER_USER

CMD ["bash", "-c", "source /azerothcore/apps/installer/includes/functions.sh && inst_download_client_data && cp -a /azerothcore/custom-dbc/. \"$DATAPATH/dbc/\""]

##################
# Map Extractors #
##################

FROM runtime AS tools

LABEL description="AzerothCore Tools"

WORKDIR /azerothcore/env/dist/

RUN mkdir -pv /azerothcore/env/dist/Cameras \
              /azerothcore/env/dist/dbc     \
              /azerothcore/env/dist/maps    \
              /azerothcore/env/dist/mmaps   \
              /azerothcore/env/dist/vmaps

COPY --chown=$DOCKER_USER:$DOCKER_USER --from=build \
  /azerothcore/env/dist/bin/map_extractor /azerothcore/env/dist/bin/map_extractor

COPY --chown=$DOCKER_USER:$DOCKER_USER --from=build \
  /azerothcore/env/dist/bin/mmaps_generator /azerothcore/env/dist/bin/mmaps_generator

COPY --chown=$DOCKER_USER:$DOCKER_USER --from=build \
  /azerothcore/env/dist/bin/vmap4_assembler /azerothcore/env/dist/bin/vmap4_assembler

COPY --chown=$DOCKER_USER:$DOCKER_USER --from=build \
  /azerothcore/env/dist/bin/vmap4_extractor /azerothcore/env/dist/bin/vmap4_extractor
