all:
	g++ -fopenmp -lGL -lGLU -lglut -O2 main.cpp

debug:
	g++ -lGL -lGLU -lglut -g main.cpp
