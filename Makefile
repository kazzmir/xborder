libs=-lX11

xborder: src/main.cpp
	clang++ $< -o $@ ${libs} `pkg-config --libs xmu`

clean:
	rm xborder
