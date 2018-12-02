CC = cc
CFLAGS = -W -Wall -Wextra -Werror -O
LIBS = -lcurl

target = curl-stats

.PHONY: all
all: ${target}

${target}: ${target}.c
	${CC} ${CFLAGS} ${LIBS} -o $@ $<

.PHONY: clean
clean:
	rm -f ${target}
