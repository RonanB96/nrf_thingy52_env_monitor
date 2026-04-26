source modules/zephyr/zephyr-env.sh
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
# Set this once for your machine if not already configured:
# west config --local zephyr.sdk-install-dir <absolute-path-to-zephyr-sdk>
export ZEPHYR_SDK_INSTALL_DIR="$(west config zephyr.sdk-install-dir)"
