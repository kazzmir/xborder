libs=-lX11

xborder: src/main.cpp
	g++ -g3 $< -o $@ ${libs}

clean:
	rm xborder
