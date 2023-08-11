# ffmpeg-pag-transition

* 自定义ffmpeg转场滤镜,使用[Tencent/libpag v4.3.3](https://github.com/Tencent/libpag/tree/v4.3.3)渲染
* 参考自[transitive-bullshit/ffmpeg-gl-transition](https://github.com/transitive-bullshit/ffmpeg-gl-transition)
* 基于ffmpeg-5.1.3
* 源码增加以下内容
```bash
# libavfilter/Makefile
# video filters
OBJS-$(CONFIG_PAGTRANSITION_FILTER)          += pag/pag_impl.o vf_pagtransition.o

# libavfilter/allfilters.c
extern const AVFilter ff_vf_pagtransition;
```
* 编译命令增加以下参数
```bash
--enable-filter=pagtransition \
--extra-cflags="-Iprefix/libpag/linux/vendor/pag/include" \
--extra-libs="-Lprefix/libpag/linux/vendor/pag/mac/x64 -lpag"
```
* 命令使用
```bash
# 参数说明
# duration 转场时长
# offset 开始进入转场处理延迟时长
# source pag 特效文件路径
# from pag 特效文件中转场 from image 图层名称, 默认 from
# to pag 特效文件中转场 to image 图层名称， 默认 to
ffmpeg -i v0200fg10000cfes7crc77u9c9u9d1b0.MP4 \
-i v0d00fg10000ci08dlbc77u2e9kb3jc0.MP4 \
-filter_complex "[0:v][1:v]pagtransition=duration=1:offset=2.0:source=./test.pag[s2];[s2]format=yuv420p[outv]" \
-map "[outv]" \
-map 1:a:0 -c:a copy \
-vcodec libx264 \
-movflags faststart \
-y out.mp4
```
* 制作pag特效文件时需要注意, 请将对应的占位图层名称标记简洁明了，然后使用时通过滤镜 from to 参数传递
* 测试效果

https://github.com/0x1306a94/ffmpeg-pag-transition/assets/14822396/f2838e91-2b1c-4904-a548-55ca215e37e5


