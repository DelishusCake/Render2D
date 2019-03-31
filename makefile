src := $(wildcard src/*.c)
out := $(src:src/%.c=out/%.o)

inc := include/

bin := game.exe
def := DEBUG
opt := -std=c11 -c -g -O3 -msse2 -Wall
lib := pthread glfw3 gdi32 opengl32

out/%.o: src/%.c
	gcc $(opt) $(def:%=-D%) $< -o $@ -I$(inc)

$(bin): $(out)
	gcc $^ -o $@ $(lib:%=-l%)

clean:
	rm out/*
	rm $(bin)