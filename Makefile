OUTPUT_DIR:=out
SRC:= src/main.c
OBJ:=$(SRC:.c=.o)
CC:=cc
CFLAGS:= -Wall -Wextra
LIBRARIES:= -lglfw -lvulkan

all: ${OUTPUT_DIR} $(OBJ)
	${CC} ${CFLAGS} ${LIBRARIES} ${OBJ} -o ${OUTPUT_DIR}/hello_vulkan

${OUTPUT_DIR}:
	@mkdir -v ${OUTPUT_DIR}

clean:
	@rm -rfv ${OUTPUT_DIR}
	@rm -rfv ${OBJ}
