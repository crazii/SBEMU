#!/bin/bash

git clone --depth 1 --filter=blob:none --no-checkout --sparse https://github.com/torvalds/linux.git alsa && \
cd alsa && \
git sparse-checkout init --no-cone && \
git sparse-checkout set --stdin < ../sparse-checkout-alsa && \
git checkout
