.PHONY: xborder

libs=-lX11

CXX=clang++

xborder:
	go build -o xborder ./main

xborder.legacy: src/main.cpp
	${CXX} -g3 $< -o $@ ${libs}

clean:
	rm xborder
