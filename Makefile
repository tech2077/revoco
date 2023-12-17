V=1.0
CFLAGS=-Os -DVERSION=\"$(V)\" -Wall -std=c11 $(USER_DEFINES)

ifeq ($(DEBUG),1)
CFLAGS+=-g -fsanitize=address,undefined
LDFLAGS=-fsanitize=address,undefined
else
LDFLAGS=-s
endif

revoco: revoco.o

clean:
	rm -f revoco revoco.o a.out

tag:
	git tag v$(V)

tar:
	git tar-tree v$(V) revoco-$(V) | gzip -9 >revoco-$(V).tar.gz

