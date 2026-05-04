source modules/zephyr/zephyr-env.sh
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
if [ -z "$ZEPHYR_SDK_INSTALL_DIR" ]; then
  _sdk=$(find /home -maxdepth 10 -name 'zephyr-sdk' -type d 2>/dev/null | head -1)
  [ -z "$_sdk" ] && { echo "ERROR: Zephyr SDK not found. Set ZEPHYR_SDK_INSTALL_DIR." >&2; return 1; }
  export ZEPHYR_SDK_INSTALL_DIR="$_sdk"
fi
