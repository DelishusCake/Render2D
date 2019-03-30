inc_path := include/

bin := game.exe
def := DEBUG
opt := -std=c11 -O3 -msse2 -Wall
lib := glfw3 gdi32 opengl32

$(bin): $(wildcard src/*.c)
	gcc $(opt) $(def:%=-D%) -I$(inc_path) $^ -o $@ $(lib:%=-l%)

clean:
	rm out/*
	rm $(bin)