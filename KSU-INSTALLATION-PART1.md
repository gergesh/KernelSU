# Using KernelSU on AVD | 5ec1cff's blog

# Using KernelSU on AVD

Published in2024-01-16|Updated on2024-01-31

|Read count:

# [](#在-AVD-上使用-KernelSU "Using KernelSU on AVD")Using KernelSU on AVD

[I once](https://github.com/5ec1cff/my-notes/blob/8fde03d5031265a8c90df30ed823cc057e1014b2/cuttlefish-on-wsl.md#avd) tried to build a kernel with KernelSU for AVD to facilitate development and testing, but I found that the kernel module was incompatible. The current method is to build the kernel module at the same time, which is slightly more complicated.

## [](#构建内核 "Build the kernel")Build the kernel

[https://source.android.com/docs/setup/build/building-kernels?hl=zh-cn](https://source.android.com/docs/setup/build/building-kernels?hl=zh-cn)

Android 14 AVD kernel version:

plaintext

1  

Linux version 6.1.23-android14-4-00257-g7e35917775b8-ab9964412 (build-user@build-host) (Android (9796371, based on r487747) clang version 17.0.0 (https://android.googlesource.com/toolchain/llvm-project d9f89f4d16663d5012e5c09495f3b30ece3d2362), LLD 17.0.0) #1 SMP PREEMPT Mon Apr 17 20:50:58 UTC 2023  

Follow the instructions in [KernelSU CI](https://github.com/tiann/KernelSU/blob/aef943ebe3be9764ed0aeeb1f4ffb5d38c1d7d22/.github/workflows/gki-kernel.yml#L166) to pull the corresponding version of the source code.

sh

1   
2   
3   
4  

repo init --depth=1 --u https://android.googlesource.com/kernel/manifest -b common-android14-6.1-2023-12 --repo-rev=v2.16   
REMOTE\_BRANCH=$(git ls-remote https://android.googlesource.com/kernel/common android14-6.1-2023-12)   
DEFAULT\_MANIFEST\_PATH=.repo/manifests/default.xml   
repo --trace sync -c -j$( nproc --all) --no-tags  

Once synchronization is complete, you can use bazel to build.

plaintext

1  

tools/bazel build //common:kernel\_x86\_64\_dist  

Product location:`bazel-out/k8-fastbuild/bin/common/kernel_x86_64/bzImage`

## [](#解决模块不兼容 "Resolving module incompatibility")Resolving module incompatibility

The kernel built above is incompatible with the modules in the original AVD image. GKI is supposed to ensure the stability of module interfaces, even though our KMI Generation is different. In short, the corresponding modules need to be built.

The module will be loaded from three locations: ramdisk, /system(\_dlkm)/lib/modules, and /vendor/lib/modules.

### [](#构建 "Build")Build

The dist command executed above has already built system\_dlkm, and the output is located in `bazel-out/k8-fastbuild/bin/common/kernel_x86_64/unstripped/`(there should actually be a dist directory, but I didn't specify it, and I couldn't find the default location).

We also need to build the vendor (and boot) ko files. According to the documentation, `tools/bazel run //common-modules/virtual-device:virtual_device_x86_64_dist`simply execute the command. The output will be displayed in...`out/virtual_device_x86_64/dist/`

### [](#ramdisk "ramdisk")ramdisk

Using magiskboot to check the ramdisk, I found it under lib/modules.

sh

1   
2  

/data/adb/magisk/magiskboot decompress ramdisk.img ramdisk.cpio   
/data/adb/magisk/magiskboot cpio ramdisk.cpio 'ls -r'  

Therefore, after building the module, you only need to add the new ko to cpio.

sh

1   
2   
3  

/data/adb/magisk/magiskboot decompress ramdisk.img ramdisk.cpio   
for f in \*.ko; do /data/adb/magisk/magiskboot cpio ramdisk.cpio "add 0644 lib/modules/ $f  $f " ; done  
 /data/adb/magisk/magiskboot compress=gzip ramdisk.cpio ramdisk.gz  

The ko files needed are roughly these:

sh

1  

cp out/virtual\_device\_x86\_64/dist/{virtio-rng.ko,virtio\_blk.ko,virtio\_console.ko,virtio\_dma\_buf.ko,virtio\_pci.ko,virtio\_pci\_legacy\_dev.ko,virtio\_pci\_modern\_dev.ko,vmw\_vsock\_virtio\_transport.ko} /mnt/d/Documents/tmp/kmods  

### [](#system-vendor "system & vendor")system & vendor

Upon observation, it was found that \`/system/lib/modules/modules.load\` was empty, while \`/vendor/lib/modules\` was present. Therefore, a script was written to parse the required .ko files and search for and copy them from the built .ko files.

py

1   
2   
3   
4   
5   
6   
7   
8   
9   
10   
11   
12   
13   
14   
15   
16   
17   
18   
19   
20   
21   
22   
23   
24   
25   
26   
27   
28   
29   
30   
31   
32   
33   
34   
35   
36   
37   
38   
39   
40   
41   
42   
43   
44   
45   
46   
47   
48   
49   
50   
51   
52   
53   
54   
55   
56   
57   
58   
59   
60   
61   
62   
63   
64   
65   
66   
67  

import os   
import re   
import sys   
import shutil   
from pathlib import Path  

os.system( 'adb.exe pull /vendor/lib/modules/modules.dep /vendor/lib/modules/modules.load .' )   
os.system( 'adb.exe pull /system/lib/modules/modules.dep sys\_modules.dep' )  

with  open ( 'modules.load' , 'r' ) as f:   
    vendor\_modules\_to\_load = re.split( r'\\s+' , f.read().strip())  

vendor\_modules\_to\_load = list ( map ( lambda x: '/vendor/lib/modules/' + x, vendor\_modules\_to\_load))  

to\_load = set (vendor\_modules\_to\_load)   
to\_load.add( '/system/lib/modules/zram.ko' )   
deps = {}  

def  load\_dep ( fn ): global deps with open ( 'modules.dep' , 'r' ) as f: for l in f.readlines():k, v = l.split( ':' )            k = k.strip() v = v.strip() if v == '' : continue             deps\[k\] = re.split( r'\\s+' , v) \# print(k, '->', deps\[k\])  











load\_dep( 'modules.dep' )   
load\_dep( 'sys\_odules.dep' )  

print ( len (to\_load))  

while  True :   
    orig\_size = len (to\_load) for k in list (to\_load): if k in deps: for v in deps\[k\]:                to\_load.add(v) new\_size = len (to\_load) if orig\_size == new\_size: break  








print ( len (to\_load), to\_load)   
paths = {}  

search\_paths = \[ '/home/five\_ec1cff/android-kernel/bazel-out/k8-fastbuild/bin/common/kernel\_x86\_64/unstripped/' , '/home/five\_ec1cff/android-kernel/out/virtual\_device\_x86\_64/dist/' \]  

for mod in to\_load:   
    path = None for search\_path in search\_paths:tp = search\_path + '/' + mod.split( '/' )\[- 1 \] if os.path.exists(tp):path = tp break     paths\[mod\] = path target\_path = '/mnt/d/Documents/tmp/kmods'  









for p in paths: print (p, '<-' , paths\[p\])    pp = Path(target\_path + p) pp.parent.mkdir(parents= True , exist\_ok= True )    shutil.copy(paths\[p\], pp)  






Next, the built .ko file needs to be placed in /system\_dlkm and /vendor. We need to enable the writable system of AVD, which will use overlayfs to provide a writable overlay.

sh

1   
2   
3   
4   
5  

\# Start writable system  
 emulator @API34 -writable-system   
\# Enable writable system  
 adb root   
adb remount \# rw  

The overlay will only take effect after the first use by remounting with adb and restarting. After that, the overlay will only be used if AVD is started with -writable-system specified, otherwise it will not be used.

After that, you can directly write these directories and push the ko file using adb push.

## [](#KernelSU "KernelSU")KernelSU

This also follows the CI approach. Note that building the kernel must use CI commands; otherwise, the KernelSU version cannot be found correctly. It seems the default build method uses sandboxes, causing the path passed to srctree in [the Makefile to be incorrect.](https://github.com/tiann/KernelSU/blob/1e676e5dc2eed4d6db6452d2815f77adb35e37a7/kernel/Makefile#L16)

sh

1  

tools/bazel run --config=fast --config=stamp --lto=thin //common:kernel\_x86\_64\_dist -- --dist\_dir=dist  

After specifying the dist directory, the built kernel (bzImage) and system\_dlkm are both located in `dist`the dist directory.

## [](#启动 "start up")start up

Booting from a pre-built kernel and patched ramdisk:

plaintext

1  

emulator @API34 -kernel D:\\Documents\\tmp\\kmods\\bzImage -no-snapshot-load -show-kernel -writable-system -ramdisk D:\\Documents\\tmp\\kmods\\ramdisk.gz  

[![](/my-blog/images/20240116_01.png)](https://5ec1cff.github.io/my-blog/images/20240116_01.png)

## [](#总结 "Summarize")Summarize

Although KernelSU supports both ARM and x86, automatic builds have historically only supported ARM, so using it on x86 requires manual intervention. Those who insist on using KernelSU on x86, like myself, are probably in the minority.(You might not believe it, but the main developer of ZygiskOnKernelSU, now ZygiskNext, primarily developed and tested this module using Magisk on an AVD. Magisk + ZygiskNext is not recommended.) After all, I don't have a new phone or an M1 or M2, so I can only run an emulator on an x86 PC for research; I apologize for that.In short, as an x86 emulator user, you only have two choices if you want to use KernelSU: either Cuttlefish or AVD, both of which are painful to integrate and use.

KernelSU is currently planning a modular implementation, which, if applicable to AVD, could alleviate this pain point and is worth a try.

Article author: [5ec1cff](https://5ec1cff.github.io/my-blog)

Article link: [https://5ec1cff.github.io/my-blog/2024/01/16/avd-ksu/](https://5ec1cff.github.io/my-blog/2024/01/16/avd-ksu/)

Copyright Notice: All articles on this blog, unless otherwise stated, are licensed under a [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) license. Please indicate that the article is from [5ec1cff's blog](https://5ec1cff.github.io/my-blog) when reprinting !

[](https://www.facebook.com/sharer/sharer.php?u=https%3A%2F%2F5ec1cff.github.io%2Fmy-blog%2F2024%2F01%2F16%2Favd-ksu%2F)[](https://twitter.com/intent/tweet?text=%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%20%7C%205ec1cff's%20blog&url=https%3A%2F%2F5ec1cff.github.io%2Fmy-blog%2F2024%2F01%2F16%2Favd-ksu%2F&via=https%3A%2F%2F5ec1cff.github.io)[

#### Scan with WeChat to share

![Scan me!](data:image/png;base64,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)

In WeChat, tap "Discover" and scan the QR code.

You can share this article to your WeChat Moments by scanning the QR code.





](javascript:)[](https://service.weibo.com/share/share.php?url=https%3A%2F%2F5ec1cff.github.io%2Fmy-blog%2F2024%2F01%2F16%2Favd-ksu%2F&title=%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%20%7C%205ec1cff's%20blog&pic=data%3Aimage%2Fpng%3Bbase64%2CiVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAEXRFWHRTb2Z0d2FyZQBTbmlwYXN0ZV0Xzt0AAAAMSURBVAiZY%2Fj%2F%2Fz8ABf4C%2Fljyaw4AAAAASUVORK5CYII%3D&appkey=)[](http://connect.qq.com/widget/shareqq/index.html?url=https%3A%2F%2F5ec1cff.github.io%2Fmy-blog%2F2024%2F01%2F16%2Favd-ksu%2F&title=%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%20%7C%205ec1cff's%20blog&source=%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%20%7C%205ec1cff's%20blog&desc=%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%E6%9B%BE%E7%BB%8F%E5%B0%9D%E8%AF%95%E8%BF%87%E7%BB%99%20AVD%20%E6%9E%84%E5%BB%BA%E5%B8%A6%E6%9C%89%20KernelSU%20%E7%9A%84%E5%86%85%E6%A0%B8%EF%BC%8C%E6%96%B9%E4%BE%BF%E5%BC%80%E5%8F%91%E6%B5%8B%E8%AF%95%EF%BC%8C%E4%BD%86%E6%98%AF%E5%8F%91%E7%8E%B0%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97%E4%B8%8D%E5%85%BC%E5%AE%B9%EF%BC%8C%E7%8E%B0%E5%9C%A8%E7%9A%84%E6%96%B9%E6%B3%95%E6%98%AF%E5%90%8C%E6%97%B6%E6%9E%84%E5%BB%BA%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97%EF%BC%8C%E7%95%A5%E4%B8%BA%E5%A4%8D%E6%9D%82%E3%80%82%20%E6%9E%84%E5%BB%BA%E5%86%85%E6%A0%B8https%3A%2F%2Fsource.android.com%2Fdocs%2Fsetup%2Fbuild%2Fbuilding-kernels%3Fhl%3Dzh-cn%20Android%2014%20%E7%9A%84%20AVD%20%E5%86%85%E6%A0%B8%E7%89%88%E6%9C%AC%EF%BC%9A%201Linux%20version%206&pics=data%3Aimage%2Fpng%3Bbase64%2CiVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAEXRFWHRTb2Z0d2FyZQBTbmlwYXN0ZV0Xzt0AAAAMSURBVAiZY%2Fj%2F%2Fz8ABf4C%2Fljyaw4AAAAASUVORK5CYII%3D&summary="%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%E6%9B%BE%E7%BB%8F%E5%B0%9D%E8%AF%95%E8%BF%87%E7%BB%99%20AVD%20%E6%9E%84%E5%BB%BA%E5%B8%A6%E6%9C%89%20KernelSU%20%E7%9A%84%E5%86%85%E6%A0%B8%EF%BC%8C%E6%96%B9%E4%BE%BF%E5%BC%80%E5%8F%91%E6%B5%8B%E8%AF%95%EF%BC%8C%E4%BD%86%E6%98%AF%E5%8F%91%E7%8E%B0%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97%E4%B8%8D%E5%85%BC%E5%AE%B9%EF%BC%8C%E7%8E%B0%E5%9C%A8%E7%9A%84%E6%96%B9%E6%B3%95%E6%98%AF%E5%90%8C%E6%97%B6%E6%9E%84%E5%BB%BA%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97%EF%BC%8C%E7%95%A5%E4%B8%BA%E5%A4%8D%E6%9D%82%E3%80%82%20%E6%9E%84%E5%BB%BA%E5%86%85%E6%A0%B8https%3A%2F%2Fsource.android.com%2Fdocs%2Fsetup%2Fbuild%2Fbuilding-kernels%3Fhl%3Dzh-cn%20Android%2014%20%E7%9A%84%20AVD%20%E5%86%85%E6%A0%B8%E7%89%88%E6%9C%AC%EF%BC%9A%201Linux%20version%206")

![avatar](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAEXRFWHRTb2Z0d2FyZQBTbmlwYXN0ZV0Xzt0AAAAMSURBVAiZY/j//z8ABf4C/ljyaw4AAAAASUVORK5CYII=)

5ec1cff

[

article

15

](/my-blog/archives/)[

Label

0

](/my-blog/tags/)[

Classification

0

](/my-blog/categories/)

[Follow Me](https://github.com/5ec1cff)

[](https://github.com/5ec1cff "Github")[](https://t.me/real5ec1cff "Telegram")

Table of contents100

1.  [1\. Using KernelSU on AVD](#%E5%9C%A8-AVD-%E4%B8%8A%E4%BD%BF%E7%94%A8-KernelSU)
    1.  [1.1. Building the kernel](#%E6%9E%84%E5%BB%BA%E5%86%85%E6%A0%B8)
    2.  [1.2. Resolving module incompatibility](#%E8%A7%A3%E5%86%B3%E6%A8%A1%E5%9D%97%E4%B8%8D%E5%85%BC%E5%AE%B9)
        1.  [1.2.1. Construction](#%E6%9E%84%E5%BB%BA)
        2.  [1.2.2. ramdisk](#ramdisk)
        3.  [1.2.3. System & Vendor](#system-vendor)
    3.  [1.3. KernelSU](#KernelSU)
    4.  [1.4. Startup](#%E5%90%AF%E5%8A%A8)
    5.  [1.5. Summary](#%E6%80%BB%E7%BB%93)

Latest articles

[Symbol lookup in ELF dynamic linking](/my-blog/2024/09/01/elf-symbol-lookup/ "Symbol lookup in ELF dynamic linking")2024-09-01

[A record of the investigation process of a random crash of LSPosed](/my-blog/2024/06/27/lsp-crash-analysis/ "A record of the investigation process of a random crash of LSPosed")2024-06-27

[Using hardware breakpoints in ptrace for Android arm64](/my-blog/2024/06/24/android-arm64-hwbkpt/ "Using hardware breakpoints in ptrace for Android arm64")2024-06-24

[Using KernelSU on AVD - Part 2 -](/my-blog/2024/01/31/avd-ksu2/ "Using KernelSU on AVD - Part 2 -")2024-01-31

[Using KernelSU on AVD](/my-blog/2024/01/16/avd-ksu/ "Using KernelSU on AVD")2024-01-16

## Embedded Content
