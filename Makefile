CC = cc
LIBS = -lcurl

target = curl-stats

all: ${target}

${target}: ${target}.c
	${CC} ${LIBS} -o $@ $<

.PHONY: clean
clean:
	rm -f ${target}
