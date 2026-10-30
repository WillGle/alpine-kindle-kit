#!/bin/sh
bb=/bin/busybox
connected=/sys/devices/platform/fsl-usb2-udc/gadget/connected
$bb sleep 10
state=$($bb cat "$connected") || { echo GW_USB_STATE_UNREADABLE; exit 1; }
case "$state" in
 1) echo GW_USB_ALREADY_CONNECTED;;
 0)
  echo GW_USB_RETRY_VENDOR_BSESSION
  $bb cat /proc/asession /proc/driver/fsl_otg
  # The vendor device-cable helper, with the inherited kernel reset deadline.
  # Old proc handler cannot accept musl stdio's empty first writev vector.
  printf '5 0\n\000' > /run/gw-usb-off
  $bb dd if=/run/gw-usb-off of=/proc/asession bs=5 count=1 || exit 1
  $bb sleep 1
  printf '5 1\n\000' > /run/gw-usb-on
  $bb dd if=/run/gw-usb-on of=/proc/asession bs=5 count=1 || exit 1
  $bb sleep 10
  $bb cat "$connected" /proc/asession /proc/driver/fsl_otg
 ;;
 *) echo GW_USB_STATE_INVALID; exit 1;;
esac
echo GW_USB_RETRY_COMPLETE
$bb sync
