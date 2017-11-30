libs=-lX11

CXX=clang++

xborder: src/main.cpp
	${CXX} -g3 $< -o $@ ${libs}

clean:
	rm xborder
