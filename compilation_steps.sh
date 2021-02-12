gn gen out/android_arm --args='target_os="android" target_cpu="arm" chip_build_tests=false chip_device_platform="linux" chip_enable_mdns=false chip_enable_ble=false chip_enable_openthread=false chip_enable_wifi=false android_ndk_root="/Users/makdam/Library/Android/android-ndk-r21d" android_sdk_root="/Users/makdam/Library/Android/sdk"'




gn gen out/android_arm --args='target_os="android" target_cpu="arm" chip_build_tests=false chip_device_platform="linux" chip_enable_mdns=false chip_enable_ble=false chip_enable_openthread=false chip_enable_wifi=false android_ndk_root="/Volumes/SSD/workspace/sources/Android/sdk/ndk/22.0.7026061" android_sdk_root="/Volumes/SSD/workspace/sources/Android/sdk/"' ; ninja -C out/android_arm all


gn gen out/android_arm --args='target_os="android" target_cpu="arm" chip_build_tests=false chip_device_platform="linux" chip_enable_mdns=false chip_enable_ble=false chip_enable_openthread=false chip_enable_wifi=false chip_bypass_rendezvous=true android_ndk_root="/Volumes/SSD/workspace/sources/Android/sdk/ndk/22.0.7026061" android_sdk_root="/Volumes/SSD/workspace/sources/Android/sdk/"'
ninja -C out/android_arm all


BatchBuilds
	+ https://acos2014-jenkins.labcollab.net/jenkins/view/FireOS%206.x/view/FireOS%20Nougat%20Mainline/job/fireos_main_nougat-patch-build/116207/