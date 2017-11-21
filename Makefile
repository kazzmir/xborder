libs=-lX11

xborder: src/main.cpp
	clang++ $< -o $@ ${libs}

clean:
	rm xborder
