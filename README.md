# ffmpeg-pag-transition

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
