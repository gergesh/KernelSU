# Using KernelSU on AVD - Part 2 - | 5ec1cff's blog

# Using KernelSU on AVD - Part 2 -

Published in2024-01-31|Updated on2024-02-17

|Read count:

# [](#编译兼容内核模块的版本 "Compile a version compatible with the kernel module")Compile a version compatible with the kernel module

[The previous article](/my-blog/2024/01/16/avd-ksu/ "Using KernelSU on AVD") successfully compiled a kernel suitable for AVD; however, compiling compatible kernel modules at the same time was too complex. After some exploration, it was discovered that as long as the correct source code is found, it is still possible to compile a kernel compatible with the released AVD image.

## [](#认识版本号 "Understanding version numbers")Understanding version numbers

Returning to /proc/version, the kernel version number provides a lot of information, such as `-g`a 12-digit hexadecimal number following it, representing the commit hash of the compiled kernel's source tree, as well as the compiler version.

plaintext

1  

Linux version 6.1.23-android14-4-00257-g7e35917775b8-ab9964412 (build-user@build-host) (Android (9796371, based on r487747) clang version 17.0.0 (https://android.googlesource.com/toolchain/llvm-project d9f89f4d16663d5012e5c09495f3b30ece3d2362), LLD 17.0.0) #1 SMP PREEMPT Mon Apr 17 20:50:58 UTC 2023  

However, this information alone is not enough to find all the code, because what is actually cloned from a repo is multiple repositories, while the commit hash is only from the common repository.

Note `-ab`that it is followed by a string of numbers, which actually corresponds to [the Android CI](https://ci.android.com/) ID.

sh

1   
2   
3   
4   
5   
6   
7   
8  

\# common/scripts/setlocalversion  
  
\# finally, add the abXXX number if BUILD\_NUMBER is set   
if  test -n " ​​${BUILD\_NUMBER} " ; then  
 	res= " $res -ab ${BUILD\_NUMBER} "   
fi  
  
echo  " $res "  

You can see branches in Android CI.[`aosp_kernel-common-android14-6.1`](https://ci.android.com/builds/branches/aosp_kernel-common-android14-6.1/grid?legacy=1)

It contains several kernel CI builds, among which kernel\_virt\_x86\_64 is the kernel we need. Open any of the artifcat files; the URL will look like this:

plaintext

1  

https://ci.android.com/builds/submitted/11381674/kernel\_virt\_x86\_64/latest  

Replace 11381674 with our kernel's build number 9964412 to find this build.

[https://ci.android.com/builds/submitted/9964412/kernel\_virt\_x86\_64/latest](https://ci.android.com/builds/submitted/9964412/kernel_virt_x86_64/latest)

Downloading bzImage revealed some differences from AVD's kernel-ranchu, but it was still executable. Therefore, we only need to locate the commits of each repository during the build process based on the information output in CI.

## [](#CI-BuildInfo "CI BuildInfo")CI BuildInfo

[https://ci.android.com/builds/submitted/9964412/kernel\_virt\_x86\_64/latest/view/BUILD\_INFO](https://ci.android.com/builds/submitted/9964412/kernel_virt_x86_64/latest/view/BUILD_INFO)

(The original JSON file is attached at the end of the article.)

The parsed\_manifest provides the revisions for all repositories corresponding to the manifest at build time, such as common. You can see that the commit hash is consistent with our kernel.

json

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

{  
    "linkFiles": \[  
        {  
            "dest": ".source\_date\_epoch\_dir",  
            "src": "."  
        }  
    \],  
    "name": "kernel/common",  
    "path": "common",  
    "remote": {  
        "fetch": "https://android.googlesource.com/",  
        "name": "aosp",  
        "review": "https://android.googlesource.com/"  
    },  
    "revision": "7e35917775b8b3e3346a87f294e334e258bf15e6"  
},  

name 是远程仓库的 path ，path 是本地的 path 。由于不知道如何用 repo 处理这些，因此我们手动 cd 到 path 对应的目录，检出对应的 revision 。

sh

1  
2  
3  
4  

cd $PATH  
git fetch aosp $REVISION  
git branch my $REVISION  
git checkout my  

## [](#编译-virtual-device-kernel "Compiling a virtual device kernel")编译 virtual device kernel

build info 还告诉了我们如何构建这种 kernel\_virt :

plaintext

1  

tools/bazel run --verbose\_failures --jobs=%cpu% --make\_jobs=%cpu% --config=android\_ci --profile=%dist\_dir%/logs/command.profile.json //common-modules/virtual-device:virtual\_device\_x86\_64\_dist -- --dist\_dir=%dist\_dir% --flat  

我们应该运行 `//common-modules/virtual-device:virtual_device_x86_64_dist` 。

> 实际上[文档](https://source.android.com/docs/setup/build/building-kernels?hl=zh-cn#building-gki-modules)也有提到，不过我以为只是构建模块。

plaintext

1  

tools/bazel run --config=fast --config=stamp --lto=none //common-modules/virtual-device:virtual\_device\_x86\_64\_dist -- --dist\_dir=virt  

如此一来构建的内核就和已有的模块兼容了，我们也可以顺利地将 KernelSU 集成进去。

[![](/my-blog/images/20240131_01.png)](https://5ec1cff.github.io/my-blog/images/20240131_01.png)

## [](#附录 "appendix")附录

json

json

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
68  
69  
70  
71  
72  
73  
74  
75  
76  
77  
78  
79  
80  
81  
82  
83  
84  
85  
86  
87  
88  
89  
90  
91  
92  
93  
94  
95  
96  
97  
98  
99  
100  
101  
102  
103  
104  
105  
106  
107  
108  
109  
110  
111  
112  
113  
114  
115  
116  
117  
118  
119  
120  
121  
122  
123  
124  
125  
126  
127  
128  
129  
130  
131  
132  
133  
134  
135  
136  
137  
138  
139  
140  
141  
142  
143  
144  
145  
146  
147  
148  
149  
150  
151  
152  
153  
154  
155  
156  
157  
158  
159  
160  
161  
162  
163  
164  
165  
166  
167  
168  
169  
170  
171  
172  
173  
174  
175  
176  
177  
178  
179  
180  
181  
182  
183  
184  
185  
186  
187  
188  
189  
190  
191  
192  
193  
194  
195  
196  
197  
198  
199  
200  
201  
202  
203  
204  
205  
206  
207  
208  
209  
210  
211  
212  
213  
214  
215  
216  
217  
218  
219  
220  
221  
222  
223  
224  
225  
226  
227  
228  
229  
230  
231  
232  
233  
234  
235  
236  
237  
238  
239  
240  
241  
242  
243  
244  
245  
246  
247  
248  
249  
250  
251  
252  
253  
254  
255  
256  
257  
258  
259  
260  
261  
262  
263  
264  
265  
266  
267  
268  
269  
270  
271  
272  
273  
274  
275  
276  
277  
278  
279  
280  
281  
282  
283  
284  
285  
286  
287  
288  
289  
290  
291  
292  
293  
294  
295  
296  
297  
298  
299  
300  
301  
302  
303  
304  
305  
306  
307  
308  
309  
310  
311  
312  
313  
314  
315  
316  
317  
318  
319  
320  
321  
322  
323  
324  
325  
326  
327  
328  
329  
330  
331  
332  
333  
334  
335  
336  
337  
338  
339  
340  
341  
342  
343  
344  
345  
346  
347  
348  
349  
350  
351  
352  
353  
354  
355  
356  
357  
358  
359  
360  
361  
362  
363  
364  
365  
366  
367  
368  
369  
370  
371  
372  
373  
374  
375  
376  
377  
378  
379  
380  
381  
382  
383  
384  
385  
386  
387  
388  
389  
390  
391  
392  
393  
394  
395  
396  
397  
398  
399  
400  
401  
402  
403  
404  
405  
406  
407  
408  
409  
410  
411  
412  
413  
414  
415  
416  
417  
418  
419  
420  
421  
422  
423  
424  
425  
426  
427  
428  
429  
430  
431  
432  
433  
434  
435  
436  
437  
438  
439  
440  
441  
442  
443  
444  
445  
446  
447  
448  
449  
450  
451  
452  
453  
454  
455  
456  
457  
458  
459  
460  
461  
462  
463  
464  
465  
466  
467  
468  
469  
470  
471  
472  
473  
474  
475  
476  
477  
478  
479  
480  
481  
482  
483  
484  
485  
486  
487  
488  
489  
490  
491  
492  
493  
494  

{  
    "best\_target\_priority": 1,  
    "bid": "9964412",  
    "branch": "aosp\_kernel-common-android14-6.1",  
    "branch\_priority": 1,  
    "build\_configs": {},  
    "build\_dependencies": \[\],  
    "build\_type": "submitted",  
    "dependency\_targets": \[\],  
    "device\_dir": "/buildbot/src/android/common-android14-6.1",  
    "docker\_image": null,  
    "enable\_docker": true,  
    "git-pull": null,  
    "git-server": "https://android.googlesource.com",  
    "hostname": "abfarm-2004-0434",  
    "inc-build": false,  
    "java-version": null,  
    "last\_logfile": null,  
    "out\_dir": "/buildbot/src/android/common-android14-6.1/out",  
    "parsed\_manifest": {  
        "parsedDefault": {  
            "remote": "aosp",  
            "revision": "master",  
            "syncJ": "4"  
        },  
        "projects": \[  
            {  
                "linkFiles": \[  
                    {  
                        "dest": "tools/bazel",  
                        "src": "kleaf/bazel.sh"  
                    },  
                    {  
                        "dest": "WORKSPACE",  
                        "src": "kleaf/bazel.WORKSPACE"  
                    },  
                    {  
                        "dest": "build/build.sh",  
                        "src": "build.sh"  
                    },  
                    {  
                        "dest": "build/build\_abi.sh",  
                        "src": "build\_abi.sh"  
                    },  
                    {  
                        "dest": "build/build\_test.sh",  
                        "src": "build\_test.sh"  
                    },  
                    {  
                        "dest": "build/build\_utils.sh",  
                        "src": "build\_utils.sh"  
                    },  
                    {  
                        "dest": "build/config.sh",  
                        "src": "config.sh"  
                    },  
                    {  
                        "dest": "build/envsetup.sh",  
                        "src": "envsetup.sh"  
                    },  
                    {  
                        "dest": "build/\_setup\_env.sh",  
                        "src": "\_setup\_env.sh"  
                    },  
                    {  
                        "dest": "build/multi-switcher.sh",  
                        "src": "multi-switcher.sh"  
                    },  
                    {  
                        "dest": "build/abi",  
                        "src": "abi"  
                    },  
                    {  
                        "dest": "build/static\_analysis",  
                        "src": "static\_analysis"  
                    }  
                \],  
                "name": "kernel/build",  
                "path": "build/kernel",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "b0377a072bb3f78cdacfd6d809914a9d1b0c0148"  
            },  
            {  
                "linkFiles": \[  
                    {  
                        "dest": ".source\_date\_epoch\_dir",  
                        "src": "."  
                    }  
                \],  
                "name": "kernel/common",  
                "path": "common",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "7e35917775b8b3e3346a87f294e334e258bf15e6"  
            },  
            {  
                "name": "kernel/tests",  
                "path": "kernel/tests",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "c90a1c1b226b975cc31e709fa96fc1c6ecdbe272"  
            },  
            {  
                "name": "kernel/configs",  
                "path": "kernel/configs",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "52a7267d6a9f9efabf3cb43839bb5e7f7ff05be3"  
            },  
            {  
                "name": "kernel/common-modules/virtual-device",  
                "path": "common-modules/virtual-device",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "0d03de3246301028775f05ea388c2c444344a268"  
            },  
            {  
                "cloneDepth": "1",  
                "name": "platform/prebuilts/clang/host/linux-x86",  
                "path": "prebuilts/clang/host/linux-x86",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "4f7e5adc160ab726ac5bafb260de98e612904c50"  
            },  
            {  
                "cloneDepth": "1",  
                "name": "platform/prebuilts/gcc/linux-x86/host/x86\_64-linux-glibc2.17-4.8",  
                "path": "prebuilts/gcc/linux-x86/host/x86\_64-linux-glibc2.17-4.8",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "f7b0d5b0ee369864d5ac3e96ae24ec9e2b6a52da"  
            },  
            {  
                "cloneDepth": "1",  
                "name": "platform/prebuilts/build-tools",  
                "path": "prebuilts/build-tools",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "dc92e06585a7647bf739a2309a721b82fcfa01d4"  
            },  
            {  
                "cloneDepth": "1",  
                "name": "platform/prebuilts/clang-tools",  
                "path": "prebuilts/clang-tools",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "5611871963f54c688d3ac49e527aecdef21e8567"  
            },  
            {  
                "cloneDepth": "1",  
                "name": "kernel/prebuilts/build-tools",  
                "path": "prebuilts/kernel-build-tools",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "2597cb1b5525e419b7fa806373be673054a68d29"  
            },  
            {  
                "name": "platform/system/tools/mkbootimg",  
                "path": "tools/mkbootimg",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "2680066d0844544b3e78d6022cd21321d31837c3"  
            },  
            {  
                "cloneDepth": "1",  
                "name": "platform/prebuilts/bazel/linux-x86\_64",  
                "path": "prebuilts/bazel/linux-x86\_64",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "4fdb9395071ff22118311d434d697c2b6fd887b4"  
            },  
            {  
                "cloneDepth": "1",  
                "name": "platform/prebuilts/jdk/jdk11",  
                "path": "prebuilts/jdk/jdk11",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "491e6aa056676f29c4541f71bd738e4e876e4ba2"  
            },  
            {  
                "cloneDepth": "1",  
                "name": "toolchain/prebuilts/ndk/r23",  
                "path": "prebuilts/ndk-r23",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "19ac7e4eded12adb99d4f613490dde6dd0e72664"  
            },  
            {  
                "name": "platform/external/bazel-skylib",  
                "path": "external/bazel-skylib",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "f998e5dc13c03f0eae9e373263d3afff0932c738"  
            },  
            {  
                "name": "platform/build/bazel\_common\_rules",  
                "path": "build/bazel\_common\_rules",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "707b2c5fe3d0d7d934a93e00a8a4062e83557831"  
            },  
            {  
                "name": "platform/external/stardoc",  
                "path": "external/stardoc",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "e83f522ee95419e55d2c5654aa6e0143beeef595"  
            },  
            {  
                "name": "platform/external/python/absl-py",  
                "path": "external/python/absl-py",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/",  
                    "name": "aosp",  
                    "review": "https://android.googlesource.com/"  
                },  
                "revision": "393d0b1e3f0fea3e95944a2fd3282cc9f76d4f14"  
            },  
            {  
                "name": "kernel/manifest",  
                "path": "kernel/manifest",  
                "remote": {  
                    "fetch": "https://android.googlesource.com/"  
                },  
                "revision": "0c7921f01bec06d4957c8b36bb0efa84727c8fd6"  
            }  
        \],  
        "remotes": \[  
            {  
                "fetch": "https://android.googlesource.com/",  
                "name": "aosp",  
                "review": "https://android.googlesource.com/"  
            }  
        \],  
        "superproject": {  
            "name": "kernel/superproject",  
            "remote": {  
                "fetch": "https://android.googlesource.com/",  
                "name": "aosp",  
                "review": "https://android.googlesource.com/"  
            },  
            "revision": "cbe96ab89d71a8b85db7c868325242c36346893f"  
        }  
    },  
    "platform": "linux",  
    "presubmit\_incremental\_build": false,  
    "proof\_build": false,  
    "repo-dict": {  
        "kernel/build": "b0377a072bb3f78cdacfd6d809914a9d1b0c0148",  
        "kernel/common": "7e35917775b8b3e3346a87f294e334e258bf15e6",  
        "kernel/common-modules/virtual-device": "0d03de3246301028775f05ea388c2c444344a268",  
        "kernel/configs": "52a7267d6a9f9efabf3cb43839bb5e7f7ff05be3",  
        "kernel/manifest": "0c7921f01bec06d4957c8b36bb0efa84727c8fd6",  
        "kernel/prebuilts/build-tools": "2597cb1b5525e419b7fa806373be673054a68d29",  
        "kernel/tests": "c90a1c1b226b975cc31e709fa96fc1c6ecdbe272",  
        "platform/build/bazel\_common\_rules": "707b2c5fe3d0d7d934a93e00a8a4062e83557831",  
        "platform/external/bazel-skylib": "f998e5dc13c03f0eae9e373263d3afff0932c738",  
        "platform/external/python/absl-py": "393d0b1e3f0fea3e95944a2fd3282cc9f76d4f14",  
        "platform/external/stardoc": "e83f522ee95419e55d2c5654aa6e0143beeef595",  
        "platform/prebuilts/bazel/linux-x86\_64": "4fdb9395071ff22118311d434d697c2b6fd887b4",  
        "platform/prebuilts/build-tools": "dc92e06585a7647bf739a2309a721b82fcfa01d4",  
        "platform/prebuilts/clang-tools": "5611871963f54c688d3ac49e527aecdef21e8567",  
        "platform/prebuilts/clang/host/linux-x86": "4f7e5adc160ab726ac5bafb260de98e612904c50",  
        "platform/prebuilts/gcc/linux-x86/host/x86\_64-linux-glibc2.17-4.8": "f7b0d5b0ee369864d5ac3e96ae24ec9e2b6a52da",  
        "platform/prebuilts/jdk/jdk11": "491e6aa056676f29c4541f71bd738e4e876e4ba2",  
        "platform/system/tools/mkbootimg": "2680066d0844544b3e78d6022cd21321d31837c3",  
        "toolchain/prebuilts/ndk/r23": "19ac7e4eded12adb99d4f613490dde6dd0e72664"  
    },  
    "repo-init-branch": "common-android14-6.1",  
    "repo-manifest": "kernel/manifest",  
    "repo\_manifest\_file": "default.xml",  
    "reset\_image\_build": false,  
    "rollout": \[\],  
    "src\_ctrl": "repo",  
    "sync\_finish\_time": 1681844162.2685213,  
    "sync\_start\_time": 1681844077.0726051,  
    "sync\_succeed": 1,  
    "target": {  
        "apiary\_target": "kernel\_virt\_x86\_64",  
        "dir\_list": \[  
            "hci\_vhci.ko",  
            "modules.builtin",  
            "usbip-core.ko",  
            "btusb.ko",  
            "gki-info.txt",  
            "manifest\_9964412.xml",  
            "virt\_wifi.ko",  
            "mt76x2u.ko",  
            "mac80211\_hwsim.ko",  
            "virtual\_device\_x86\_64\_modules",  
            "repo.prop",  
            "mt76x0-common.ko",  
            "mt76-usb.ko",  
            "virtio\_pci.ko",  
            "virtio\_balloon.ko",  
            "vmlinux.symvers",  
            "mt76x2-common.ko",  
            "gs\_usb.ko",  
            "mt76x0u.ko",  
            "mt76.ko",  
            "vhci-hcd.ko",  
            "system\_dlkm.modules.load",  
            "virtio\_pci\_modern\_dev.ko",  
            "goldfish\_pipe.ko",  
            "vkms.ko",  
            "virtio\_snd.ko",  
            "btintel.ko",  
            "kernel\_x86\_64\_modules",  
            "modules.builtin.modinfo",  
            "system\_dlkm\_staging\_archive.tar.gz",  
            "virtio\_net.ko",  
            "virtio\_input.ko",  
            "vmlinux",  
            "system\_heap.ko",  
            "boot.img",  
            "vmw\_vsock\_virtio\_transport.ko",  
            "goldfish\_sync.ko",  
            "System.map",  
            "kernel-uapi-headers.tar.gz",  
            "virtio-rng.ko",  
            "mt76x02-usb.ko",  
            "kernel-headers.tar.gz",  
            "test\_meminit.ko",  
            "modules.load",  
            "test\_mappings.zip",  
            "pulse8-cec.ko",  
            "goldfish\_address\_space.ko",  
            "multiple.intoto.jsonl",  
            "initramfs.img",  
            "rtc-test.ko",  
            "dummy-cpufreq.ko",  
            "virtio\_pmem.ko",  
            "virtio\_pci\_legacy\_dev.ko",  
            "virtio\_console.ko",  
            "btrtl.ko",  
            "nd\_virtio.ko",  
            "applied.prop",  
            "boot-img.tar.gz",  
            "goldfish\_battery.ko",  
            "virtio\_blk.ko",  
            "virtio-gpu.ko",  
            "dummy\_hcd.ko",  
            "system\_dlkm.modules.blocklist",  
            "net\_failover.ko",  
            "virtio\_dma\_buf.ko",  
            "mt76x02-lib.ko",  
            "system\_dlkm.img",  
            "bzImage",  
            "failover.ko",  
            "logs/git.log",  
            "logs/execute\_build\_result.textproto",  
            "logs/git\_thread.log",  
            "logs/command.profile.json",  
            "logs/build\_tee\_error.log",  
            "logs/buildbot\_trace.trace",  
            "logs/execute\_build\_config.textproto",  
            "logs/git\_metrics.textproto",  
            "logs/SUCCEEDED",  
            "logs/STARTED",  
            "logs/build.log",  
            "BUILD\_INFO"  
        \],  
        "dist-dir": "/buildbot/dist\_dirs/aosp\_kernel-common-android14-6.1-linux-kernel\_virt\_x86\_64/9964412",  
        "name": "kernel\_virt\_x86\_64",  
        "rules": \[  
            \[  
                "tools/bazel run --verbose\_failures --jobs=80 --make\_jobs=80 --config=android\_ci --profile=/buildbot/dist\_dirs/aosp\_kernel-common-android14-6.1-linux-kernel\_virt\_x86\_64/9964412/logs/command.profile.json //common-modules/virtual-device:virtual\_device\_x86\_64\_dist -- --dist\_dir=/buildbot/dist\_dirs/aosp\_kernel-common-android14-6.1-linux-kernel\_virt\_x86\_64/9964412 --flat",  
                "build.log",  
                true,  
                false  
            \]  
        \],  
        "storage\_path": "/bigstore/android-build/builds/aosp\_kernel-common-android14-6.1-linux-kernel\_virt\_x86\_64/9964412/c6318ad0db1b918a3a7cb10a3a2ef07e68f5615af82c2179b252a7440889de29",  
        "target\_finish\_time": 1681844566.1232665,  
        "target\_start\_time": 1681844163.1656117,  
        "target\_status": 1  
    },  
    "trident\_usage": {  
        "external\_disk\_no\_space": false,  
        "trident\_used": false  
    },  
    "use\_goma": false,  
    "worknode": {  
        "containerId": "L95800000959995395",  
        "creationTimeMillis": "1681839400348",  
        "currentAttempt": {  
            "attemptId": "IsVTpUkjTSk0VVc+HIsF7Q==",  
            "progressMessages": \[  
                {  
                    "displayMessage": "Build 9964412 for node L95800000959995395:N80600001359138780 has been inserted",  
                    "messageString": "Build 9964412 for node L95800000959995395:N80600001359138780 has been inserted",  
                    "timeMillis": "1681839407679"  
                }  
            \],  
            "startTimeMillis": "1681839402883"  
        },  
        "heartbeatTimeMillis": "15724800000",  
        "id": "L95800000959995395:N80600001359138780",  
        "inputEdges": \[  
            {  
                "neighborId": "L95800000959995395:N85500001359138757"  
            }  
        \],  
        "isFinal": false,  
        "lastUpdatedMillis": "1681839407737",  
        "nodeClass": "postsubmit",  
        "retryStatus": {  
            "maximumRetries": 4,  
            "retryCount": 0  
        },  
        "revision": "P9C4ksNppTyoTHpyqwANsQ==",  
        "status": "scheduled",  
        "workExecutorType": "submittedBuild",  
        "workParameters": {  
            "submittedBuild": {  
                "branch": "aosp\_kernel-common-android14-6.1",  
                "branchConfig": {  
                    "sloTier": "bestEffort"  
                },  
                "buildId": "9964412",  
                "gerritPollerTimestamp": "1681839297836",  
                "manuallyTriggered": false,  
                "syncTimeoutSecond": 2700,  
                "target": {  
                    "buildCommands": \[  
                        "tools/bazel run --verbose\_failures --jobs=%cpu% --make\_jobs=%cpu% --config=android\_ci --profile=%dist\_dir%/logs/command.profile.json //common-modules/virtual-device:virtual\_device\_x86\_64\_dist -- --dist\_dir=%dist\_dir% --flat"  
                    \],  
                    "buildPlatform": "linux",  
                    "disabled": false,  
                    "incrementalBuild": false,  
                    "launchcontrolName" :  "kernel\_virt\_x86\_64" , "name" : "kernel\_virt\_x86\_64" , "platformVersion" : "docker" , "priority" : "high" , "product" : "kernel\_virt\_x86\_64" , "target" : "kernel\_virt\_x86\_64" } } } , "workerId" : "buildassembler\_buildnotifier\_workers" } }                                                                                                                                                          

Article author: [5ec1cff](https://5ec1cff.github.io/my-blog)

Article link: [https://5ec1cff.github.io/my-blog/2024/01/31/avd-ksu2/](https://5ec1cff.github.io/my-blog/2024/01/31/avd-ksu2/)

Copyright Notice: All articles on this blog, unless otherwise stated, are licensed under a [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) license. Please indicate that the article is from [5ec1cff's blog](https://5ec1cff.github.io/my-blog) when reprinting !

[](https://www.facebook.com/sharer/sharer.php?u=https%3A%2F%2F5ec1cff.github.io%2Fmy-blog%2F2024%2F01%2F31%2Favd-ksu2%2F)[](https://twitter.com/intent/tweet?text=%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%20-%20%E7%AC%AC%E4%BA%8C%E5%9B%9E%20-%20%7C%205ec1cff's%20blog&url=https%3A%2F%2F5ec1cff.github.io%2Fmy-blog%2F2024%2F01%2F31%2Favd-ksu2%2F&via=https%3A%2F%2F5ec1cff.github.io)[

#### Scan with WeChat to share

![Scan me!](data:image/png;base64,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)

In WeChat, tap "Discover" and scan the QR code.

You can share this article to your WeChat Moments by scanning the QR code.





](javascript:)[](https://service.weibo.com/share/share.php?url=https%3A%2F%2F5ec1cff.github.io%2Fmy-blog%2F2024%2F01%2F31%2Favd-ksu2%2F&title=%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%20-%20%E7%AC%AC%E4%BA%8C%E5%9B%9E%20-%20%7C%205ec1cff's%20blog&pic=data%3Aimage%2Fpng%3Bbase64%2CiVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAEXRFWHRTb2Z0d2FyZQBTbmlwYXN0ZV0Xzt0AAAAMSURBVAiZY%2Fj%2F%2Fz8ABf4C%2Fljyaw4AAAAASUVORK5CYII%3D&appkey=)[](http://connect.qq.com/widget/shareqq/index.html?url=https%3A%2F%2F5ec1cff.github.io%2Fmy-blog%2F2024%2F01%2F31%2Favd-ksu2%2F&title=%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%20-%20%E7%AC%AC%E4%BA%8C%E5%9B%9E%20-%20%7C%205ec1cff's%20blog&source=%E5%9C%A8%20AVD%20%E4%B8%8A%E4%BD%BF%E7%94%A8%20KernelSU%20-%20%E7%AC%AC%E4%BA%8C%E5%9B%9E%20-%20%7C%205ec1cff's%20blog&desc=%E7%BC%96%E8%AF%91%E5%85%BC%E5%AE%B9%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97%E7%9A%84%E7%89%88%E6%9C%AC%E5%89%8D%E7%AF%87%20%E5%B7%B2%E7%BB%8F%E6%88%90%E5%8A%9F%E7%BC%96%E8%AF%91%E5%87%BA%E4%BA%86%E9%80%82%E5%90%88%20avd%20%E4%BD%BF%E7%94%A8%E7%9A%84%E5%86%85%E6%A0%B8%EF%BC%8C%E7%84%B6%E8%80%8C%E5%90%8C%E6%97%B6%E9%9C%80%E8%A6%81%E7%BC%96%E8%AF%91%E5%85%BC%E5%AE%B9%E7%9A%84%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97%EF%BC%8C%E8%BF%87%E4%BA%8E%E5%A4%8D%E6%9D%82%E3%80%82%E7%BB%8F%E8%BF%87%E4%B8%80%E7%95%AA%E6%8E%A2%E7%B4%A2%EF%BC%8C%E5%8F%91%E7%8E%B0%E5%8F%AA%E8%A6%81%E6%89%BE%E5%88%B0%E6%AD%A3%E7%A1%AE%E7%9A%84%E6%BA%90%E7%A0%81%EF%BC%8C%E8%BF%98%E6%98%AF%E8%83%BD%E7%BC%96%E8%AF%91%E5%87%BA%E4%B8%8E%E5%8F%91%E5%B8%83%E7%9A%84%20avd%20%E9%95%9C%E5%83%8F%E5%85%BC%E5%AE%B9%E7%9A%84%E5%86%85%E6%A0%B8%E3%80%82%20%20%E8%AE%A4%E8%AF%86%E7%89%88%E6%9C%AC%E5%8F%B7%E8%BF%98%E6%98%AF%E5%9B%9E%E5%88%B0%20%2Fproc%2Fversion%20%E4%B8%8A%EF%BC%8C%E5%86%85%E6%A0%B8%E7%9A%84%E7%89%88%E6%9C%AC%E5%8F%B7%E6%8F%90%E4%BE%9B%E4%BA%86%E5%BE%88%E5%A4%9A%E4%BF%A1%E6%81%AF%EF%BC%8C%E6%AF%94%E5%A6%82%20-g%20%E5%90%8E%E8%B7%9F%E7%9D%80%E7%9A%84%E6%98%AF%E4%B8%80%E4%B8%AA%2012%20%E4%BD%8D%2016%20%E8%BF%9B%E5%88%B6%E6%95%B0%EF%BC%8C%E8%A1%A8%E7%A4%BA%E7%BC%96%E8%AF%91%E7%9A%84%E5%86%85%E6%A0%B8%E7%9A%84%E6%BA%90%E7%A0%81%E6%A0%91%E7%9A%84%E6%8F%90%E4%BA%A4%20hash%20%EF%BC%8C%E6%AD%A4%E5%A4%96%E8%BF%98&pics=data%3Aimage%2Fpng%3Bbase64%2CiVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAEXRFWHRTb2Z0d2FyZQBTbmlwYXN0ZV0Xzt0AAAAMSURBVAiZY%2Fj%2F%2Fz8ABf4C%2Fljyaw4AAAAASUVORK5CYII%3D&summary="%E7%BC%96%E8%AF%91%E5%85%BC%E5%AE%B9%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97%E7%9A%84%E7%89%88%E6%9C%AC%E5%89%8D%E7%AF%87%20%E5%B7%B2%E7%BB%8F%E6%88%90%E5%8A%9F%E7%BC%96%E8%AF%91%E5%87%BA%E4%BA%86%E9%80%82%E5%90%88%20avd%20%E4%BD%BF%E7%94%A8%E7%9A%84%E5%86%85%E6%A0%B8%EF%BC%8C%E7%84%B6%E8%80%8C%E5%90%8C%E6%97%B6%E9%9C%80%E8%A6%81%E7%BC%96%E8%AF%91%E5%85%BC%E5%AE%B9%E7%9A%84%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97%EF%BC%8C%E8%BF%87%E4%BA%8E%E5%A4%8D%E6%9D%82%E3%80%82%E7%BB%8F%E8%BF%87%E4%B8%80%E7%95%AA%E6%8E%A2%E7%B4%A2%EF%BC%8C%E5%8F%91%E7%8E%B0%E5%8F%AA%E8%A6%81%E6%89%BE%E5%88%B0%E6%AD%A3%E7%A1%AE%E7%9A%84%E6%BA%90%E7%A0%81%EF%BC%8C%E8%BF%98%E6%98%AF%E8%83%BD%E7%BC%96%E8%AF%91%E5%87%BA%E4%B8%8E%E5%8F%91%E5%B8%83%E7%9A%84%20avd%20%E9%95%9C%E5%83%8F%E5%85%BC%E5%AE%B9%E7%9A%84%E5%86%85%E6%A0%B8%E3%80%82%20%20%E8%AE%A4%E8%AF%86%E7%89%88%E6%9C%AC%E5%8F%B7%E8%BF%98%E6%98%AF%E5%9B%9E%E5%88%B0%20%2Fproc%2Fversion%20%E4%B8%8A%EF%BC%8C%E5%86%85%E6%A0%B8%E7%9A%84%E7%89%88%E6%9C%AC%E5%8F%B7%E6%8F%90%E4%BE%9B%E4%BA%86%E5%BE%88%E5%A4%9A%E4%BF%A1%E6%81%AF%EF%BC%8C%E6%AF%94%E5%A6%82%20-g%20%E5%90%8E%E8%B7%9F%E7%9D%80%E7%9A%84%E6%98%AF%E4%B8%80%E4%B8%AA%2012%20%E4%BD%8D%2016%20%E8%BF%9B%E5%88%B6%E6%95%B0%EF%BC%8C%E8%A1%A8%E7%A4%BA%E7%BC%96%E8%AF%91%E7%9A%84%E5%86%85%E6%A0%B8%E7%9A%84%E6%BA%90%E7%A0%81%E6%A0%91%E7%9A%84%E6%8F%90%E4%BA%A4%20hash%20%EF%BC%8C%E6%AD%A4%E5%A4%96%E8%BF%98")

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

Table of contents

1.  [1\. Compile a compatible kernel module version](#%E7%BC%96%E8%AF%91%E5%85%BC%E5%AE%B9%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97%E7%9A%84%E7%89%88%E6%9C%AC)
    1.  [1.1 Understanding Version Numbers](#%E8%AE%A4%E8%AF%86%E7%89%88%E6%9C%AC%E5%8F%B7)
    2.  [1.2. CI BuildInfo](#CI-BuildInfo)
    3.  [1.3. Compiling the virtual device kernel](#%E7%BC%96%E8%AF%91-virtual-device-kernel)
    4.  [1.4. Appendix](#%E9%99%84%E5%BD%95)

Latest articles

[Symbol lookup in ELF dynamic linking](/my-blog/2024/09/01/elf-symbol-lookup/ "Symbol lookup in ELF dynamic linking")2024-09-01

[A record of the investigation process of a random crash of LSPosed](/my-blog/2024/06/27/lsp-crash-analysis/ "A record of the investigation process of a random crash of LSPosed")2024-06-27

[Using hardware breakpoints in ptrace for Android arm64](/my-blog/2024/06/24/android-arm64-hwbkpt/ "Using hardware breakpoints in ptrace for Android arm64")2024-06-24

[Using KernelSU on AVD - Part 2 -](/my-blog/2024/01/31/avd-ksu2/ "Using KernelSU on AVD - Part 2 -")2024-01-31

[Using KernelSU on AVD](/my-blog/2024/01/16/avd-ksu/ "Using KernelSU on AVD")2024-01-16

## Embedded Content