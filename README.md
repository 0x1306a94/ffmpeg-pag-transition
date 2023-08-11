# ffmpeg-pag-transition

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
ffmpeg -i /Users/king/Downloads/v0200fg10000cfes7crc77u9c9u9d1b0.MP4 \
-i /Users/king/Downloads/v0d00fg10000ci08dlbc77u2e9kb3jc0.MP4 \
-filter_complex "[0:v][1:v]pagtransition=duration=1:offset=2.0:source=/Users/king/WorkSpace/FFmpeg/ffmpeg-pag-transition/test.pag[s2];[s2]format=yuv420p[outv]" \
-map "[outv]" \
-map 1:a:0 -c:a copy \
-vcodec libx264 \
-movflags faststart -y out.mp4
```
* 制作pag特效文件时需要注意, 滤镜内使用0号可编辑图片图层为from, 1号可编辑图片图层为to.

<video width="640" height="360" controls>
    <source src="https://github.com/0x1306a94/ffmpeg-pag-transition/releases/download/temp_file/out.mp4" type="video/mp4">
</video>
