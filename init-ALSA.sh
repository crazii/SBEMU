#!/bin/bash

git clone --depth 1 --filter=blob:none --sparse --no-checkout https://github.com/torvalds/linux.git ALSA
cd ALSA
git sparse-checkout init --no-cone
git sparse-checkout set --stdin < ../sparse-checkout-ALSA
git checkout
